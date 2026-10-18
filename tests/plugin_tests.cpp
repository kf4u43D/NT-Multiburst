#include <distingnt/api.h>
#define _DISTINGNT_SERIALISATION_INTERNAL
#include <distingnt/serialisation.h>
#include "multi_burst/parameters.h"
#include "multi_burst/performance_presets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace multi_burst;
constexpr uint32_t parameterOffset = 7;
constexpr int blockCapacity = 64;
constexpr size_t guardCount = 8;
constexpr float bufferGuard = -9182.5f;
constexpr size_t memoryGuard = 32;
const char* currentTest = "host setup";

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL [%s]: %s\n", currentTest, message);
        std::exit(EXIT_FAILURE);
    }
}
bool near(float actual, float expected, float tolerance = 0.001f) {
    return std::fabs(actual - expected) <= tolerance;
}

uint32_t testSampleRate() {
#if defined(_WIN32) && defined(_MSC_VER)
    char* requested = nullptr;
    size_t requestedLength = 0;
    require(_dupenv_s(&requested, &requestedLength, "MULTI_BURST_TEST_SAMPLE_RATE") == 0,
            "sample rate environment is readable");
#else
    const char* requested = std::getenv("MULTI_BURST_TEST_SAMPLE_RATE");
#endif
    if (!requested) return 48000;
    char* end = nullptr;
    const unsigned long rate = std::strtoul(requested, &end, 10);
    require(end && *end == 0 && rate >= 8000 && rate <= 384000 && rate % 4 == 0,
            "sample rate environment must be a multiple of 4 between 8000 and 384000");
#if defined(_WIN32) && defined(_MSC_VER)
    std::free(requested);
#endif
    return static_cast<uint32_t>(rate);
}

uint32_t framesForMilliseconds(uint32_t milliseconds) {
    return ((NT_globals.sampleRate * milliseconds + 999) / 1000 + 3) & ~uint32_t{3};
}

struct MidiEvent {
    uint32_t destination;
    uint8_t status, data1, data2, size;
};
struct ParameterWrite { uint32_t index; int16_t value; };
const _NT_factory* hostFactory = nullptr;
_NT_algorithm* hostAlgorithm = nullptr;
int16_t* hostValues = nullptr;
std::vector<MidiEvent> midiEvents;
std::vector<ParameterWrite> parameterWrites;
// Test-owned scratch; the plugin does not depend on later SDK buffer guarantees.
float hostWorkBuffer[blockCapacity * 12] = {};
int drawCalls = 0;

// Minimal token host for the official streaming JSON contract. No JSON code is
// linked into the firmware; these stubs exercise its actual save/load callbacks.
struct JsonToken {
    enum Kind { Object, Array, Name, Number } kind;
    std::string name;
    int number = 0;
    size_t count = 0, end = 0;
};
struct JsonData {
    std::vector<JsonToken> tokens{{JsonToken::Object, "", 0, 0, 0}};
    std::vector<size_t> parents{0};
    size_t append(JsonToken token) {
        const size_t parent = parents.back();
        if (tokens[parent].kind == JsonToken::Array) ++tokens[parent].count;
        const size_t index = tokens.size();
        token.end = index + 1;
        tokens.push_back(token);
        return index;
    }
};

void hostSetParameter(uint32_t algorithmIndex, uint32_t index, int16_t value) {
    require(algorithmIndex == 0 && hostAlgorithm && hostValues, "setter targets registered algorithm");
    require(index >= parameterOffset && index < parameterOffset + ParameterCount,
            "setter includes common-parameter offset");
    const uint32_t local = index - parameterOffset;
    const _NT_parameter& definition = hostAlgorithm->parameters[local];
    value = static_cast<int16_t>(std::max<int>(definition.min, std::min<int>(definition.max, value)));
    parameterWrites.push_back({index, value});
    hostValues[index] = value;
    if (hostFactory->parameterChanged) hostFactory->parameterChanged(hostAlgorithm, static_cast<int>(local));
}

size_t countMidi(uint8_t statusClass) {
    return static_cast<size_t>(std::count_if(midiEvents.begin(), midiEvents.end(), [statusClass](const MidiEvent& event) {
        return (event.status & 0xF0) == statusClass;
    }));
}
size_t countNoteOns() {
    return static_cast<size_t>(std::count_if(midiEvents.begin(), midiEvents.end(), [](const MidiEvent& event) {
        return (event.status & 0xF0) == 0x90 && event.data2 != 0;
    }));
}
size_t countClockTicks() {
    return static_cast<size_t>(std::count_if(midiEvents.begin(), midiEvents.end(), [](const MidiEvent& event) {
        return event.size == 1 && event.status == 0xF8;
    }));
}
void requireNote(const MidiEvent& event, uint32_t destination, uint8_t status, uint8_t note) {
    require(event.size == 3 && event.destination == destination && event.status == status && event.data1 == note,
            "MIDI note targets its original destination, channel and note");
}
void requireBalancedNotes() {
    std::array<int, 8 * 16 * 128> ownership{};
    for (const MidiEvent& event : midiEvents) {
        const uint8_t type = event.status & 0xF0;
        if (type != 0x90 && type != 0x80) continue;
        require(event.destination < 8 && event.data1 < 128, "MIDI note key is bounded");
        const size_t index = (event.destination * 16 + (event.status & 15)) * 128 + event.data1;
        int& state = ownership[index];
        if (type == 0x90 && event.data2) {
            require(state == 0, "old Note Off precedes repeated Note On");
            state = 1;
        } else {
            require(state == 1, "Note Off owns an outstanding note");
            state = 0;
        }
    }
    require(std::all_of(ownership.begin(), ownership.end(), [](int state) { return state == 0; }),
            "no MIDI notes remain held after completion or panic");
}

struct Fixture {
    _NT_algorithmRequirements requirements{};
    std::vector<std::max_align_t> memory;
    std::array<int16_t, parameterOffset + ParameterCount> values{};
    std::array<float, kNT_lastBus> voltages{};
    std::array<float, kNT_lastBus * blockCapacity + guardCount * 2> buffer{};
    _NT_algorithm* algorithm = nullptr;
    int lastFrames = 0;

