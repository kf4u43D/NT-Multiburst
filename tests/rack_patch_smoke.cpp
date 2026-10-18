// Exercise the saved patch with the installed Rack/Fundamental/nt_emu DLLs.
// The test does not open a GUI/audio device or write Rack's user profile.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <context.hpp>
#include <engine/Engine.hpp>
#include <plugin/Plugin.hpp>
#include <plugin/Model.hpp>
#include <plugin.hpp>
#include <widget/event.hpp>
#include <history.hpp>
#include <asset.hpp>
#include <logger.hpp>
#include <settings.hpp>
#include <jansson.h>
#include "multi_burst/parameters.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace rack;
namespace {
constexpr int sampleRate = 48000;
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::string field(json_t* object, const char* key) {
    const char* value = json_string_value(json_object_get(object, key));
    return value ? value : "";
}
int64_t integer(json_t* object, const char* key) {
    json_t* value = json_object_get(object, key);
    require(json_is_integer(value), std::string("Missing integer field: ") + key);
    return json_integer_value(value);
}
json_t* readJson(const std::string& path) {
    json_error_t error{};
    json_t* value = json_load_file(path.c_str(), JSON_REJECT_DUPLICATES, &error);
    require(value != nullptr, path + ": " + error.text);
    return value;
}
template <typename Function>
Function symbol(HMODULE library, const char* name) {
    FARPROC exported = GetProcAddress(library, name);
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(exported), "Function pointer size");
    std::memcpy(&function, &exported, sizeof(function));
    require(function != nullptr, std::string("Missing DLL export: ") + name);
    return function;
}
plugin::Plugin* loadPlugin(const std::string& directory) {
    HMODULE library = LoadLibraryA((directory + "/plugin.dll").c_str());
    require(library != nullptr, "Cannot load " + directory + "/plugin.dll (Windows error " +
            std::to_string(GetLastError()) + ")");
    auto* result = new plugin::Plugin;
    result->path = directory;
    result->handle = library;
    json_t* manifest = readJson(directory + "/plugin.json");
    result->fromJson(manifest);
    symbol<void (*)(plugin::Plugin*)>(library, "init")(result);
    result->modulesFromJson(json_object_get(manifest, "modules"));
    plugin::plugins.push_back(result);
    std::printf("Installed plugin: %s %s\n", field(manifest, "slug").c_str(), field(manifest, "version").c_str());
    json_decref(manifest);
    return result;
}
struct Meter {
    bool high = false;
    int rises = 0;
    int width = 0;
    std::vector<int64_t> starts;
    std::vector<int> widths;
    void add(float voltage, int64_t frame) {
        require(std::isfinite(voltage) && (std::fabs(voltage) < .00001f ||
                std::fabs(voltage - 5.f) < .00001f), "Digital output is not 0/5 V");
        bool now = voltage > 2.5f;
        if (now && !high) { ++rises; starts.push_back(frame); width = 0; }
        if (now) ++width;
        if (high && !now) widths.push_back(width);
        high = now;
    }
};
struct Capture {
    std::array<Meter, 4> digital; // Gate, Active, EOC, Clock.
    std::vector<float> peaks;
    std::vector<int64_t> cvClockStarts;
    bool cvClockHigh = false;
    double audioSquare = 0;
    float audioPeak = 0;
    float secondPeak = 0, mixPeak = 0;
    int frames = 0;
};
class Patch {
    struct Cable { engine::Module* from; int output; engine::Module* to; int input; };
    std::map<int64_t, std::unique_ptr<engine::Module>> modules;
    std::map<int64_t, json_t*> entries;
    std::vector<Cable> cables;
    engine::Module* nt = nullptr;
    engine::Module* pulses = nullptr;
    engine::Module* audioSource = nullptr;
    int audioOutput = -1;
    json_t* ntEntry = nullptr;
    int64_t frame = 0;
    using SetParameter = void (*)(uint32_t, uint32_t, int16_t);
    SetParameter setParameter = nullptr;
    bool motionProfile = false;
    int packSlot = -1;
    std::vector<int> packValues;
public:
    Patch(json_t* patch, plugin::Plugin* emulator, plugin::Plugin* fundamental) {
        require(json_is_array(json_object_get(patch, "modules")) &&
                json_is_array(json_object_get(patch, "cables")), "Missing patch graph");
        size_t index;
        json_t* entry;
        int scopes = 0;
        json_array_foreach(json_object_get(patch, "modules"), index, entry) {
            const int64_t id = integer(entry, "id");
            require(entries.emplace(id, entry).second, "Duplicate module ID");
            const std::string owner = field(entry, "plugin"), modelName = field(entry, "model");
            if (owner == "Core") {
                require(modelName == "Notes" || modelName == "AudioInterface2", "Unsupported Core module: " + modelName);
                continue;
            }
            require(owner == "nt_emu" || owner == "Fundamental", "Unexpected plugin: " + owner);
            auto* model = (owner == "nt_emu" ? emulator : fundamental)->getModel(modelName);
            require(model != nullptr, "Missing installed Rack model: " + modelName);
            auto* module = model->createModule();
            modules[id].reset(module);
            module->fromJson(entry);
            if (modelName == "Scope") ++scopes;
            if (owner == "nt_emu") {
                require(!nt, "Multiple nt_emu modules");
                nt = module; ntEntry = entry;
                json_t* slot = json_object_get(json_object_get(entry, "data"), "presetPackSlot");
                if (slot) {
                    require(json_is_integer(slot) && json_integer_value(slot) >= 0 && json_integer_value(slot) < 16,
                            "Invalid preset pack slot marker");
                    packSlot = int(json_integer_value(slot));
                }
            }
            if (modelName == "Pulses") {
                require(!pulses, "Multiple manual Pulses modules");
                pulses = module;
            }
        }
        require(nt && pulses && pulses->params.size() >= 2, "Missing emulator/manual controls");
        require(scopes >= 3, "Missing diagnostic scopes");
        std::set<int64_t> cableIds;
        std::set<std::pair<int64_t, int>> destinations;
        std::set<int> ntOutputs;
        int triggerWires = 0, resetWires = 0, clockWires = 0;
        json_array_foreach(json_object_get(patch, "cables"), index, entry) {
            require(cableIds.insert(integer(entry, "id")).second, "Duplicate cable ID");
            const int64_t from = integer(entry, "outputModuleId"), to = integer(entry, "inputModuleId");
            const int output = int(integer(entry, "outputId")), input = int(integer(entry, "inputId"));
            require(entries.count(from) && entries.count(to), "Cable references missing module");
            require(output >= 0 && input >= 0 && modules.count(from) &&
                    output < int(modules.at(from)->outputs.size()), "Invalid source port");
            require(destinations.emplace(to, input).second, "Multiple cables target one input");
            auto* source = modules.at(from).get();
            source->outputs[output].channels = 1;
            if (source == nt) ntOutputs.insert(output);
            if (!modules.count(to)) {
                require(field(entries.at(to), "model") == "AudioInterface2" && input < 2, "Invalid Core destination");
                if (!audioSource) { audioSource = source; audioOutput = output; }
                require(source == audioSource && output == audioOutput, "Expected mono audition on both audio channels");
                continue;
            }
            auto* destination = modules.at(to).get();
            require(input < int(destination->inputs.size()), "Invalid destination port");
            destination->inputs[input].channels = 1;
            cables.push_back({source, output, destination, input});
            if (source == pulses && output == 0 && destination == nt && input == 0) ++triggerWires;
            if (source == pulses && output == 1 && ((destination == nt && input == 2) ||
                    (field(entries.at(to), "model") == "LFO" && input == 2))) ++resetWires;
            if (field(entries.at(from), "model") == "LFO" && output == 3 && destination == nt && input == 1) ++clockWires;
        }
        for (int output = packSlot >= 0 ? 1 : 0; output < 5; ++output)
            require(ntOutputs.count(output), "Required NT output is not wired");
        if (packSlot >= 0) {
            require(ntOutputs.count(5) && ntOutputs.count(7), "Pack patch must wire G2 and the envelope mix");
            // Probe the rendered G1 signal even when the visible scope shows Mix.
            nt->outputs[0].channels = 1;
        }
        require(triggerWires == 1 && resetWires == 2 && clockWires == 1, "Trigger/reset/CV clock routing differs from test patch");
        require(audioSource != nullptr, "Missing audio audition routing");
        validateState();
        setParameter = symbol<SetParameter>(static_cast<HMODULE>(nt->model->plugin->handle), "NT_setParameterFromUi");
        std::printf("Patch graph: %zu DSP modules, %zu internal cables, %d scopes, %zu wired NT outputs: PASS\n",
                    modules.size(), cables.size(), scopes, ntOutputs.size());
    }
    void validateState() {
        json_t* wanted = json_object_get(ntEntry, "data");
        json_t* saved = nt->dataToJson();
        require(field(saved, "pluginPath") == field(wanted, "pluginPath"), "Native DLL path was not restored");
        json_t* before = json_object_get(json_object_get(wanted, "parameterValues"), "routingMatrix");
        json_t* after = json_object_get(json_object_get(saved, "parameterValues"), "routingMatrix");
        motionProfile = packSlot < 0 && json_integer_value(json_array_get(before, multi_burst::CvResponse)) == 1;
        require(json_array_size(before) >= multi_burst::ParameterCount && json_array_size(after) >= multi_burst::ParameterCount,
                "The native DLL must restore all Multi Burst parameters");
        // nt_emu serializes its complete 256-value host matrix, including the
        // unused tail after the algorithm's persisted parameters.
        for (int i = 0; i < multi_burst::ParameterCount; ++i)
            require(json_equal(json_array_get(before, i), json_array_get(after, i)),
                    "Saved Multi Burst parameter " + std::to_string(i) + " changed during load/save");
        if (packSlot >= 0) {
            packValues.clear();
            for (int i = 0; i < multi_burst::ParameterCount; ++i)
                packValues.push_back(int(json_integer_value(json_array_get(before, i))));
            require(packValues[multi_burst::PackPreset] == packSlot &&
                    packValues[multi_burst::LoadPackPreset] == 0 &&
                    packValues[multi_burst::Generator2Enabled] == 1 &&
                    packValues[multi_burst::MixOutput] == 20 &&
                    packValues[multi_burst::G2EnvelopeOutput] == 18,
                    "Pack selection, idle action or two-voice output routing was not restored");
        } else if (motionProfile) {
            require(json_integer_value(json_array_get(before, multi_burst::Generator2Enabled)) == 1 &&
                    json_integer_value(json_array_get(before, multi_burst::ClockSource)) == 4 &&
                    json_integer_value(json_array_get(before, multi_burst::TimingMode)) == 0 &&
                    json_integer_value(json_array_get(before, multi_burst::G2TimingMode)) == 0 &&
                    json_integer_value(json_array_get(before, multi_burst::ReferenceCurves)) == 2,
                    "Motion patch must use two free-timing voices, combined clocks and morph");
        } else require(json_integer_value(json_array_get(before, multi_burst::Bursts)) == 8 &&
                json_integer_value(json_array_get(before, multi_burst::ClockSource)) == 2 &&
                json_integer_value(json_array_get(before, multi_burst::Width)) == 100 &&
                json_integer_value(json_array_get(before, multi_burst::Slope)) == 100 &&
                json_integer_value(json_array_get(before, multi_burst::Level)) == 80,
                "Test patch must use 8 bursts, CV clock, 100 ms width, crescendo +100%, level 8 V");
        json_decref(saved);
        std::printf("Native DLL and all %d saved parameters restored: PASS\n", multi_burst::ParameterCount);
    }
    void process(int count, Capture* capture = nullptr, bool silence = false) {
        engine::Module::ProcessArgs args{};
        args.sampleRate = sampleRate;
        args.sampleTime = 1.f / sampleRate;
        for (int i = 0; i < count; ++i, ++frame) {
            args.frame = frame;
            for (const auto& cable : cables)
                cable.to->inputs[cable.input].setVoltage(cable.from->outputs[cable.output].getVoltage());
            if (capture) {
                const float cv = nt->inputs[1].getVoltage();
                require(std::isfinite(cv), "Non-finite CV clock input");
                if (!capture->cvClockHigh && cv >= 1.f) {
                    capture->cvClockStarts.push_back(frame);
                    capture->cvClockHigh = true;
                } else if (cv <= .2f) capture->cvClockHigh = false;
            }
            for (auto& module : modules) module.second->process(args);
            const float env = nt->outputs[0].getVoltage();
            require(std::isfinite(env) && env >= -.00001f && env <= (motionProfile || packSlot >= 0 ? 10.01f : 8.01f), "Invalid envelope voltage");
            if (packSlot >= 0) {
                const float second = nt->outputs[5].getVoltage(), mix = nt->outputs[7].getVoltage();
                require(std::isfinite(second) && second >= -.00001f && second <= 10.01f,
                        "Invalid G2 envelope voltage in preset pack");
                require(std::isfinite(mix) && mix >= -.00001f && mix <= 10.01f &&
                        std::fabs(mix - std::min(10.f, env + second)) < .001f,
                        "Preset pack mix must equal the bounded sum of both envelopes");
                if (silence) require(std::fabs(second) < .00001f && std::fabs(mix) < .00001f,
                                     "Reset/Stop left a preset pack envelope sounding");
                if (capture) {
                    capture->secondPeak = std::max(capture->secondPeak, second);
                    capture->mixPeak = std::max(capture->mixPeak, mix);
                }
            }
            if (silence) for (int output = 0; output < 4; ++output)
                require(std::fabs(nt->outputs[output].getVoltage()) < .00001f, "Reset emitted an envelope/gate/active/EOC");
            if (!capture) continue;
            const int previousBursts = capture->digital[0].rises;
            for (int output = 1; output <= 4; ++output)
                capture->digital[output - 1].add(nt->outputs[output].getVoltage(), frame);
            if (capture->digital[0].rises != previousBursts) capture->peaks.push_back(0.f);
            if (capture->digital[0].high && !capture->peaks.empty())
                capture->peaks.back() = std::max(capture->peaks.back(), env);
            const float audio = audioSource->outputs[audioOutput].getVoltage();
            require(std::isfinite(audio) && std::fabs(audio) <= 12.f, "Invalid audition audio");
            capture->audioPeak = std::max(capture->audioPeak, std::fabs(audio));
            capture->audioSquare += double(audio) * audio;
            ++capture->frames;
        }
    }
    void pulse(int parameter, Capture* capture = nullptr) {
        pulses->params.at(parameter).setValue(1.f); process(64, capture);
        pulses->params.at(parameter).setValue(0.f);
    }
    void checkBurstSequence(const char* label, bool external = false) {
        Capture capture;
        capture.cvClockHigh = nt->inputs[1].getVoltage() >= 1.f;
        pulse(0, &capture);
        process(3 * sampleRate, &capture);
        auto& gate = capture.digital[0];
        require(gate.rises == 8 && gate.widths.size() == 8 && capture.peaks.size() == 8, "Expected exactly eight completed bursts");
        for (int width : gate.widths) require(width >= 4799 && width <= 4802, "Gate width differs from 100 ms");
        for (size_t i = 0; i < capture.peaks.size(); ++i) {
            const float expected = 8.f * (.04f + .96f * float(i) / 7.f);
            require(std::fabs(capture.peaks[i] - expected) < .02f, "Crescendo envelope peak differs from expected gain");
        }
        require(capture.digital[1].rises == 1 && !capture.digital[1].high, "Active must cover one completed sequence");
        require(capture.digital[2].rises == 1 && capture.digital[2].widths.size() == 1 &&
                capture.digital[2].widths[0] == 240, "EOC must emit one 5 ms pulse");
        const auto& clocks = capture.digital[3].starts;
        require(clocks.size() >= 10, "Too few output clock pulses");
        int64_t minPeriod = sampleRate, maxPeriod = 0, totalPeriod = 0;
        for (size_t i = 1; i < clocks.size(); ++i) {
            const int64_t interval = clocks[i] - clocks[i - 1];
            minPeriod = std::min(minPeriod, interval); maxPeriod = std::max(maxPeriod, interval);
            totalPeriod += interval;
            require(interval >= 11999 && interval <= 12001,
                    "Clock output differs from 4 Hz: " + std::to_string(interval) + " samples between edges");
        }
        std::printf("Clock periods: %lld..%lld samples, %.6f Hz, %zu measured intervals\n",
                    static_cast<long long>(minPeriod), static_cast<long long>(maxPeriod),
                    double(sampleRate) * (clocks.size() - 1) / totalPeriod, clocks.size() - 1);
        if (external) {
            require(capture.cvClockStarts.size() == clocks.size(), "CV clock input/output edge counts differ");
            int64_t minLag = sampleRate, maxLag = 0;
            for (size_t i = 0; i < clocks.size(); ++i) {
                const int64_t lag = clocks[i] - capture.cvClockStarts[i];
                minLag = std::min(minLag, lag); maxLag = std::max(maxLag, lag);
                require(lag >= 0 && lag <= 8, "Output clock is not synchronized with CV input");
            }
            std::printf("Actual CV input/output synchronization: %zu matching edges, pipeline %lld..%lld samples: PASS\n",
                        clocks.size(), static_cast<long long>(minLag), static_cast<long long>(maxLag));
        }
        const double rms = std::sqrt(capture.audioSquare / capture.frames);
        require(rms > .001, "Envelope/VCA audition path is silent");
        std::printf("%s: eight bursts, 100 ms gates, crescendo %.3f..%.3f V, 4 Hz clock, one EOC, audio RMS %.6f V: PASS\n",
                    label, capture.peaks.front(), capture.peaks.back(), rms);
    }
    void checkPresetRoundTrip() {
        using namespace multi_burst;
        setParameter(0, Bursts, 13);
        setParameter(0, PresetSlot, 4);
        setParameter(0, PresetSave, 1);
        setParameter(0, G2Bursts, 17);
        setParameter(0, G2PresetSlot, 6);
        setParameter(0, G2PresetSave, 1);
        process(64);
        json_t* snapshot = nt->dataToJson();
        const std::string state = field(snapshot, "pluginState");
        json_error_t error{};
        json_t* bank = json_loads(state.c_str(), JSON_REJECT_DUPLICATES, &error);
        require(bank && json_array_size(json_object_get(bank, "multiBurstBanks")) == 54,
                "Native JSON must save two complete 27-value bank records");
        json_decref(bank);

        // Replace both memories, then reload through the real nt_emu parser.
        setParameter(0, Bursts, 2);
        setParameter(0, PresetSave, 1);
        setParameter(0, G2Bursts, 3);
        setParameter(0, G2PresetSave, 1);
        process(64);
        nt->dataFromJson(snapshot);
        process(64);
        setParameter(0, Bursts, 1);
        setParameter(0, G2Bursts, 1);
        setParameter(0, PresetLoad, 1);
        setParameter(0, G2PresetLoad, 1);
        process(64);
        json_t* restored = nt->dataToJson();
        json_t* values = json_object_get(json_object_get(restored, "parameterValues"), "routingMatrix");
        require(json_integer_value(json_array_get(values, Bursts)) == 13 &&
                json_integer_value(json_array_get(values, G2Bursts)) == 17,
                "User banks did not survive the actual nt_emu JSON load/save path");
        json_decref(restored);
        json_decref(snapshot);
        std::puts("Both user banks saved, reloaded and recalled through real nt_emu JSON: PASS");
    }
    void checkMotion() {
        Capture capture;
        pulse(0, &capture);
        float cvMinimum[3] = {10,10,10}, cvMaximum[3] = {-10,-10,-10};
        float secondPeak = 0;
        for (int i = 0; i < 12 * sampleRate; ++i) {
            process(1, &capture);
            for (unsigned j = 0; j < 3; ++j) {
                const float cv = nt->inputs[3+j].getVoltage();
                require(std::isfinite(cv), "Motion CV is not finite");
                cvMinimum[j] = std::min(cvMinimum[j], cv);
                cvMaximum[j] = std::max(cvMaximum[j], cv);
            }
            const float second = nt->outputs[5].getVoltage();
            require(std::isfinite(second) && second >= -.00001f && second <= 10.01f,
                    "G2 envelope exceeds 0..10 V in motion mode");
            secondPeak = std::max(secondPeak, second);
        }
        for (unsigned j = 0; j < 3; ++j)
            require(cvMinimum[j] < -.5f && cvMaximum[j] > .5f,
                    "Each motion input must receive an actual bipolar modulation");
        const auto& widths = capture.digital[0].widths;
        require(widths.size() > 12 && secondPeak > .1f, "Both motion voices must keep producing envelopes");
        const auto bounds = std::minmax_element(widths.begin(), widths.end());
        require(*bounds.second > *bounds.first * 1.5f, "Motion widths did not vary enough");
        const auto& starts = capture.digital[3].starts;
        require(starts.size() > 12, "Motion clock has too few events");
        int64_t minimum = sampleRate, maximum = 0;
        for (size_t i = 1; i < starts.size(); ++i) {
            const int64_t interval = starts[i] - starts[i-1];
            minimum = std::min(minimum, interval); maximum = std::max(maximum, interval);
        }
        require(maximum > minimum * 1.5, "Combined clocks and rate CV remained uniform");
        std::printf("Motion: %zu G1 gate regions, G2 peak %.3f V, gate high spans %d..%d samples, clock gaps %lld..%lld samples: PASS\n",
                    widths.size(), secondPeak, *bounds.first, *bounds.second,
                    static_cast<long long>(minimum), static_cast<long long>(maximum));
        std::puts("Three real bipolar CV inputs and both continuously modulated voices: PASS");
    }
    double packCycle(unsigned voice) const {
        using namespace multi_burst;
        constexpr double ratios[] = {1.0/16,1.0/12,1.0/8,1.0/6,1.0/4,1.0/3,1.0/2,2.0/3,1,1.5,2,3,4,6,8,16};
        auto value = [&](Parameter first, Parameter second) { return packValues[voice ? second : first]; };
        double width = value(Width, G2Width) * .001;
        double interval = 0;
        if (value(TimingMode, G2TimingMode)) {
            const double quarter = 600.0 / packValues[Tempo];
            width = quarter * ratios[value(WidthRatio, G2WidthRatio)];
            const int ratio = value(IntervalRatio, G2IntervalRatio);
            if (ratio) interval = quarter * ratios[ratio - 1];
        } else if (packValues[ClockSource] == 1) {
            interval = 600.0 / packValues[Tempo] * divisionTicks[value(Division, G2Division)] / 24.0;
        } else if (value(Rate, G2Rate) > 0) interval = 100.0 / value(Rate, G2Rate);
        const int count = value(Bursts, G2Bursts);
        require(count <= 1 || interval > 0, "Representative pack scene needs a running clock");
        return (count - 1 + value(FirstBurst, G2FirstBurst)) * interval + width;
    }
    void checkPack() {
        using namespace multi_burst;
        // Loading a scene restores controls, never starts either generator.
        process(sampleRate / 4, nullptr, true);
        const bool manual = !packValues[TimingMode] && !packValues[G2TimingMode] &&
                            packValues[Rate] == 0 && packValues[G2Rate] == 0;
        const int manualBursts = std::max(packValues[Bursts], packValues[G2Bursts]);
        const double cycle = manual ? manualBursts * .5 : std::max(packCycle(0), packCycle(1));
        require(cycle > 0 && cycle < 35, "Representative pack scene cycle is unexpectedly long");
        Capture capture;
        setParameter(0, ManualTriggerAll, 1);
        if (manual) {
            process(sampleRate / 2, &capture);
            require(capture.digital[0].rises == 1 && capture.digital[1].high && capture.digital[2].rises == 0,
                    "Rate Off must finish its first attack and wait for a manual step");
            for (int step = 1; step < manualBursts; ++step) {
                pulse(2, &capture); // Actual PULSES 3 -> physical input 7 -> Step All.
                process(sampleRate / 2, &capture);
            }
            require(capture.digital[0].rises == packValues[Bursts] &&
                    capture.digital[3].rises == manualBursts - 1,
                    "Each wired Step All pulse must advance exactly one attack");
        } else process(int(std::ceil((cycle + .5) * sampleRate)), &capture);
        require(capture.digital[1].rises > 0 && capture.digital[2].rises > 0,
                "Pack scene must become active and finish its first G1 cycle");
        require(!capture.peaks.empty() && capture.secondPeak > .01f && capture.mixPeak > .01f,
                "Both pack voices and their audio mix must produce envelopes");
        const bool looping = packValues[Loop] || packValues[G2Loop];
        if (looping) {
            require(capture.digital[1].high, "Looping scene stopped after its first cycle");
            setParameter(0, ManualStopAll, 1);
            Capture stopped;
            process(64, &stopped);
            require(stopped.digital[2].rises == 0, "Stop All emitted an EOC in the preset pack");
            process(sampleRate / 20, nullptr, true);
        } else {
            require(!capture.digital[1].high && !capture.digital[0].high,
                    "One-shot pack scene remained active after its final tail");
        }
        // Reset during a new cycle clears both voices without generating EOC.
        setParameter(0, ManualTriggerAll, 1); process(64);
        setParameter(0, ManualResetAll, 1);
        Capture reset;
        process(64, &reset);
        require(reset.digital[2].rises == 0, "Reset All emitted an EOC in the preset pack");
        process(sampleRate / 20, nullptr, true);
        const double rms = std::sqrt(capture.audioSquare / capture.frames);
        require(rms > .0001, "Preset pack mix audition path is silent");
        std::printf("Pack %02d: restored, both voices/mix bounded, first cycle %.3f s, %s, Stop/Reset, audio RMS %.6f V: PASS\n",
                    packSlot + 1, cycle, manual ? "wired manual steps complete" : looping ? "loop continues" : "one-shot EOC", rms);
    }
    void run() {
        // Prime routing/pipeline and let the initial 5 ms CV clock pulse end.
        process(512);
        if (packSlot >= 0) { checkPack(); return; }
        if (motionProfile) { checkMotion(); return; }
        checkBurstSequence("Saved patch / external CV sync", true);
        pulse(0); process(1200);
        require(nt->outputs[2].getVoltage() > 2.5f, "Reset test did not start a sequence");
        pulse(1); process(64); process(sampleRate / 2, nullptr, true);
        std::puts("Manual CV reset cancels the sequence without an EOC: PASS");
        setParameter(0, multi_burst::ClockSource, 0);
        process(512); // Let the last external clock pulse finish before capture.
        checkBurstSequence("Internal 4 Hz sync");
        setParameter(0, multi_burst::ClockSource, 2);
        process(64);
        validateState();
        checkPresetRoundTrip();
    }
};
} // namespace
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        require(argc == 5 || (argc == 6 && std::strcmp(argv[5], "--layout-only") == 0),
                "Usage: rack_patch_smoke <test-dir> <Rack-dir> <Rack-plugins-dir> <patch.vcv> [--layout-only]");
        asset::userDir = argv[1]; asset::systemDir = argv[2];
        logger::logPath = std::string(argv[1]) + "/rack-smoke.log";
        logger::init();
        auto* context = new Context;
        contextSet(context);
        context->event = new widget::EventState;
        context->history = new history::State;
        settings::sampleRate = sampleRate;
        context->engine = new engine::Engine;
        context->engine->setSampleRate(sampleRate);
        auto* emulator = loadPlugin(std::string(argv[3]) + "/nt_emu");
        auto* fundamental = loadPlugin(std::string(argv[3]) + "/Fundamental");
        json_t* patch = readJson(argv[4]);
        {
            Patch test(patch, emulator, fundamental);
            if (argc == 5) test.run();
            else std::puts("Installed modules, port graph and complete saved state: PASS");
        }
        json_decref(patch);
        std::puts("ALL REAL RACK / NT_EMU MULTI BURST CHECKS PASSED");
        logger::destroy();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
