// Windows-only adapter, based on the same host-resolution approach used by
// C:/VS_Code/NT/Grids/emu/nt_emu_bridge.cc. It is never linked into firmware.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include "multi_burst/parameters.h"

extern "C" uintptr_t multiBurstFirmwarePluginEntry(_NT_selector, uint32_t);

namespace {
struct Host {
    const _NT_globals* globals = nullptr;
    decltype(&NT_algorithmIndex) algorithmIndex = nullptr;
    decltype(&NT_parameterOffset) parameterOffset = nullptr;
    decltype(&NT_setParameterFromUi) setParameter = nullptr;
    decltype(&NT_drawText) drawText = nullptr;
    decltype(&NT_drawShapeI) drawShape = nullptr;
    decltype(&NT_intToString) intToString = nullptr;
    decltype(&NT_sendMidiByte) midiByte = nullptr;
    decltype(&NT_sendMidi3ByteMessage) midiMessage = nullptr;
    void (*jsonOpenArray)(void*) = nullptr;
    void (*jsonCloseArray)(void*) = nullptr;
    void (*jsonMember)(void*, const char*) = nullptr;
    void (*jsonNumber)(void*, int) = nullptr;
    bool (*jsonArraySize)(void*, int&) = nullptr;
    bool (*jsonObjectSize)(void*, int&) = nullptr;
    bool (*jsonMatch)(void*, const char*) = nullptr;
    bool (*jsonReadNumber)(void*, int&) = nullptr;
    bool (*jsonSkip)(void*) = nullptr;
};
Host host;
thread_local _NT_algorithm* activeAlgorithm = nullptr;

const _NT_factory* firmwareFactory() {
    return reinterpret_cast<const _NT_factory*>(
        multiBurstFirmwarePluginEntry(kNT_selector_factoryInfo, 0));
}

struct InstanceGuard {
    _NT_algorithm* previous;
    explicit InstanceGuard(_NT_algorithm* algorithm) : previous(activeAlgorithm) {
        activeAlgorithm = algorithm;
    }
    ~InstanceGuard() { activeAlgorithm = previous; }
};

void nativeStep(_NT_algorithm* algorithm, float* buses, int framesBy4) {
    InstanceGuard ownInstance(algorithm);
    firmwareFactory()->step(algorithm, buses, framesBy4);
}
void nativeParameterChanged(_NT_algorithm* algorithm, int parameter) {
    InstanceGuard ownInstance(algorithm);
    firmwareFactory()->parameterChanged(algorithm, parameter);
}
void nativeMidiMessage(_NT_algorithm* algorithm, uint8_t status, uint8_t a, uint8_t b) {
    InstanceGuard ownInstance(algorithm);
    firmwareFactory()->midiMessage(algorithm, status, a, b);
}
void nativeMidiRealtime(_NT_algorithm* algorithm, uint8_t byte) {
    InstanceGuard ownInstance(algorithm);
    firmwareFactory()->midiRealtime(algorithm, byte);
}
void nativeCustomUi(_NT_algorithm* algorithm, const _NT_uiData& data) {
    InstanceGuard ownInstance(algorithm);
    firmwareFactory()->customUi(algorithm, data);
}

template<class T>
bool resolve(HMODULE module, const char* name, T& target) {
    const FARPROC procedure = GetProcAddress(module, name);
    static_assert(sizeof(procedure) == sizeof(target), "Native pointer size mismatch");
    std::memcpy(&target, &procedure, sizeof(procedure));
    return procedure != nullptr;
}

bool bindHost() {
    if (host.globals) return true;
    HMODULE modules[2048];
    DWORD bytes = 0;
    if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytes)
        || bytes > sizeof(modules)) return false;

    for (DWORD index = 0; index < bytes / sizeof(HMODULE); ++index) {
        HMODULE module = modules[index];
        if (!GetProcAddress(module, "getNT_API")) continue;
        Host candidate;
        candidate.globals = reinterpret_cast<const _NT_globals*>(
            GetProcAddress(module, "NT_globals"));
        if (!candidate.globals) continue;
        // The API function table contains parameter stubs in nt_emu 2.0.2.
        // Resolve the real exports, including the host's configured MIDI port.
        if (resolve(module, "NT_algorithmIndex", candidate.algorithmIndex)
            && resolve(module, "NT_parameterOffset", candidate.parameterOffset)
            && resolve(module, "NT_setParameterFromUi", candidate.setParameter)
            && resolve(module, "NT_drawText", candidate.drawText)
            && resolve(module, "NT_drawShapeI", candidate.drawShape)
            && resolve(module, "NT_intToString", candidate.intToString)
            && resolve(module, "NT_sendMidiByte", candidate.midiByte)
            && resolve(module, "NT_sendMidi3ByteMessage", candidate.midiMessage)
            && resolve(module, "_ZN14_NT_jsonStream9openArrayEv", candidate.jsonOpenArray)
            && resolve(module, "_ZN14_NT_jsonStream10closeArrayEv", candidate.jsonCloseArray)
            && resolve(module, "_ZN14_NT_jsonStream13addMemberNameEPKc", candidate.jsonMember)
            && resolve(module, "_ZN14_NT_jsonStream9addNumberEi", candidate.jsonNumber)
            && resolve(module, "_ZN13_NT_jsonParse21numberOfArrayElementsERi", candidate.jsonArraySize)
            && resolve(module, "_ZN13_NT_jsonParse21numberOfObjectMembersERi", candidate.jsonObjectSize)
            && resolve(module, "_ZN13_NT_jsonParse9matchNameEPKc", candidate.jsonMatch)
            && resolve(module, "_ZN13_NT_jsonParse6numberERi", candidate.jsonReadNumber)
            && resolve(module, "_ZN13_NT_jsonParse10skipMemberEv", candidate.jsonSkip)) {
            host = candidate;
            return true;
        }
    }
    return false;
}