    Fixture() {
        require(!hostAlgorithm, "one active host fixture");
        requirements.numParameters = 999;
        requirements.sram = requirements.dram = requirements.dtc = requirements.itc = 999;
        hostFactory->calculateRequirements(requirements, nullptr);
        require(requirements.numParameters == ParameterCount && requirements.dtc >= sizeof(_NT_algorithm),
                "memory requirement declares all parameters and instance storage");
        require(requirements.sram == 0 && requirements.dram == 0 && requirements.itc == 0,
                "unused memory requirements are cleared");
        const size_t bytes = requirements.dtc + memoryGuard * 2;
        memory.resize((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        std::memset(memory.data(), 0xA5, memory.size() * sizeof(std::max_align_t));
        _NT_algorithmMemoryPtrs pointers{};
        pointers.dtc = reinterpret_cast<uint8_t*>(memory.data()) + memoryGuard;
        algorithm = hostFactory->construct(pointers, requirements, nullptr);
        require(algorithm && algorithm->parameters && algorithm->parameterPages, "construction initializes metadata");
        require(reinterpret_cast<uint8_t*>(algorithm) >= pointers.dtc &&
                reinterpret_cast<uint8_t*>(algorithm) < pointers.dtc + requirements.dtc,
                "instance lives inside declared host memory");
        for (size_t p = 0; p < ParameterCount; ++p) values[parameterOffset + p] = algorithm->parameters[p].def;
        algorithm->vIncludingCommon = values.data();
        algorithm->v = values.data() + parameterOffset;
        hostAlgorithm = algorithm;
        hostValues = values.data();
        midiEvents.clear();
        parameterWrites.clear();
        // Most tests use isolated outputs and a stopped internal burst clock.
        set(TriggerInput, 0); set(ClockInput, 0); set(ResetInput, 0);
        for (Parameter p : {EnvelopeOutputMode, GateOutputMode, ActiveOutputMode, EocOutputMode, ClockOutputMode}) set(p, 1);
        set(Rate, 0);
        midiEvents.clear();
        parameterWrites.clear();
        checkMemory();
    }
    ~Fixture() { hostAlgorithm = nullptr; hostValues = nullptr; }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    int value(Parameter p) const { return values[parameterOffset + p]; }
    void set(Parameter p, int valueToSet) {
        NT_setParameterFromUi(0, parameterOffset + p, static_cast<int16_t>(valueToSet));
    }
    void rawSet(Parameter p, int valueToSet) { values[parameterOffset + p] = static_cast<int16_t>(valueToSet); }
    void message(uint8_t status, uint8_t data1, uint8_t data2) {
        hostFactory->midiMessage(algorithm, status, data1, data2);
    }
    void realtime(uint8_t byte) { hostFactory->midiRealtime(algorithm, byte); }
    void button(uint16_t control) {
        _NT_uiData data{};
        data.controls = control;
        hostFactory->customUi(algorithm, data);
    }
    void notes(uint32_t destination = 1) {
        set(MidiDestination, static_cast<int>(destination));
        set(MidiNotes, 1); set(EnvelopeShape, 7); set(Bursts, 1); set(Width, 2500);
    }
    void disablePorts() {
        for (Parameter p : {TriggerInput, ClockInput, ResetInput, EnvelopeOutput, GateOutput, ActiveOutput, EocOutput, ClockOutput}) set(p, 0);
    }
    float* buses() { return buffer.data() + guardCount; }
    float sample(int bus, int frame = 0) const {
        require(bus >= 1 && bus <= kNT_lastBus && frame >= 0 && frame < lastFrames, "test bus coordinate is valid");
        return buffer[guardCount + static_cast<size_t>((bus - 1) * lastFrames + frame)];
    }
    void checkMemory() const {
        const auto* bytes = reinterpret_cast<const uint8_t*>(memory.data());
        for (size_t i = 0; i < memoryGuard; ++i) {
            require(bytes[i] == 0xA5 && bytes[memoryGuard + requirements.dtc + i] == 0xA5,
                    "construction and callbacks stay inside declared memory");
        }
    }
    void block(int frames = 4, float initialBusValue = 0.0f) {
        require(frames > 0 && frames <= blockCapacity && frames % 4 == 0, "test block size follows API");
        buffer.fill(bufferGuard);
        const size_t rendered = static_cast<size_t>(kNT_lastBus * frames);
        std::fill_n(buses(), rendered, initialBusValue);
        for (Parameter p : {TriggerInput, ClockInput, ResetInput, StopInput, StepInput,
                G2TriggerInput, G2ResetInput, G2StopInput, G2StepInput,
                TriggerAllInput, ResetAllInput, StopAllInput, StepAllInput,
                CountCvInput, WidthCvInput, IntervalCvInput, SlopeCvInput, ShapeCvInput, AmplitudeCvInput}) {
            const int bus = value(p);
            if (bus >= 1 && bus <= kNT_lastBus) std::fill_n(buses() + (bus - 1) * frames, frames, voltages[static_cast<size_t>(bus - 1)]);
        }
        hostFactory->step(algorithm, buses(), frames / 4);
        lastFrames = frames;
        for (size_t i = 0; i < guardCount; ++i) require(buffer[i] == bufferGuard, "audio leading guard is intact");
        for (size_t i = guardCount + rendered; i < buffer.size(); ++i) require(buffer[i] == bufferGuard, "audio trailing guard is intact");
        for (size_t i = 0; i < rendered; ++i) require(std::isfinite(buses()[i]), "all rendered bus values are finite");
        checkMemory();
    }
    void run(uint32_t frames) {
        require(frames % 4 == 0, "render duration is a multiple of four");
        while (frames) {
            const int count = static_cast<int>(std::min<uint32_t>(frames, blockCapacity));
            block(count);
            frames -= static_cast<uint32_t>(count);
        }
    }
    void trigger() { set(ManualTrigger, 1); block(); }
    void reset() { set(ManualReset, 1); block(); }
};

void metadataAndConstruction() {
    require(kNT_apiVersionCurrent == kNT_apiVersion13, "release build uses the pinned API 13 SDK");
    const auto requestedApi = pluginEntry(kNT_selector_version, 0);
    require(requestedApi == kNT_apiVersion13, "API 13 firmware must accept the hardware plugin");
    require(requestedApi <= 13, "stable firmware loader rejects newer API requirements");
    require(hostFactory->parameterString != nullptr, "API 12/13 parameter strings remain available");
    require(pluginEntry(kNT_selector_numFactories, 0) == 1, "plugin exposes one factory");
    require(pluginEntry(kNT_selector_factoryInfo, 1) == 0 && pluginEntry(kNT_selector_factoryInfo, 99) == 0,
            "invalid factory indices are rejected");
    require(hostFactory->name && hostFactory->guid && hostFactory->numSpecifications == 0,
            "factory exposes name, GUID and no undeclared specifications");
    Fixture fixture;
    std::array<unsigned, ParameterCount> pageReferences{};
    const _NT_parameterPages& groups = *fixture.algorithm->parameterPages;
    require(groups.numPages == 19 && groups.pages, "all dual-generator and performance-pack pages are available");
    require(Bursts == 0 && ManualStep == 41 && Generator2Enabled == 42,
            "the original 42 persisted parameter indices are unchanged");
    require(fixture.value(Generator2Enabled) == 0 && fixture.value(TimingMode) == 0,
            "old patches retain one generator with absolute timing");
    require(CvResponse==166&&PackPreset==167&&LoadPackPreset==168&&fixture.value(CvResponse)==0&&fixture.value(ReferenceCurves)==0,
            "new modulation controls preserve the previous parameter indices and default responses");
    for (uint32_t i = 0; i < groups.numPages; ++i) {
        const _NT_parameterPage& page = groups.pages[i];
        require(page.name && page.numParams && page.params, "page metadata is initialized");
        for (uint32_t p = 0; p < page.numParams; ++p) {
            require(page.params[p] < ParameterCount, "page parameter index stays in range");
            ++pageReferences[page.params[p]];
        }
    }
    for (size_t p = 0; p < ParameterCount; ++p) {
        const _NT_parameter& definition = fixture.algorithm->parameters[p];
        require(definition.name && definition.min <= definition.def && definition.def <= definition.max,
                "parameter definition has valid name, bounds and default");
        require(pageReferences[p] == 1, "every persisted parameter appears on exactly one page");
        if (definition.unit == kNT_unitEnum) {
            require(definition.enumStrings, "enum parameter has labels");
            for (int i = definition.min; i <= definition.max; ++i) require(definition.enumStrings[i - definition.min], "all enum values have labels");
        }
    }
    for (Parameter p : {TriggerInput, ClockInput, ResetInput, EnvelopeOutput, GateOutput, ActiveOutput, EocOutput, ClockOutput}) {
        require(fixture.algorithm->parameters[p].min == 0 && fixture.algorithm->parameters[p].max == kNT_lastBus,
                "optional ports allow None and highest bus");
    }
    fixture.block(blockCapacity);
    require(near(fixture.sample(13), 0) && near(fixture.sample(14), 0) && near(fixture.sample(15), 0),
            "dirty host memory does not create envelopes or active gates");
    drawCalls = 0;
    hostFactory->draw(fixture.algorithm);
    require(drawCalls > 0, "draw callback operates after construction");
    require((hostFactory->hasCustomUi(fixture.algorithm) & (kNT_potButtonL | kNT_potButtonC | kNT_potButtonR))
            == (kNT_potButtonL | kNT_potButtonC | kNT_potButtonR),
            "custom UI retains the three original action buttons");
    char text[32] = {};
    require(hostFactory->parameterString(fixture.algorithm, MidiInputChannel, 17, text) == 4 && std::strcmp(text, "Omni") == 0,
            "custom parameter string labels Omni");
}

void optionalPortsAndBufferBounds() {
    Fixture fixture;
    fixture.disablePorts();
    fixture.set(EnvelopeShape, 7);
    fixture.set(ManualTrigger, 1);
    fixture.block(blockCapacity, 1.25f);
    for (int bus = 1; bus <= kNT_lastBus; ++bus) {
        for (int frame = 0; frame < blockCapacity; ++frame) require(near(fixture.sample(bus, frame), 1.25f), "None ports leave all buses untouched");
    }
    require(midiEvents.empty(), "disabled MIDI emits no messages");
    fixture.rawSet(EnvelopeOutput, kNT_lastBus + 1);
    fixture.rawSet(TriggerInput, -1);
    fixture.block(4, 1.25f);
    for (int bus = 1; bus <= kNT_lastBus; ++bus) require(near(fixture.sample(bus), 1.25f), "invalid bus numbers are ignored safely");
    fixture.set(EnvelopeOutput, kNT_lastBus);
    fixture.set(Level, 100);
    fixture.block();
    require(near(fixture.sample(kNT_lastBus), 10.0f), "highest logical output bus receives envelope volts");
    fixture.reset();
    fixture.set(EnvelopeOutput, 13);
    fixture.set(TriggerInput, kNT_lastBus);
    fixture.voltages[kNT_lastBus - 1] = 0;
    fixture.block();
    fixture.voltages[kNT_lastBus - 1] = 2;
    fixture.block();
    require(near(fixture.sample(13), 10.0f), "highest logical input bus triggers normally");
}

void additiveAndReplaceOutputs() {
    Fixture fixture;
    fixture.set(EnvelopeShape, 7);
    for (Parameter p : {EnvelopeOutputMode, GateOutputMode, ActiveOutputMode, EocOutputMode, ClockOutputMode}) fixture.set(p, 0);
    fixture.set(ManualTrigger, 1);
    fixture.block(4, 2.0f);
    require(near(fixture.sample(13), 10.0f), "Add output preserves existing bus contribution");
    require(fixture.sample(14) > 2 && fixture.sample(15) > 2, "Add mode adds gates and active signal");
    require(near(fixture.sample(16), 2.0f), "inactive EOC contributes zero in Add mode");
    for (Parameter p : {EnvelopeOutputMode, GateOutputMode, ActiveOutputMode, EocOutputMode, ClockOutputMode}) fixture.set(p, 1);
    fixture.set(ManualTrigger, 1);
    fixture.block(4, 2.0f);
    require(near(fixture.sample(13), 8.0f), "Replace output replaces previous bus content");
    require(near(fixture.sample(16), 0.0f), "inactive EOC clears Replace bus");
}

void schmittAndRerouting() {
    Fixture fixture;
    fixture.notes();
    fixture.set(TriggerInput, 1);
    fixture.voltages[0] = 2;
    fixture.block();
    require(countNoteOns() == 0, "initially held-high routed input does not create a fresh edge");
    fixture.voltages[0] = 0; fixture.block();
    fixture.voltages[0] = 0.8f; fixture.block();
    require(countNoteOns() == 0, "sub-threshold noise does not trigger");
    fixture.voltages[0] = 1.01f; fixture.block();
    require(countNoteOns() == 1, "rising threshold crossing triggers once");
    for (float voltage : {0.95f, 0.21f, 0.999f, 1.1f}) { fixture.voltages[0] = voltage; fixture.block(); }
    require(countNoteOns() == 1, "Schmitt hysteresis prevents held-high retriggers");
    fixture.set(TriggerInput, 2);
    fixture.voltages[1] = 2; fixture.block();
    fixture.voltages[1] = 0.9f; fixture.block();
    fixture.voltages[1] = 1.2f; fixture.block();
    require(countNoteOns() == 1, "rerouting to a held-high bus does not retrigger");
    fixture.voltages[1] = 0.19f; fixture.block();
    fixture.voltages[1] = 2; fixture.block();
    require(countNoteOns() == 2, "fresh low-to-high edge rearms the new route");
    fixture.reset();
    requireBalancedNotes();
}

void midiFilteringAndVelocity() {
    Fixture fixture;
    fixture.set(EnvelopeShape, 7); fixture.set(Bursts, 1); fixture.set(Width, 2500);
    fixture.set(MidiInputChannel, 2);
    fixture.message(0x90, 60, 127); fixture.block();
    require(near(fixture.sample(13), 0), "wrong MIDI channel is ignored");
    fixture.message(0x91, 60, 64); fixture.block();
    require(near(fixture.sample(13), 8.0f * 64.0f / 127.0f), "MIDI velocity scales envelope amplitude");
    fixture.reset();
    fixture.set(MidiInputChannel, 17);
    fixture.message(0x9F, 60, 127); fixture.block();
    require(near(fixture.sample(13), 8), "Omni accepts channel sixteen");
    fixture.reset(); fixture.set(MidiInputChannel, 0);
    fixture.message(0x90, 60, 127); fixture.block();
    require(near(fixture.sample(13), 0), "MIDI input Off ignores note triggers");
    fixture.set(MidiInputChannel, 1);
    fixture.message(0x90, 60, 0); fixture.message(0x80, 60, 127); fixture.block();
    require(near(fixture.sample(13), 0), "velocity-zero Note On and Note Off do not trigger");
    fixture.message(0x90, 60, 127); fixture.message(0x90, 61, 127); fixture.block();
    require(near(fixture.sample(13), 0) && near(fixture.sample(15), 0), "MIDI reset note wins over queued trigger");
    fixture.set(FirstBurst, 1); fixture.set(ClockSource, 2); fixture.set(CvDivider, 64);
    fixture.message(0x90, 60, 127); fixture.block();
    require(near(fixture.sample(13), 0) && fixture.sample(15) > 0, "On clock arms without an immediate envelope");
    fixture.message(0x90, 62, 127); fixture.block();
    require(near(fixture.sample(13), 8), "MIDI step note advances an armed burst independently of CV divider");
}

void midiVelocitySurvivesHeldCvAndStepActions() {
    for (int stepSource = 0; stepSource < 3; ++stepSource) {
        Fixture fixture;
        fixture.notes(); fixture.set(Level, 100); fixture.set(TriggerInput, 1);
        fixture.voltages[0] = 2; fixture.block();
        require(countNoteOns() == 0, "held-high CV routing starts without a fresh trigger edge");
        fixture.voltages[0] = 0; fixture.block();
        fixture.voltages[0] = 2; fixture.block();
        require(countNoteOns() == 1 && near(fixture.sample(13), 10) && midiEvents.back().data2 == 127,
                "fresh CV trigger starts a full-level burst");
        fixture.message(0x90, 60, 64);
        if (stepSource == 0) fixture.button(kNT_potButtonR);
        else if (stepSource == 1) fixture.set(ManualStep, 1);
        else fixture.message(0x90, 62, 127);
        fixture.block();
        require(countNoteOns() == 2 && near(fixture.sample(13), 10.0f * 64.0f / 127.0f),
                "held-high CV and pending step action preserve MIDI trigger velocity");
        requireNote(midiEvents.back(), 1, 0x90, 60);
        require(midiEvents.back().data2 == 64, "UI, parameter and MIDI step actions do not overwrite pending note velocity");
        fixture.button(kNT_potButtonL); fixture.block();
        require(near(fixture.sample(13), 10) && midiEvents.back().data2 == 127,
                "explicit UI trigger restores full trigger velocity");
        fixture.reset(); requireBalancedNotes();
    }
}

void midiCcEndpointsAndHostOffset() {
    Fixture fixture;
    struct Mapping { uint8_t cc; Parameter parameter; int low, high; };
    const Mapping mappings[] = {{20, Bursts, 1, 40}, {21, EnvelopeShape, 0, 7}, {22, Width, 20, 2500},
        {23, Rate, 0, 5100}, {24, Slope, -100, 100}, {25, Level, 0, 100}, {26, Tempo, 200, 3000}};
    for (const Mapping& mapping : mappings) {
        fixture.message(0xB0, mapping.cc, 0);
        require(fixture.value(mapping.parameter) == mapping.low, "CC zero maps to parameter minimum");
        require(parameterWrites.back().index == parameterOffset + mapping.parameter, "audio host setter applies common offset");
        fixture.message(0xB0, mapping.cc, 127);
        require(fixture.value(mapping.parameter) == mapping.high, "CC 127 maps to parameter maximum");
    }
    fixture.message(0xB0, 23, 1);
    require(fixture.value(Rate) == 24, "first nonzero rate CC selects minimum nonzero rate");
    fixture.set(Rate, 1);
    require(fixture.value(Rate) == 1 && fixture.algorithm->parameters[Rate].scaling == kNT_scaling100,
            "NT rate parameter supports the documented 0.01 Hz extension independently of CC scaling");
    fixture.set(ManualTrigger, 1); fixture.set(ManualReset, 1); fixture.set(ManualStep, 1);
    require(fixture.value(ManualTrigger) == 0 && fixture.value(ManualReset) == 0 && fixture.value(ManualStep) == 0,
            "momentary action parameters reset via host callback");
    fixture.block();
    require(near(fixture.sample(13), 0) && near(fixture.sample(15), 0), "queued reset outranks all momentary actions");
    fixture.set(EnvelopeShape, 7); fixture.set(Rate, 0); fixture.set(FirstBurst, 0);
    fixture.message(0xB0, 115, 63); fixture.block();
    require(near(fixture.sample(15), 0), "action CC values below 64 are ignored");
    fixture.message(0xB0, 115, 64); fixture.block();
    require(fixture.sample(15) > 0, "trigger CC accepts value 64");
    fixture.message(0xB0, 116, 64); fixture.block();
    require(near(fixture.sample(15), 0), "reset CC cancels the active sequence");
}

void resetPriorityAndChannelPanic() {
    Fixture fixture;
    fixture.notes();
    fixture.set(ClockInput, 2); fixture.set(ResetInput, 3);
    fixture.block();
    fixture.trigger();
    fixture.button(kNT_potButtonL | kNT_potButtonR);
    fixture.voltages[1] = 2; fixture.voltages[2] = 2;
    fixture.block();
    require(countNoteOns() == 1 && near(fixture.sample(13), 0) && near(fixture.sample(15), 0) && near(fixture.sample(16), 0),
            "CV reset wins over UI trigger and simultaneous clock without EOC");
    requireBalancedNotes();
    fixture.voltages[1] = fixture.voltages[2] = 0; fixture.block();
    for (uint8_t cc : {uint8_t{120}, uint8_t{123}}) {
        midiEvents.clear();
        fixture.trigger();
        fixture.button(kNT_potButtonL);
        fixture.message(0xB0, cc, 0);
        fixture.block();
        require(countNoteOns() == 1 && near(fixture.sample(15), 0), "channel panic cancels a pending UI trigger");
        requireBalancedNotes();
    }
}

void realtimePanicCancelsPendingUi() {
    Fixture fixture;
    fixture.notes();
    for (uint8_t byte : {uint8_t{0xFC}, uint8_t{0xFF}}) {
        midiEvents.clear();
        fixture.trigger();
        fixture.button(kNT_potButtonL);
        fixture.realtime(byte);
        fixture.block(); fixture.block();
        require(countNoteOns() == 1 && near(fixture.sample(13), 0) && near(fixture.sample(15), 0),
                "Stop or System Reset cancels a queued UI start");
        requireBalancedNotes();
    }
}

void midiStopRequiresContinueOrStart() {
    Fixture fixture;
    fixture.notes(); fixture.set(ClockSource, 3); fixture.set(FirstBurst, 1); fixture.set(Division, 0);
    fixture.realtime(0xFA); fixture.block();
    for (int tick = 0; tick < 3; ++tick) { fixture.realtime(0xF8); fixture.block(); }
    require(countNoteOns() == 1, "MIDI Start arms a sequence and divided MIDI clocks start its burst");
    fixture.button(kNT_potButtonL);
    fixture.realtime(0xFC); fixture.block();
    requireBalancedNotes();
    for (int tick = 0; tick < 6; ++tick) { fixture.realtime(0xF8); fixture.block(); }
    require(countNoteOns() == 1 && near(fixture.sample(15), 0), "MIDI clocks after Stop do not revive canceled pending UI start");
    midiEvents.clear();
    fixture.trigger();
    for (int tick = 0; tick < 6; ++tick) { fixture.realtime(0xF8); fixture.block(); }
    require(countNoteOns() == 0 && near(fixture.sample(13), 0), "manually rearmed On clock sequence respects stopped MIDI transport");
    fixture.realtime(0xFB); fixture.block();
    for (int tick = 0; tick < 3; ++tick) { fixture.realtime(0xF8); fixture.block(); }
    require(countNoteOns() == 1, "MIDI Continue resumes divided clock advancement");
    fixture.reset(); requireBalancedNotes();
    midiEvents.clear();
    fixture.realtime(0xFA); fixture.block();
    for (int tick = 0; tick < 3; ++tick) { fixture.realtime(0xF8); fixture.block(); }
    require(countNoteOns() == 1, "MIDI Start restarts and rearms external MIDI transport");
    fixture.reset(); requireBalancedNotes();
}

void midiStartDoesNotArmOtherClockModes() {
    for (int mode : {0, 1, 2, 4}) {
        Fixture fixture;
        fixture.set(ClockSource, mode);
        fixture.realtime(0xFA); fixture.block();
        fixture.realtime(0xFB); fixture.block();
        require(near(fixture.sample(13), 0) && near(fixture.sample(15), 0),
                "MIDI Start and Continue do not arm sequences in other clock modes");
    }
}

void midiTransportOrderingAndResetPriority() {
    Fixture fixture;
    fixture.notes(); fixture.set(ClockSource, 3); fixture.set(FirstBurst, 1); fixture.set(Division, 0);
    fixture.realtime(0xFC); fixture.realtime(0xFA);
    for (int tick = 0; tick < 3; ++tick) fixture.realtime(0xF8);
    fixture.block();
    require(countNoteOns() == 1 && near(fixture.sample(13, 3), 8),
            "Start following Stop before render wins and preserves its following clocks");
    fixture.reset(); requireBalancedNotes();
    midiEvents.clear();
    fixture.realtime(0xFA); fixture.realtime(0xFC); fixture.block();
    require(countNoteOns() == 0 && near(fixture.sample(15), 0), "Stop following Start before render wins");
    for (int tick = 0; tick < 3; ++tick) fixture.realtime(0xF8);
    fixture.realtime(0xFA); fixture.block();
    require(countNoteOns() == 0 && near(fixture.sample(13), 0) && fixture.sample(15) > 0,
            "clocks preceding Start are discarded rather than beginning the new sequence");
    for (int tick = 0; tick < 3; ++tick) fixture.realtime(0xF8);
    fixture.block();
    require(countNoteOns() == 1, "clocks following Start advance the newly armed sequence");
    fixture.reset(); requireBalancedNotes();
    midiEvents.clear();
    fixture.realtime(0xFA); fixture.realtime(0xFF); fixture.realtime(0xFA);
    for (int tick = 0; tick < 3; ++tick) fixture.realtime(0xF8);
    fixture.block();
    require(countNoteOns() == 0 && near(fixture.sample(15), 0), "System Reset remains prioritary over later queued Start and clocks");
    fixture.set(ManualReset, 1); fixture.realtime(0xFA);
    for (int tick = 0; tick < 3; ++tick) fixture.realtime(0xF8);
    fixture.block();
    require(countNoteOns() == 0 && near(fixture.sample(15), 0), "explicit reset outranks MIDI Start and clocks in the same block");
}

void noteRetriggerAndVelocity() {
    Fixture fixture;
    fixture.notes(); fixture.set(Level, 100);
    fixture.trigger();
    require(midiEvents.size() == 1, "first burst sends one note");
    requireNote(midiEvents[0], 1, 0x90, 60);
    require(midiEvents[0].data2 == 127, "ten-volt burst yields full MIDI velocity");
    fixture.message(0x90, 60, 64); fixture.block();
    require(midiEvents.size() == 3, "retrigger sends old Note Off before new Note On");
    requireNote(midiEvents[1], 1, 0x80, 60); requireNote(midiEvents[2], 1, 0x90, 60);
    require(midiEvents[2].data2 == 64, "output note velocity includes trigger velocity and level volts");
    fixture.reset();
    requireBalancedNotes();
    const size_t oldCount = countNoteOns();
    fixture.set(Level, 0);
    fixture.trigger();
    require(countNoteOns() == oldCount && near(fixture.sample(13), 0),
            "zero envelope level does not send a MIDI Note On");
}

void noteReconfigurationAndDisabling() {
    const Parameter changed[] = {MidiDestination, MidiOutputChannel, MidiOutputNote, MidiNotes};
    const int newValues[] = {4, 3, 70, 0};
    for (size_t i = 0; i < 4; ++i) {
        Fixture fixture;
        fixture.notes(); fixture.trigger();
        fixture.set(changed[i], newValues[i]); fixture.block();
        require(midiEvents.size() == 2, "note configuration change releases old note without inventing a new burst");
        requireNote(midiEvents.back(), 1, 0x80, 60);
        requireBalancedNotes();
        if (changed[i] != MidiNotes) {
            fixture.trigger();
            const uint32_t destination = changed[i] == MidiDestination ? 4 : 1;
            const uint8_t channel = changed[i] == MidiOutputChannel ? 2 : 0;
            const uint8_t note = changed[i] == MidiOutputNote ? 70 : 60;
            requireNote(midiEvents.back(), destination, static_cast<uint8_t>(0x90 | channel), note);
            fixture.reset(); requireBalancedNotes();
        }
    }
    Fixture fixture;
    fixture.notes(); fixture.trigger();
    fixture.set(MidiDestination, 0); fixture.block();
    requireNote(midiEvents.back(), 1, 0x80, 60);
    requireBalancedNotes();
}

void naturalCompletionClosesNotesAndTransport() {
    Fixture fixture;
    fixture.notes(); fixture.set(Width, 20); fixture.set(MidiTransportOutput, 1);
    fixture.trigger();
    fixture.run(framesForMilliseconds(100));
    require(countNoteOns() == 1 && countMidi(0x80) == 1, "natural completion closes final burst note");
    requireBalancedNotes();
    const auto starts = std::count_if(midiEvents.begin(), midiEvents.end(), [](const MidiEvent& event) { return event.status == 0xFA; });
    const auto stops = std::count_if(midiEvents.begin(), midiEvents.end(), [](const MidiEvent& event) { return event.status == 0xFC; });
    require(starts == 1 && stops == 1 && near(fixture.sample(15), 0), "natural completion closes output transport once");
}

void retriggerRestartsMidiTransport() {
    Fixture fixture;
    fixture.notes(); fixture.set(MidiTransportOutput, 1);
    fixture.trigger();
    require(midiEvents.size() == 2 && midiEvents[0].status == 0xFA && midiEvents[1].status == 0x90,
            "first burst starts MIDI transport before its note");
    fixture.trigger();
    require(midiEvents.size() == 6, "RET emits old note off, transport stop, new start and new note");
    requireNote(midiEvents[2], 1, 0x80, 60);
    require(midiEvents[3].status == 0xFC && midiEvents[4].status == 0xFA,
            "RET closes previous transport before sending a fresh Start");
    requireNote(midiEvents[5], 1, 0x90, 60);
    fixture.set(RetriggerMode, 1);
    fixture.trigger();
    require(midiEvents.size() == 6, "ONE ignored trigger does not reset notes or output transport");
    fixture.reset(); requireBalancedNotes();
}

void envelopeCcRateAndTerminalZero() {
    Fixture fixture;
    fixture.set(MidiDestination, 1); fixture.set(MidiEnvelopeCc, 74); fixture.set(MidiCcRate, 20);
    fixture.set(Bursts, 1); fixture.set(Width, 2500); fixture.set(EnvelopeShape, 0);
    fixture.set(ManualTrigger, 1);
    fixture.run(NT_globals.sampleRate);
    require(countMidi(0xB0) >= 5 && countMidi(0xB0) <= 21, "envelope CC stream follows bounded twenty-hertz rate");
    fixture.run(NT_globals.sampleRate * 2);
    require(countMidi(0xB0) <= 62, "CC rate remains bounded through completion including terminal zero");
    require(!midiEvents.empty() && midiEvents.back().status == 0xB0 && midiEvents.back().data1 == 74 && midiEvents.back().data2 == 0,
            "CC output ends at zero after the sequence");
}

void terminalZeroBypassesCcCountdown() {
    Fixture fixture;
    fixture.set(MidiDestination, 1); fixture.set(MidiEnvelopeCc, 74); fixture.set(MidiCcRate, 1);
    fixture.set(Bursts, 1); fixture.set(Width, 20); fixture.set(EnvelopeShape, 7); fixture.set(Level, 100);
    fixture.set(ManualTrigger, 1);
    fixture.run(framesForMilliseconds(100));
    require(midiEvents.size() == 2 && midiEvents[0].data2 == 127 && midiEvents[1].data2 == 0,
            "terminal zero is immediate even before the next one-hertz CC deadline");
}

void ccRouteChangeReturnsOldControllerToZero() {
    Fixture fixture;
    fixture.set(MidiDestination, 1); fixture.set(MidiEnvelopeCc, 74);
    fixture.set(EnvelopeShape, 7); fixture.set(Width, 2500); fixture.trigger();
    const size_t before = midiEvents.size();
    fixture.set(MidiDestination, 4); fixture.set(MidiOutputChannel, 3); fixture.set(MidiEnvelopeCc, 75);
    fixture.block();
    require(midiEvents.size() >= before + 2, "controller route change emits old zero and new value");
    const MidiEvent& zero = midiEvents[before];
    require(zero.destination == 1 && zero.status == 0xB0 && zero.data1 == 74 && zero.data2 == 0,
            "old controller zero uses its original route");
    fixture.set(MidiEnvelopeCc, -1); fixture.block();
    const MidiEvent& disabled = midiEvents.back();
    require(disabled.destination == 4 && disabled.status == 0xB2 && disabled.data1 == 75 && disabled.data2 == 0,
            "disabling envelope CC clears its previous destination and channel");
}

void midiClockUsesHostSampleRate() {
    Fixture fixture;
    fixture.set(ClockSource, 1); fixture.set(Tempo, 1200);
    fixture.set(MidiDestination, 1); fixture.set(MidiClockOutput, 1);
    fixture.run(NT_globals.sampleRate * 2);
    const size_t ticks = countClockTicks();
    require(ticks >= 95 && ticks <= 97, "120 BPM emits 96 MIDI ticks in two seconds at current host sample rate");
    require(std::all_of(midiEvents.begin(), midiEvents.end(), [](const MidiEvent& event) { return event.destination == 1; }),
            "MIDI clock follows configured destination");
}


void dualGeneratorsMixAndTargetedStop() {
    Fixture f;
    f.set(Generator2Enabled,1); f.set(EnvelopeShape,7); f.set(G2EnvelopeShape,7);
    f.set(Bursts,1); f.set(G2Bursts,1); f.set(Width,2500); f.set(G2Width,2500);
    f.set(G2EnvelopeOutput,18); f.set(G2EnvelopeOutputMode,1);
    f.set(G2ActiveOutput,19); f.set(G2ActiveOutputMode,1);
    f.set(MixOutput,20); f.set(MixOutputMode,1);
    f.set(BusyAllOutput,21); f.set(BusyAllOutputMode,1);
    f.set(LevelsOutput,22); f.set(LevelsOutputMode,1);
    f.set(BurstOutput,23); f.set(BurstOutputMode,1);
    f.trigger();
    require(near(f.sample(13),8)&&near(f.sample(18),0)&&near(f.sample(20),8)&&near(f.sample(21),10),
            "G1 runs independently and feeds shared envelope/busy outputs");
    require(near(f.sample(22),10)&&near(f.sample(23),5),"level and burst events have dedicated outputs");
    f.set(G2ManualTrigger,1); f.block();
    require(near(f.sample(13),8)&&near(f.sample(18),8)&&near(f.sample(20),10),
            "two independent voices sum into a mix saturated at ten volts");
    f.set(ManualStop,1); f.block();
    require(near(f.sample(13),0)&&near(f.sample(18),8)&&near(f.sample(21),10)&&near(f.sample(16),0),
            "targeted stop leaves the other voice and shared busy running without EOC");
    f.set(ManualStopAll,1); f.block();
    require(near(f.sample(18),0)&&near(f.sample(20),0)&&near(f.sample(21),0),"Stop all silences both generators");
}

void focusedUiAndDisabledGenerator() {
    Fixture f;
    f.set(G2EnvelopeShape,7); f.set(G2Width,2500); f.set(G2EnvelopeOutput,18); f.set(G2EnvelopeOutputMode,1);
    f.set(G2ManualTrigger,1); f.block();
    require(near(f.sample(18),0),"G2 stays silent until explicitly enabled");
    f.set(Generator2Enabled,1); f.set(FocusGenerator,1); f.button(kNT_potButtonL); f.block();
    require(near(f.sample(13),0)&&near(f.sample(18),8),"the trigger button addresses focused G2");
    f.button(kNT_encoderButtonL); f.block();
    require(near(f.sample(18),0),"the left encoder button stops the focused voice");
    _NT_uiData data{}; data.encoders[0]=-1; hostFactory->customUi(f.algorithm,data);
    require(f.value(FocusGenerator)==0,"left encoder changes focus through the UI parameter setter");
    f.set(EnvelopeShape,7); f.button(kNT_encoderButtonR); f.block();
    require(near(f.sample(13),8)&&near(f.sample(18),8),"right encoder button triggers both enabled generators");
    f.set(Generator2Enabled,0); f.block();
    require(near(f.sample(18),0)&&near(f.sample(13),8),"disabling G2 leaves G1 untouched");
}

void muteAndOptionalVelocity() {
    Fixture f;
    f.notes(); f.set(VelocityEnabled,0); f.message(0x90,60,32); f.block();
    require(near(f.sample(13),8)&&midiEvents.back().data2==102,"disabled velocity preserves configured level");
    f.set(Mute,1); f.block();
    require(near(f.sample(13),0)&&near(f.sample(14),0)&&f.sample(15)>0,
            "mute silences envelope and gate while the sequence remains busy");
    requireBalancedNotes();
    const size_t before=midiEvents.size(); f.set(Mute,0); f.block();
    require(near(f.sample(13),8)&&midiEvents.size()==before,"unmute resumes CV without inventing a MIDI note");
    f.reset(); f.set(VelocityEnabled,1); f.message(0x90,60,32); f.block();
    require(near(f.sample(13),8.0f*32/127),"enabled velocity scales the next accepted trigger");
    f.set(ManualResetAll,1); f.block(); requireBalancedNotes();
}

void sharedCvDepthAndInputSnapshot() {
    Fixture f;
    f.set(EnvelopeShape,7); f.set(Width,20); f.set(Bursts,4);
    f.set(Generator2Enabled,1); f.set(G2EnvelopeShape,7); f.set(G2Width,20); f.set(G2Bursts,4); f.set(G2Rate,0);
    f.set(G2ActiveOutput,18); f.set(G2ActiveOutputMode,1);
    f.set(CountCvInput,1); f.voltages[0]=0; f.set(G2CvDepth,0);
    f.set(ManualTriggerAll,1); f.run(framesForMilliseconds(100));
    require(near(f.sample(15),0)&&f.sample(18)>0,"the shared count CV has independent depth on each voice");
    f.set(ManualResetAll,1); f.block(); f.set(CvEnabled,0); f.set(ManualTriggerAll,1);
    f.run(framesForMilliseconds(100));
    require(f.sample(15)>0&&f.sample(18)>0,"CV disable preserves both panel counts");
    f.set(ManualResetAll,1); f.block(); f.set(CvEnabled,1); f.set(CountCvInput,0);
    f.set(ShapeCvInput,13); f.voltages[12]=0.5f; f.set(Width,2500);
    f.set(G2Width,2500); f.set(G2CvDepth,100); f.set(ManualTriggerAll,1); f.block();
    require(f.sample(13)<0.01f,"shape CV uses eight equal bins including the first sine bin");
    // Both voices read the same CV before G1 replaces that bus.
    f.set(G2EnvelopeOutput,19); f.set(G2EnvelopeOutputMode,1);
    f.set(ManualTriggerAll,1); f.block();
    require(near(f.sample(13),f.sample(19)),"all shared CV inputs are snapshotted before either voice writes an output");
}

void bipolarCountAndSlopeCapture() {
    {
        Fixture f; f.notes(); f.set(CvResponse,1); f.set(CountCvInput,1); f.voltages[0]=0.5f;
        f.trigger(); f.voltages[0]=-5;
        for(int i=0;i<8;++i){f.set(ManualStep,1);f.block();}
        require(countNoteOns()==5,"positive bipolar count CV increases a panel count of one and is latched at trigger");
        f.reset(); requireBalancedNotes(); midiEvents.clear();
        f.set(Bursts,40); f.trigger();
        for(int i=0;i<8;++i){f.set(ManualStep,1);f.block();}
        require(countNoteOns()==1,"negative bipolar count CV reduces the captured count to one");
        f.reset(); requireBalancedNotes();
    }
    for(const float voltage : {-2.5f,2.5f}){
        Fixture f; f.notes(); f.set(Bursts,2); f.set(CvResponse,1); f.set(SlopeCvInput,1); f.voltages[0]=voltage;
        f.trigger();
        require(near(f.sample(13),voltage>0?4.16f:8.0f),"bipolar slope CV creates a contour from a neutral panel slope");
        f.voltages[0]=-voltage; f.set(ManualStep,1);f.block();
        require(near(f.sample(13),voltage>0?8.0f:4.16f),"slope changes remain latched until the next accepted trigger");
        f.reset(); requireBalancedNotes();
    }
}

uint32_t measuredGateDuration(Fixture& f){
    f.trigger();
    uint32_t duration=0;
    for(int frame=0;frame<f.lastFrames;++frame)if(f.sample(14,frame)>0)++duration;
    const uint32_t limit=NT_globals.sampleRate*3;
    while(duration<limit){
        f.block();
        for(int frame=0;frame<f.lastFrames;++frame){
            if(f.sample(14,frame)==0)return duration;
            ++duration;
        }
    }
    require(false,"modulated gate duration is bounded");return 0;
}

void bipolarContinuousTimeAndLevel(){
    {
        Fixture f; f.notes(); f.set(Width,100); f.set(CvResponse,1); f.set(WidthCvInput,1);
        for(const float voltage : {-0.625f,0.0f,1.25f}){
            f.reset(); f.voltages[0]=voltage;
            const uint32_t duration=measuredGateDuration(f);
            const float expected=static_cast<float>(NT_globals.sampleRate)*0.1f*std::pow(2.0f,4.0f*voltage/5.0f);
            require(std::fabs(static_cast<float>(duration)-expected)<=1.5f,"bipolar width CV follows continuous exponential time around the panel value");
        }
    }
    {
        Fixture f; f.notes(); f.set(Bursts,40); f.set(Rate,400); f.set(CvResponse,1); f.set(IntervalCvInput,1); f.voltages[0]=1.25f;
        f.trigger();f.run(framesForMilliseconds(110));
        require(countNoteOns()==1,"positive bipolar rate CV retains the oscillator phase before its new period");
        f.run(framesForMilliseconds(30));
        require(countNoteOns()==2,"positive bipolar rate CV doubles four hertz to eight hertz");
        f.reset();requireBalancedNotes();midiEvents.clear();f.voltages[0]=-1.25f;
        f.trigger();f.run(framesForMilliseconds(490));
        require(countNoteOns()==1,"negative bipolar rate CV halves four hertz to two hertz");
        f.run(framesForMilliseconds(20));
        require(countNoteOns()==2,"negative bipolar rate CV advances at the slower continuous rate");
        f.reset();requireBalancedNotes();midiEvents.clear();f.set(Rate,0);f.voltages[0]=5;
        f.trigger();f.run(framesForMilliseconds(600));
        require(countNoteOns()==1,"bipolar rate CV preserves the panel Off position");
        f.reset();requireBalancedNotes();
    }
    {
        Fixture f;f.notes();f.set(Level,50);f.set(CvResponse,1);f.set(AmplitudeCvInput,1);f.voltages[0]=1.25f;f.trigger();
        require(near(f.sample(13),7.5f),"positive bipolar amplitude CV increases the configured level");
        f.voltages[0]=-1.25f;f.block();require(near(f.sample(13),2.5f),"negative bipolar amplitude CV attenuates the live envelope");
        f.voltages[0]=5;f.block();require(near(f.sample(13),10),"bipolar amplitude remains bounded at ten volts");
        f.voltages[0]=-5;f.block();require(near(f.sample(13),0),"bipolar amplitude remains bounded at zero volts");
        f.reset();requireBalancedNotes();
    }
}

void musicalIntervalCvPositionLaw(){
    Fixture f;f.notes();f.set(TimingMode,1);f.set(WidthRatio,0);f.set(IntervalRatio,16);f.set(Bursts,4);
    f.set(IntervalCvInput,1);f.set(TimeCvDepth,50);f.voltages[0]=0;f.trigger();
    f.run(framesForMilliseconds(400));require(countNoteOns()==1,"reference interval CV interpolates the normalized grid position before selecting a ratio");
    f.run(framesForMilliseconds(200));require(countNoteOns()==2,"half depth between sixteen quarters and zero CV selects one quarter");
    f.run(framesForMilliseconds(500));require(countNoteOns()==3,"reference interval modulation keeps the selected quarter-note cadence");
    f.reset();requireBalancedNotes();midiEvents.clear();
    f.set(IntervalRatio,1);f.set(TimeCvDepth,0);f.trigger();f.run(framesForMilliseconds(100));
    require(countNoteOns()>1,"zero time depth preserves the shortest enabled interval with a connected zero CV");
    f.reset();requireBalancedNotes();midiEvents.clear();f.set(CvResponse,1);f.set(TimeCvDepth,100);f.trigger();f.run(framesForMilliseconds(100));
    require(countNoteOns()>1,"bipolar zero volts preserves the shortest enabled interval");
    f.reset();requireBalancedNotes();
    midiEvents.clear();f.voltages[0]=0.01f;f.trigger();f.run(framesForMilliseconds(100));
    require(countNoteOns()>1,"a small positive bipolar interval CV preserves the shortest enabled ratio");
    f.reset();requireBalancedNotes();midiEvents.clear();f.voltages[0]=-0.01f;f.trigger();f.run(framesForMilliseconds(100));
    require(countNoteOns()==1,"negative bipolar interval CV can select Off below the first grid position");
    f.reset();requireBalancedNotes();
}

void morphShapeCvAndNeutralNonFiniteInput(){
    Fixture f;f.set(EnvelopeShape,0);f.set(Bursts,1);f.set(Width,2500);f.set(ShapeCvInput,1);f.voltages[0]=1.25f;
    f.set(ReferenceCurves,1);f.trigger();require(near(f.sample(13),0),"DROID response keeps the eight equal shape bins");
    f.reset();f.set(ReferenceCurves,2);f.trigger();
    require(near(f.sample(13),2.0f),"Morph response continuously mixes adjacent DROID curves at a shared phase");
    f.voltages[0]=0.5f;f.block();require(f.sample(13)>5.5f,"shape morph CV changes an envelope while it is running");
    f.reset();f.set(CvResponse,1);f.set(EnvelopeShape,1);f.voltages[0]=0.5f;f.trigger();
    require(near(f.sample(13),2.4f),"bipolar shape CV offsets the panel shape continuously when Morph is selected");
    f.reset();f.set(EnvelopeShape,7);f.set(ShapeCvInput,13);f.voltages[12]=std::numeric_limits<float>::quiet_NaN();f.trigger();
    require(near(f.sample(13),8),"non-finite bipolar shape CV is neutral before an aliased output replaces its bus");
}

void musicalRatiosAndManualStep() {
    Fixture f;
    f.notes(); f.set(Width,100); f.set(TimingMode,1); f.set(WidthRatio,0); f.set(IntervalRatio,9); f.set(Bursts,3);
    f.set(Generator2Enabled,1); f.set(G2TimingMode,1); f.set(G2WidthRatio,0); f.set(G2IntervalRatio,10);
    f.set(G2Bursts,3); f.set(G2EnvelopeShape,7); f.set(G2MidiDestination,1); f.set(G2MidiNotes,1);
    f.set(ManualTriggerAll,1); f.run(framesForMilliseconds(1600));
    size_t first=0,second=0;
    for(const auto& event:midiEvents)if((event.status&0xF0)==0x90&&event.data2){
        if(event.data1==60)++first;if(event.data1==64)++second;
    }
    require(first==3&&second==3,"binary and dotted intervals share the same musical reference");
    requireBalancedNotes(); midiEvents.clear();
    f.set(Generator2Enabled,0); f.set(IntervalRatio,0); f.set(Bursts,2); f.trigger();
    f.run(NT_globals.sampleRate);
    require(countNoteOns()==1&&f.sample(15)>0,"musical interval Off waits for manual steps");
    f.set(ManualStep,1); f.block(); f.run(framesForMilliseconds(100));
    require(countNoteOns()==2&&near(f.sample(15),0),"manual step advances an interval-Off sequence to completion");
    requireBalancedNotes();
}

void musicalTransportFiltersSources() {
    for(int source:{0,1}) {
        Fixture f; f.notes(); f.set(TimingMode,1); f.set(SharedClockSource,source); f.set(WidthRatio,15);
        f.trigger(); const size_t before=countMidi(0x80);
        f.realtime(0xFC); f.block();
        require(f.sample(15)>0&&countMidi(0x80)==before,"MIDI Stop cannot interrupt musical Internal or CV clock voices");
        f.reset(); requireBalancedNotes();
    }
    Fixture f; f.notes(); f.set(TimingMode,1); f.set(SharedClockSource,2); f.set(WidthRatio,15);
    f.trigger(); f.realtime(0xFC); f.block();
    require(near(f.sample(15),0),"MIDI Stop stops voices using the musical MIDI reference");
    requireBalancedNotes();
}

void presetActionsAndJsonRoundTrip() {
    JsonData json;
    {
        Fixture f;
        f.set(PresetSlot,7); f.set(Bursts,17); f.set(Width,777); f.set(Slope,-65);
        f.set(GlideRise,1234); f.set(CvDepth,37); f.set(CountCvDepth,25); f.set(ReferenceCurves,2); f.set(CvResponse,1);
        f.set(PresetSave,1); f.block(); f.set(Bursts,2); f.set(Width,20); f.set(PresetLoad,1); f.block();
        require(f.value(Bursts)==17&&f.value(Width)==777&&f.value(Slope)==-65&&f.value(GlideRise)==1234
            &&f.value(CvDepth)==37&&f.value(CountCvDepth)==25&&f.value(ReferenceCurves)==2&&f.value(CvResponse)==1,
            "saving and loading captures Morph while leaving the shared CV response intact");
        f.set(G2PresetSlot,3); f.set(G2Bursts,29); f.set(G2IntervalRatio,0); f.set(G2PresetSave,1); f.block();
        _NT_jsonStream stream(&json); hostFactory->serialise(f.algorithm,stream);
        require(json.parents.size()==1,"preset bank serialisation closes every array");
    }
    {
        Fixture f;
        _NT_jsonParse parse(&json,0);
        require(hostFactory->deserialise(f.algorithm,parse),"user banks restore through the SDK JSON parser");
        f.set(PresetSlot,7); f.set(PresetLoad,1); f.set(G2PresetSlot,3); f.set(G2PresetLoad,1); f.block();
        require(f.value(Bursts)==17&&f.value(Width)==777&&f.value(GlideRise)==1234&&f.value(ReferenceCurves)==2&&f.value(CvResponse)==0&&f.value(G2Bursts)==29
            &&f.value(G2IntervalRatio)==0,"both generator banks and interval Off survive host preset persistence");
        f.set(PresetFactory,1); f.block();
        require(f.value(Bursts)==16&&f.value(EnvelopeShape)==6&&f.value(TimingMode)==1,
                "Factory restores the selected reference sound and clears its override");
        JsonData malformed=json; malformed.tokens[2].number=99;
        _NT_jsonParse invalid(&malformed,0);
        require(!hostFactory->deserialise(f.algorithm,invalid),"unknown preset data versions are rejected");
        JsonData lateVersion=json;
        std::rotate(lateVersion.tokens.begin()+1,lateVersion.tokens.begin()+3,lateVersion.tokens.end());
        lateVersion.tokens[2].end=lateVersion.tokens.size()-2;
        lateVersion.tokens.back().number=99;
        _NT_jsonParse lateInvalid(&lateVersion,0);
        require(!hostFactory->deserialise(f.algorithm,lateInvalid),"a version error following staged banks rejects the whole document");
        JsonData versionless=json;
        versionless.tokens.erase(versionless.tokens.begin()+1,versionless.tokens.begin()+3);
        --versionless.tokens[0].count; versionless.tokens[2].end-=2;
        _NT_jsonParse noVersion(&versionless,0);
        require(!hostFactory->deserialise(f.algorithm,noVersion),"bank data requires an explicit format version");
        JsonData unknown=json;
        const std::vector<JsonToken> extra={{JsonToken::Name,"futureData",0,0,2},
            {JsonToken::Array,"",0,2,6},{JsonToken::Number,"",1,0,4},
            {JsonToken::Array,"",0,1,6},{JsonToken::Number,"",2,0,6}};
        unknown.tokens.insert(unknown.tokens.begin()+1,extra.begin(),extra.end());
        ++unknown.tokens[0].count; unknown.tokens[9].end+=extra.size();
        _NT_jsonParse future(&unknown,0);
        require(hostFactory->deserialise(f.algorithm,future),"unknown nested host fields are skipped while known bank data restores");
        f.set(G2Bursts,1); f.set(G2PresetLoad,1); f.block();
        require(f.value(G2Bursts)==29,"a rejected JSON document cannot partially overwrite the current bank");
        JsonData old; _NT_jsonParse legacy(&old,0);
        require(hostFactory->deserialise(f.algorithm,legacy),"old host presets without bank data remain loadable");
    }
}

void disablingSecondVoiceClearsMidiCc() {
    Fixture f; f.set(Generator2Enabled,1); f.set(G2EnvelopeShape,7); f.set(G2Width,2500);
    f.set(G2MidiDestination,1); f.set(G2MidiEnvelopeCc,75); f.set(G2ManualTrigger,1); f.block();
    require(!midiEvents.empty()&&midiEvents.back().data2>0,"G2 sends its envelope controller");
    f.set(Generator2Enabled,0); f.block();
    require(midiEvents.back().status==0xB1&&midiEvents.back().data1==75&&midiEvents.back().data2==0,
            "disabling G2 clears a held outgoing controller immediately");
}

void performancePackPlaysEveryScene() {
    require(performancePresetCount==16,"performance pack contains exactly sixteen scenes");
    for(unsigned slot=0;slot<performancePresetCount;++slot){
        Fixture f;
        const PerformancePreset scene=performancePreset(slot);
        require(performancePresetNames[slot]&&std::strlen(performancePresetNames[slot])>0,
                "every scene has a user-facing name");
        for(unsigned previous=0;previous<slot;++previous)
            require(std::strcmp(performancePresetNames[slot],performancePresetNames[previous])!=0,
                    "performance scene names are distinct");
        f.set(G2EnvelopeOutput,18); f.set(G2EnvelopeOutputMode,1);
        f.set(MixOutput,20); f.set(MixOutputMode,1);
        f.set(PackPreset,static_cast<int>(slot)); f.set(LoadPackPreset,1); f.block();
        require(f.value(LoadPackPreset)==0&&near(f.sample(13),0)&&near(f.sample(18),0)
                &&near(f.sample(15),0)&&near(f.sample(16),0),
                "loading a pack scene is momentary, silent and produces no EOC");
        require(f.value(Bursts)==scene.voices[0].bursts&&f.value(G2Bursts)==scene.voices[1].bursts
                &&f.value(Width)==scene.voices[0].widthMilliseconds&&f.value(Rate)==scene.voices[0].rateCentihz
                &&f.value(TimingMode)==scene.voices[0].timingMode&&f.value(G2TimingMode)==scene.voices[1].timingMode
                &&f.value(Generator2Enabled)==scene.generator2Enabled&&f.value(ClockSource)==scene.clockSource
                &&f.value(CvResponse)==scene.cvResponse&&f.value(Tempo)==scene.tempo,
                "pack applies both voice sounds and their shared musical settings");
        float peak[2]={0,0};
        f.set(ManualTriggerAll,1);
        uint32_t remaining=NT_globals.sampleRate;
        while(remaining){
            const int frames=static_cast<int>(std::min<uint32_t>(remaining,blockCapacity));
            f.block(frames);
            for(int frame=0;frame<frames;++frame){
                const float first=f.sample(13,frame),second=f.sample(18,frame),mix=f.sample(20,frame);
                require(first>=0&&first<=10.0001f&&second>=0&&second<=10.0001f,
                        "every pack voice stays within 0..10 V");
                require(near(mix,std::min(10.0f,first+second),0.0002f),
                        "every pack scene produces the expected bounded mix");
                peak[0]=std::max(peak[0],first);peak[1]=std::max(peak[1],second);
            }
            remaining-=static_cast<uint32_t>(frames);
        }
        require(peak[0]>0.0001f,"each scene produces G1 envelopes immediately without a modulation cable");
        require(scene.generator2Enabled?peak[1]>0.0001f:peak[1]==0.0f,
                "enabled pack voices produce envelopes and disabled voices remain silent");
        f.set(ManualStopAll,1);f.block();
        require(near(f.sample(13),0)&&near(f.sample(18),0)&&near(f.sample(16),0),
                "Stop all silences every pack scene without an EOC");
    }
}

void performancePackPreservesUserMemoryAndRoutes() {
    Fixture f;
    f.set(PresetSlot,7); f.set(Bursts,17); f.set(PresetSave,1);
    f.set(G2PresetSlot,3); f.set(G2Bursts,29); f.set(G2PresetSave,1); f.block();
    JsonData before;
    { _NT_jsonStream stream(&before);hostFactory->serialise(f.algorithm,stream); }
    const Parameter routes[]={EnvelopeOutput,EnvelopeOutputMode,G2EnvelopeOutput,G2EnvelopeOutputMode,
        TriggerInput,ResetInput,ClockInput,WidthCvInput,ShapeCvInput,MidiDestination,MidiOutputChannel,
        MidiOutputNote,MidiNotes,MidiEnvelopeCc,G2MidiDestination,G2MidiOutputChannel,G2MidiOutputNote,
        G2MidiNotes,G2MidiEnvelopeCc,FocusGenerator,PresetSlot,G2PresetSlot};
    f.notes(4); f.set(Generator2Enabled,1); f.set(G2MidiDestination,2); f.set(G2MidiNotes,1);
    f.set(G2Width,2500); f.set(G2EnvelopeShape,7); f.set(G2EnvelopeOutput,18);f.set(G2EnvelopeOutputMode,1);
    f.set(ClockInput,2);f.set(WidthCvInput,4);f.set(ShapeCvInput,6);f.set(FocusGenerator,1);
    std::array<int,ARRAY_SIZE(routes)> retained{};
    for(unsigned i=0;i<ARRAY_SIZE(routes);++i)retained[i]=f.value(routes[i]);
    f.set(ManualTriggerAll,1);f.block();
    require(countMidi(0x90)==2,"both old voices own MIDI notes before pack replacement");
    midiEvents.clear();
    f.set(ManualTriggerAll,1);f.set(G2ManualStep,1);f.set(PresetSave,1);
    f.set(PackPreset,6);f.set(LoadPackPreset,1);f.block();
    require(countMidi(0x80)==2&&countMidi(0x90)==0,
            "scene loading releases both old MIDI notes and suppresses queued starts");
    for(unsigned i=0;i<ARRAY_SIZE(routes);++i)
        require(f.value(routes[i])==retained[i],"scene loading preserves CV/MIDI routes, focus and user-slot selections");
    JsonData after;
    { _NT_jsonStream stream(&after);hostFactory->serialise(f.algorithm,stream); }
    require(before.tokens.size()==after.tokens.size(),"loading a scene does not add user-bank overrides");
    for(size_t i=0;i<before.tokens.size();++i){
        const auto& left=before.tokens[i];const auto& right=after.tokens[i];
        require(left.kind==right.kind&&left.name==right.name&&left.number==right.number
                &&left.count==right.count&&left.end==right.end,
                "the complete saved user-bank state remains identical after loading a scene");
    }
    f.set(PresetLoad,1);f.set(G2PresetLoad,1);f.block();
    require(f.value(Bursts)==17&&f.value(G2Bursts)==29,
            "the original user sounds can still be recalled after using the performance pack");
}

} // namespace

_NT_jsonStream::_NT_jsonStream(void* reference) : refCon(reference) {}
_NT_jsonStream::~_NT_jsonStream() = default;
void _NT_jsonStream::addMemberName(const char* name) {
    auto& json = *static_cast<JsonData*>(refCon);
    ++json.tokens[json.parents.back()].count;
    json.append({JsonToken::Name, name, 0, 0, 0});
}
void _NT_jsonStream::addNumber(int value) {
    static_cast<JsonData*>(refCon)->append({JsonToken::Number, "", value, 0, 0});
}
void _NT_jsonStream::openArray() {
    auto& json = *static_cast<JsonData*>(refCon);
    const size_t index = json.append({JsonToken::Array, "", 0, 0, 0});
    json.parents.push_back(index);
}
void _NT_jsonStream::closeArray() {
    auto& json = *static_cast<JsonData*>(refCon);
    require(json.parents.size() > 1, "JSON arrays close in order");
    json.tokens[json.parents.back()].end = json.tokens.size();
    json.parents.pop_back();
}
_NT_jsonParse::_NT_jsonParse(void* reference, int index) : refCon(reference), i(index) {}
_NT_jsonParse::~_NT_jsonParse() = default;
bool _NT_jsonParse::numberOfObjectMembers(int& count) {
    const auto& tokens = static_cast<JsonData*>(refCon)->tokens;
    if (i < 0 || static_cast<size_t>(i) >= tokens.size() || tokens[static_cast<size_t>(i)].kind != JsonToken::Object) return false;
    count = static_cast<int>(tokens[static_cast<size_t>(i++)].count); return true;
}
bool _NT_jsonParse::numberOfArrayElements(int& count) {
    const auto& tokens = static_cast<JsonData*>(refCon)->tokens;
    if (i < 0 || static_cast<size_t>(i) >= tokens.size() || tokens[static_cast<size_t>(i)].kind != JsonToken::Array) return false;
    count = static_cast<int>(tokens[static_cast<size_t>(i++)].count); return true;
}
bool _NT_jsonParse::matchName(const char* name) {
    const auto& tokens = static_cast<JsonData*>(refCon)->tokens;
    if (i < 0 || static_cast<size_t>(i) >= tokens.size() || tokens[static_cast<size_t>(i)].kind != JsonToken::Name || tokens[static_cast<size_t>(i)].name != name) return false;
    ++i; return true;
}
bool _NT_jsonParse::number(int& value) {
    const auto& tokens = static_cast<JsonData*>(refCon)->tokens;
    if (i < 0 || static_cast<size_t>(i) >= tokens.size() || tokens[static_cast<size_t>(i)].kind != JsonToken::Number) return false;
    value = tokens[static_cast<size_t>(i++)].number; return true;
}
bool _NT_jsonParse::skipMember() {
    const auto& tokens = static_cast<JsonData*>(refCon)->tokens;
    if (i < 0 || static_cast<size_t>(i) >= tokens.size() || tokens[static_cast<size_t>(i)].kind != JsonToken::Name) return false;
    ++i;
    if (static_cast<size_t>(i) >= tokens.size()) return false;
    i = static_cast<int>(tokens[static_cast<size_t>(i)].end); return true;
}

extern "C" {
const _NT_globals NT_globals = {testSampleRate(), blockCapacity, hostWorkBuffer, sizeof(hostWorkBuffer), 0, 0};
uint8_t NT_screen[128 * 64] = {};
int32_t NT_algorithmIndex(const _NT_algorithm*) { return 0; }
uint32_t NT_parameterOffset() { return parameterOffset; }
void NT_setParameterFromAudio(uint32_t algorithmIndex, uint32_t parameter, int16_t value) { hostSetParameter(algorithmIndex, parameter, value); }
void NT_setParameterFromUi(uint32_t algorithmIndex, uint32_t parameter, int16_t value) { hostSetParameter(algorithmIndex, parameter, value); }
void NT_sendMidiByte(uint32_t destination, uint8_t status) { midiEvents.push_back({destination, status, 0, 0, 1}); }
void NT_sendMidi2ByteMessage(uint32_t destination, uint8_t status, uint8_t data1) { midiEvents.push_back({destination, status, data1, 0, 2}); }
void NT_sendMidi3ByteMessage(uint32_t destination, uint8_t status, uint8_t data1, uint8_t data2) { midiEvents.push_back({destination, status, data1, data2, 3}); }
void NT_drawText(int, int, const char*, int, _NT_textAlignment, _NT_textSize) { ++drawCalls; }
void NT_drawShapeI(_NT_shape, int, int, int, int, int) { ++drawCalls; }
void NT_drawShapeF(_NT_shape, float, float, float, float, float) { ++drawCalls; }
int NT_intToString(char* text, int32_t value) { return std::snprintf(text, 24, "%ld", static_cast<long>(value)); }
}

int main() {
    hostFactory = reinterpret_cast<const _NT_factory*>(pluginEntry(kNT_selector_factoryInfo, 0));
    require(hostFactory && hostFactory->calculateRequirements && hostFactory->construct && hostFactory->step &&
            hostFactory->parameterChanged && hostFactory->midiMessage && hostFactory->midiRealtime && hostFactory->customUi,
            "observable plugin callbacks are available");
    struct Test { const char* name; void (*run)(); };
    const Test tests[] = {
        {"metadata and dirty-memory construction", metadataAndConstruction},
        {"optional ports and highest bus bounds", optionalPortsAndBufferBounds},
        {"Add and Replace outputs", additiveAndReplaceOutputs},
        {"Schmitt noise and held-high rerouting", schmittAndRerouting},
        {"MIDI filtering and velocity", midiFilteringAndVelocity},
        {"MIDI velocity with held CV and pending step actions", midiVelocitySurvivesHeldCvAndStepActions},
        {"CC endpoints and host parameter offset", midiCcEndpointsAndHostOffset},
        {"reset priority and channel panic", resetPriorityAndChannelPanic},
        {"realtime panic and pending UI start", realtimePanicCancelsPendingUi},
        {"MIDI Stop requires Continue or Start", midiStopRequiresContinueOrStart},
        {"MIDI Start does not arm other modes", midiStartDoesNotArmOtherClockModes},
        {"queued MIDI transport ordering and reset priority", midiTransportOrderingAndResetPriority},
        {"Note Off before retrigger and velocity volts", noteRetriggerAndVelocity},
        {"note reconfiguration and disabling", noteReconfigurationAndDisabling},
        {"natural completion and transport", naturalCompletionClosesNotesAndTransport},
        {"RET restarts output MIDI transport", retriggerRestartsMidiTransport},
        {"CC output rate and terminal zero", envelopeCcRateAndTerminalZero},
        {"terminal CC zero bypasses countdown", terminalZeroBypassesCcCountdown},
        {"old CC route returns to zero", ccRouteChangeReturnsOldControllerToZero},
        {"MIDI clock at current sample rate", midiClockUsesHostSampleRate},
        {"dual generators, saturated mix and targeted stop", dualGeneratorsMixAndTargetedStop},
        {"focused controls and disabled G2", focusedUiAndDisabledGenerator},
        {"mute and optional velocity", muteAndOptionalVelocity},
        {"shared CV and input snapshot", sharedCvDepthAndInputSnapshot},
        {"bipolar count and slope capture", bipolarCountAndSlopeCapture},
        {"continuous bipolar time and live level", bipolarContinuousTimeAndLevel},
        {"musical interval CV position law", musicalIntervalCvPositionLaw},
        {"live shape morph and neutral invalid CV", morphShapeCvAndNeutralNonFiniteInput},
        {"shared musical ratios and manual step", musicalRatiosAndManualStep},
        {"musical transport source filtering", musicalTransportFiltersSources},
        {"preset actions and JSON persistence", presetActionsAndJsonRoundTrip},
        {"G2 disable clears outgoing controller", disablingSecondVoiceClearsMidiCc},
        {"sixteen playable performance scenes", performancePackPlaysEveryScene},
        {"performance pack preserves user banks and routing", performancePackPreservesUserMemoryAndRoutes},
    };
    for (const Test& test : tests) { currentTest = test.name; test.run(); }
    std::printf("Plugin integration: %zu tests passed at %u Hz\n", sizeof(tests) / sizeof(tests[0]), NT_globals.sampleRate);
    return EXIT_SUCCESS;
}