void setNativeParameter(uint32_t algorithm, uint32_t parameter, int16_t value) {
    if (activeAlgorithm && activeAlgorithm->v && activeAlgorithm->parameters
        && parameter < multi_burst::ParameterCount) {
        // nt_emu's exported setters use one global current module. Write the
        // callback's own host-backed array and deliver the firmware callback
        // directly. This also keeps
        // MIDI CC and momentary Trigger/Reset/Step isolated across instances.
        const _NT_parameter& definition = activeAlgorithm->parameters[parameter];
        if (value < definition.min) value = definition.min;
        if (value > definition.max) value = definition.max;
        const_cast<int16_t*>(activeAlgorithm->v)[parameter] = value;
        firmwareFactory()->parameterChanged(activeAlgorithm, static_cast<int>(parameter));
        return;
    }
    host.setParameter(algorithm, parameter, value);
}
} // namespace

const _NT_globals& multiBurstHostGlobals() { return *host.globals; }

// The Windows plugin uses the MSVC C++ ABI; nt_emu exports the SDK's Itanium
// method names as C wrappers. Forward to those wrappers on its active JSON
// stream/parser so user banks persist in Rack patches as on the hardware.
void _NT_jsonStream::openArray() { host.jsonOpenArray(this); }
void _NT_jsonStream::closeArray() { host.jsonCloseArray(this); }
void _NT_jsonStream::addMemberName(const char* name) { host.jsonMember(this, name); }
void _NT_jsonStream::addNumber(int value) { host.jsonNumber(this, value); }
bool _NT_jsonParse::numberOfArrayElements(int& count) { return host.jsonArraySize(this, count); }
bool _NT_jsonParse::numberOfObjectMembers(int& count) { return host.jsonObjectSize(this, count); }
bool _NT_jsonParse::matchName(const char* name) { return host.jsonMatch(this, name); }
bool _NT_jsonParse::number(int& value) { return host.jsonReadNumber(this, value); }
bool _NT_jsonParse::skipMember() { return host.jsonSkip(this); }

extern "C" {
int32_t NT_algorithmIndex(const _NT_algorithm* algorithm) {
    return algorithm && algorithm == activeAlgorithm ? 0 : host.algorithmIndex(algorithm);
}
uint32_t NT_parameterOffset() { return host.parameterOffset(); }
void NT_setParameterFromAudio(uint32_t algorithm, uint32_t parameter, int16_t value) {
    setNativeParameter(algorithm, parameter, value);
}
void NT_setParameterFromUi(uint32_t algorithm, uint32_t parameter, int16_t value) {
    setNativeParameter(algorithm, parameter, value);
}
void NT_drawText(int x, int y, const char* text, int colour,
                 _NT_textAlignment alignment, _NT_textSize size) {
    host.drawText(x, y, text, colour, alignment, size);
}
void NT_drawShapeI(_NT_shape shape, int x0, int y0, int x1, int y1, int colour) {
    host.drawShape(shape, x0, y0, x1, y1, colour);
}
int NT_intToString(char* buffer, int32_t value) { return host.intToString(buffer, value); }
void NT_sendMidiByte(uint32_t destination, uint8_t byte) { host.midiByte(destination, byte); }
void NT_sendMidi3ByteMessage(uint32_t destination, uint8_t status, uint8_t a, uint8_t b) {
    host.midiMessage(destination, status, a, b);
}

__declspec(dllexport) uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    if (selector == kNT_selector_factoryInfo) {
        if (data != 0 || !bindHost()) return 0;
        static _NT_factory nativeFactory = [] {
            _NT_factory factory = *firmwareFactory();
            factory.step = nativeStep;
            factory.parameterChanged = nativeParameterChanged;
            factory.midiMessage = nativeMidiMessage;
            factory.midiRealtime = nativeMidiRealtime;
            factory.customUi = nativeCustomUi;
            return factory;
        }();
        return reinterpret_cast<uintptr_t>(&nativeFactory);
    }
    return multiBurstFirmwarePluginEntry(selector, data);
}
}
