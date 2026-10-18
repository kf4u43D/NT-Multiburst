#include <cstddef>
#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include "multi_burst/api_contract.h"
#include "multi_burst/engine.h"
#include "multi_burst/parameters.h"
#include "multi_burst/presets.h"
#include "multi_burst/performance_presets.h"
#include <atomic>
#include <new>

namespace {
using namespace multi_burst;
static_assert(std::atomic<uint32_t>::is_always_lock_free, "Audio/UI atomics must be lock-free");
constexpr uint32_t triggerFlag=1, resetFlag=2, stepFlag=4, stopFlag=8, loadFlag=16, saveFlag=32, factoryFlag=64, packFlag=128;
constexpr const char* shapes[]={"Sine","Log Down","Log Up","Expo Down","Ramp Up","Triangle","Ramp Down","Pulse"};
constexpr const char* clockModes[]={"Internal rate","Internal BPM","CV","MIDI","Internal + CV"};
constexpr const char* divisions[]={"1/32","1/16","1/8","1/4","1/2","1 bar","2 bars"};
constexpr const char* ppqns[]={"1","2","4","8","12","24","48"};
constexpr const char* retriggers[]={"Restart (RET)","Ignore (ONE)"};
constexpr const char* firstBursts[]={"Immediate","On clock"};
constexpr const char* onOff[]={"Off","On"};
constexpr const char* destinations[]={"Off","Breakout","Select Bus","Breakout + SB","USB","USB + Breakout","USB + SB","USB + BO + SB"};
constexpr const char* sources[]={"Internal BPM","CV","MIDI","Auto"};
constexpr const char* timingModes[]={"Absolute","Musical"};
constexpr const char* cvResponses[]={"DROID reference","Bipolar additive"};
constexpr const char* shapeResponses[]={"Legacy","DROID","Morph"};
constexpr const char* focusNames[]={"G1","G2"};
constexpr const char* ratios[]={"1/16","1/12","1/8","1/6","1/4","1/3","1/2","2/3","1","3/2","2","3","4","6","8","16"};
constexpr const char* optionalRatios[]={"Off","1/16","1/12","1/8","1/6","1/4","1/3","1/2","2/3","1","3/2","2","3","4","6","8","16"};
#define PARAM(n,lo,hi,d,u,sc,es) {n,lo,hi,d,u,sc,es}
#define SWITCH(n,d) PARAM(n,0,1,d,kNT_unitEnum,0,onOff)
#define PERCENT(n,d) PARAM(n,0,100,d,kNT_unitPercent,0,nullptr)
#define FEATURES(p) \
    PARAM(p "Timing",0,1,0,kNT_unitEnum,0,timingModes), \
    PARAM(p "Width ratio",0,15,3,kNT_unitEnum,0,ratios), \
    PARAM(p "Interval ratio",0,16,6,kNT_unitEnum,0,optionalRatios), \
    PARAM(p "Glide rise",0,30000,0,kNT_unitMs,0,nullptr), \
    PARAM(p "Glide fall",0,30000,0,kNT_unitMs,0,nullptr), \
    PARAM(p "Glide rise ratio",0,16,0,kNT_unitEnum,0,optionalRatios), \
    PARAM(p "Glide fall ratio",0,16,0,kNT_unitEnum,0,optionalRatios), \
    SWITCH(p "Loop",0), SWITCH(p "Mute",0), SWITCH(p "MIDI velocity",1), \
    PERCENT(p "Level trim",100), SWITCH(p "CV enable",1), PERCENT(p "CV depth",100), \
    PERCENT(p "Count CV depth",100), PERCENT(p "Time CV depth",100), \
    PERCENT(p "Shape CV depth",100), PERCENT(p "Slope CV depth",100), PARAM(p "Shape response",0,2,0,kNT_unitEnum,0,shapeResponses)
#define PRESETS(p) PARAM(p "Preset",1,16,1,kNT_unitNone,0,nullptr), SWITCH(p "Load preset",0), SWITCH(p "Save preset",0), SWITCH(p "Factory preset",0)
const _NT_parameter parameters[]={
    PARAM("Bursts",1,40,8,kNT_unitNone,0,nullptr),
    PARAM("Shape",0,7,0,kNT_unitEnum,0,shapes),
    PARAM("Width",20,2500,100,kNT_unitMs,0,nullptr),
    PARAM("Slope",-100,100,0,kNT_unitPercent,0,nullptr),
    PARAM("Level",0,100,80,kNT_unitVolts,kNT_scaling10,nullptr),
    PARAM("Rate",0,5100,400,kNT_unitHz,kNT_scaling100,nullptr),
    PARAM("Tempo",200,3000,1200,kNT_unitBPM,kNT_scaling10,nullptr),
    PARAM("Clock source",0,4,0,kNT_unitEnum,0,clockModes),
    PARAM("Burst division",0,6,1,kNT_unitEnum,0,divisions),
    PARAM("CV divider",1,64,1,kNT_unitNone,0,nullptr),
    PARAM("CV clock PPQN",0,6,0,kNT_unitEnum,0,ppqns),
    PARAM("Retrigger",0,1,0,kNT_unitEnum,0,retriggers),
    PARAM("First burst",0,1,0,kNT_unitEnum,0,firstBursts),
    PARAM("Pulse width",1,20,5,kNT_unitMs,0,nullptr),
    NT_PARAMETER_CV_INPUT("Trigger input",0,1)
    NT_PARAMETER_CV_INPUT("Clock input",0,2)
    NT_PARAMETER_CV_INPUT("Reset input",0,3)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Envelope output",0,13)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Gate output",0,14)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Active output",0,15)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("EOC output",0,16)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Clock output",0,17)
    PARAM("MIDI input channel",0,17,1,kNT_unitHasStrings,0,nullptr),
    PARAM("MIDI trigger note",0,127,60,kNT_unitMIDINote,0,nullptr),
    PARAM("MIDI reset note",0,127,61,kNT_unitMIDINote,0,nullptr),
    PARAM("MIDI step note",0,127,62,kNT_unitMIDINote,0,nullptr),
    PARAM("MIDI destination",0,7,0,kNT_unitEnum,0,destinations),
    PARAM("MIDI output channel",1,16,1,kNT_unitNone,0,nullptr),
    PARAM("MIDI output note",0,127,60,kNT_unitMIDINote,0,nullptr),
    SWITCH("MIDI notes",0),
    PARAM("Envelope MIDI CC",-1,119,-1,kNT_unitHasStrings,0,nullptr),
    PARAM("MIDI CC rate",1,100,50,kNT_unitHz,0,nullptr),
    SWITCH("MIDI clock output",0), SWITCH("MIDI transport out",0),
    SWITCH("Trigger",0), SWITCH("Reset",0), SWITCH("Step",0),
    SWITCH("G2 enabled",0),
    PARAM("Focus",0,1,0,kNT_unitEnum,0,focusNames),
    PARAM("Musical clock",0,3,0,kNT_unitEnum,0,sources),
    FEATURES("G1 "),
    NT_PARAMETER_CV_INPUT("G1 stop input",0,0)
    NT_PARAMETER_CV_INPUT("G1 step input",0,0)
    SWITCH("G1 stop",0),
    PARAM("G1 MIDI stop note",0,127,63,kNT_unitMIDINote,0,nullptr),
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G1 levels output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G1 progress output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G1 burst output",0,0)
    PRESETS("G1 "),
    PARAM("G2 bursts",1,40,5,kNT_unitNone,0,nullptr),
    PARAM("G2 shape",0,7,0,kNT_unitEnum,0,shapes),
    PARAM("G2 width",20,2500,100,kNT_unitMs,0,nullptr),
    PARAM("G2 slope",-100,100,0,kNT_unitPercent,0,nullptr),
    PARAM("G2 level",0,100,80,kNT_unitVolts,kNT_scaling10,nullptr),
    PARAM("G2 rate",0,5100,400,kNT_unitHz,kNT_scaling100,nullptr),
    PARAM("G2 burst division",0,6,1,kNT_unitEnum,0,divisions),
    PARAM("G2 CV divider",1,64,1,kNT_unitNone,0,nullptr),
    PARAM("G2 retrigger",0,1,0,kNT_unitEnum,0,retriggers),
    PARAM("G2 first burst",0,1,0,kNT_unitEnum,0,firstBursts),
    FEATURES("G2 "),
    NT_PARAMETER_CV_INPUT("G2 trigger input",0,0)
    NT_PARAMETER_CV_INPUT("G2 reset input",0,0)
    NT_PARAMETER_CV_INPUT("G2 stop input",0,0)
    NT_PARAMETER_CV_INPUT("G2 step input",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 envelope output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 gate output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 active output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 EOC output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 clock output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 levels output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 progress output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("G2 burst output",0,0)
    PARAM("G2 MIDI input channel",0,17,1,kNT_unitHasStrings,0,nullptr),
    PARAM("G2 MIDI trigger note",0,127,64,kNT_unitMIDINote,0,nullptr),
    PARAM("G2 MIDI reset note",0,127,65,kNT_unitMIDINote,0,nullptr),
    PARAM("G2 MIDI step note",0,127,66,kNT_unitMIDINote,0,nullptr),
    PARAM("G2 MIDI stop note",0,127,67,kNT_unitMIDINote,0,nullptr),
    PARAM("G2 MIDI destination",0,7,0,kNT_unitEnum,0,destinations),
    PARAM("G2 MIDI output channel",1,16,2,kNT_unitNone,0,nullptr),
    PARAM("G2 MIDI output note",0,127,64,kNT_unitMIDINote,0,nullptr),
    SWITCH("G2 MIDI notes",0),
    PARAM("G2 envelope MIDI CC",-1,119,-1,kNT_unitHasStrings,0,nullptr),
    PARAM("G2 MIDI CC rate",1,100,50,kNT_unitHz,0,nullptr),
    SWITCH("G2 trigger",0), SWITCH("G2 reset",0), SWITCH("G2 step",0), SWITCH("G2 stop",0),
    PRESETS("G2 "),
    NT_PARAMETER_CV_INPUT("Count CV input",0,0)
    NT_PARAMETER_CV_INPUT("Width CV input",0,0)
    NT_PARAMETER_CV_INPUT("Interval CV input",0,0)
    NT_PARAMETER_CV_INPUT("Slope CV input",0,0)
    NT_PARAMETER_CV_INPUT("Shape CV input",0,0)
    NT_PARAMETER_CV_INPUT("Amplitude CV input",0,0)
    NT_PARAMETER_CV_INPUT("Trigger all input",0,0)
    NT_PARAMETER_CV_INPUT("Reset all input",0,0)
    NT_PARAMETER_CV_INPUT("Stop all input",0,0)
    NT_PARAMETER_CV_INPUT("Step all input",0,0)
    SWITCH("Trigger all",0), SWITCH("Reset all",0), SWITCH("Stop all",0), SWITCH("Step all",0),
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Envelope mix output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Busy all output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Reference clock output",0,0)
    NT_PARAMETER_CV_OUTPUT_WITH_MODE("Focus progress output",0,0)
    PARAM("CV response",0,1,0,kNT_unitEnum,0,cvResponses),
    PARAM("Pack preset",0,15,0,kNT_unitEnum,0,performancePresetNames),
    SWITCH("Load pack preset",0),
};
#undef PRESETS
#undef FEATURES
#undef PERCENT
#undef SWITCH
#undef PARAM
static_assert(ARRAY_SIZE(parameters)==ParameterCount,"Parameter order mismatch");

enum VoiceParameter : uint8_t {
    VBursts,
    VEnvelopeShape,
    VWidth,
    VSlope,
    VLevel,
    VRate,
    VDivision,
    VCvDivider,
    VRetriggerMode,
    VFirstBurst,
    VTimingMode,
    VWidthRatio,
    VIntervalRatio,
    VGlideRise,
    VGlideFall,
    VGlideRiseRatio,
    VGlideFallRatio,
    VLoop,
    VMute,
    VVelocityEnabled,
    VLevelTrim,
    VCvEnabled,
    VCvDepth,
    VCountCvDepth,
    VTimeCvDepth,
    VShapeCvDepth,
    VSlopeCvDepth,
    VReferenceCurves,
    VTriggerInput,
    VResetInput,
    VStopInput,
    VStepInput,
    VEnvelopeOutput,
    VGateOutput,
    VActiveOutput,
    VEocOutput,
    VClockOutput,
    VLevelsOutput,
    VProgressOutput,
    VBurstOutput,
    VMidiInputChannel,
    VMidiTriggerNote,
    VMidiResetNote,
    VMidiStepNote,
    VMidiStopNote,
    VMidiDestination,
    VMidiOutputChannel,
    VMidiOutputNote,
    VMidiNotes,
    VMidiEnvelopeCc,
    VMidiCcRate,
    VManualTrigger,
    VManualReset,
    VManualStep,
    VManualStop,
    VPresetSlot,
    VPresetLoad,
    VPresetSave,
    VPresetFactory, VoiceParameterCount
};
constexpr Parameter voiceMap[2][VoiceParameterCount]={
    {Bursts,EnvelopeShape,Width,Slope,Level,Rate,Division,CvDivider,RetriggerMode,FirstBurst,TimingMode,WidthRatio,IntervalRatio,GlideRise,GlideFall,GlideRiseRatio,GlideFallRatio,Loop,Mute,VelocityEnabled,LevelTrim,CvEnabled,CvDepth,CountCvDepth,TimeCvDepth,ShapeCvDepth,SlopeCvDepth,ReferenceCurves,TriggerInput,ResetInput,StopInput,StepInput,EnvelopeOutput,GateOutput,ActiveOutput,EocOutput,ClockOutput,LevelsOutput,ProgressOutput,BurstOutput,MidiInputChannel,MidiTriggerNote,MidiResetNote,MidiStepNote,MidiStopNote,MidiDestination,MidiOutputChannel,MidiOutputNote,MidiNotes,MidiEnvelopeCc,MidiCcRate,ManualTrigger,ManualReset,ManualStep,ManualStop,PresetSlot,PresetLoad,PresetSave,PresetFactory},
    {G2Bursts,G2EnvelopeShape,G2Width,G2Slope,G2Level,G2Rate,G2Division,G2CvDivider,G2RetriggerMode,G2FirstBurst,G2TimingMode,G2WidthRatio,G2IntervalRatio,G2GlideRise,G2GlideFall,G2GlideRiseRatio,G2GlideFallRatio,G2Loop,G2Mute,G2VelocityEnabled,G2LevelTrim,G2CvEnabled,G2CvDepth,G2CountCvDepth,G2TimeCvDepth,G2ShapeCvDepth,G2SlopeCvDepth,G2ReferenceCurves,G2TriggerInput,G2ResetInput,G2StopInput,G2StepInput,G2EnvelopeOutput,G2GateOutput,G2ActiveOutput,G2EocOutput,G2ClockOutput,G2LevelsOutput,G2ProgressOutput,G2BurstOutput,G2MidiInputChannel,G2MidiTriggerNote,G2MidiResetNote,G2MidiStepNote,G2MidiStopNote,G2MidiDestination,G2MidiOutputChannel,G2MidiOutputNote,G2MidiNotes,G2MidiEnvelopeCc,G2MidiCcRate,G2ManualTrigger,G2ManualReset,G2ManualStep,G2ManualStop,G2PresetSlot,G2PresetLoad,G2PresetSave,G2PresetFactory}
};
const uint8_t page0[]={Bursts,EnvelopeShape,Width,Slope,Level,RetriggerMode,FirstBurst,Loop,Mute,VelocityEnabled,LevelTrim,ReferenceCurves,ManualTrigger,ManualReset,ManualStep,ManualStop};
const uint8_t page1[]={ClockSource,Rate,Tempo,Division,CvDivider,CvPpqn,PulseWidth,SharedClockSource};
const uint8_t page2[]={TimingMode,WidthRatio,IntervalRatio,GlideRise,GlideFall,GlideRiseRatio,GlideFallRatio};
const uint8_t page3[]={TriggerInput,ResetInput,StopInput,StepInput,CvEnabled,CvDepth,CountCvDepth,TimeCvDepth,ShapeCvDepth,SlopeCvDepth};
const uint8_t page4[]={EnvelopeOutput,EnvelopeOutputMode,GateOutput,GateOutputMode,ActiveOutput,ActiveOutputMode,EocOutput,EocOutputMode,ClockOutput,ClockOutputMode,LevelsOutput,LevelsOutputMode,ProgressOutput,ProgressOutputMode,BurstOutput,BurstOutputMode};
const uint8_t page5[]={MidiInputChannel,MidiTriggerNote,MidiResetNote,MidiStepNote,MidiStopNote};
const uint8_t page6[]={MidiDestination,MidiOutputChannel,MidiOutputNote,MidiNotes,MidiEnvelopeCc,MidiCcRate,MidiClockOutput,MidiTransportOutput};
const uint8_t page7[]={PresetSlot,PresetLoad,PresetSave,PresetFactory};
const uint8_t page8[]={G2Bursts,G2EnvelopeShape,G2Width,G2Slope,G2Level,G2Rate,G2RetriggerMode,G2FirstBurst,G2Loop,G2Mute,G2VelocityEnabled,G2LevelTrim,G2ReferenceCurves,G2ManualTrigger,G2ManualReset,G2ManualStep,G2ManualStop};
const uint8_t page9[]={G2TimingMode,G2WidthRatio,G2IntervalRatio,G2GlideRise,G2GlideFall,G2GlideRiseRatio,G2GlideFallRatio,G2Division,G2CvDivider};
const uint8_t page10[]={G2TriggerInput,G2ResetInput,G2StopInput,G2StepInput,G2CvEnabled,G2CvDepth,G2CountCvDepth,G2TimeCvDepth,G2ShapeCvDepth,G2SlopeCvDepth};
const uint8_t page11[]={G2EnvelopeOutput,G2EnvelopeOutputMode,G2GateOutput,G2GateOutputMode,G2ActiveOutput,G2ActiveOutputMode,G2EocOutput,G2EocOutputMode,G2ClockOutput,G2ClockOutputMode,G2LevelsOutput,G2LevelsOutputMode,G2ProgressOutput,G2ProgressOutputMode,G2BurstOutput,G2BurstOutputMode};
const uint8_t page12[]={G2MidiInputChannel,G2MidiTriggerNote,G2MidiResetNote,G2MidiStepNote,G2MidiStopNote};
const uint8_t page13[]={G2MidiDestination,G2MidiOutputChannel,G2MidiOutputNote,G2MidiNotes,G2MidiEnvelopeCc,G2MidiCcRate};
const uint8_t page14[]={G2PresetSlot,G2PresetLoad,G2PresetSave,G2PresetFactory};
const uint8_t page15[]={ClockInput,CountCvInput,WidthCvInput,IntervalCvInput,SlopeCvInput,ShapeCvInput,AmplitudeCvInput,TriggerAllInput,ResetAllInput,StopAllInput,StepAllInput,CvResponse};
const uint8_t page16[]={Generator2Enabled,FocusGenerator,ManualTriggerAll,ManualResetAll,ManualStopAll,ManualStepAll};
const uint8_t page17[]={MixOutput,MixOutputMode,BusyAllOutput,BusyAllOutputMode,ReferenceClockOutput,ReferenceClockOutputMode,FocusProgressOutput,FocusProgressOutputMode};
const uint8_t page18[]={PackPreset,LoadPackPreset};
#define PAGE(n,a) {n,static_cast<uint8_t>(ARRAY_SIZE(a)),0,{0,0},a}
const _NT_parameterPage pages[]={
    PAGE("G1 envelope",page0),
    PAGE("Clock",page1),
    PAGE("G1 timing",page2),
    PAGE("G1 CV",page3),
    PAGE("G1 outputs",page4),
    PAGE("G1 MIDI input",page5),
    PAGE("MIDI output",page6),
    PAGE("G1 presets",page7),
    PAGE("G2 envelope",page8),
    PAGE("G2 timing",page9),
    PAGE("G2 CV",page10),
    PAGE("G2 outputs",page11),
    PAGE("G2 MIDI input",page12),
    PAGE("G2 MIDI output",page13),
    PAGE("G2 presets",page14),
    PAGE("Shared inputs",page15),
    PAGE("Performance",page16),
    PAGE("Shared outputs",page17),
    PAGE("Preset pack",page18)
};
#undef PAGE
const _NT_parameterPages parameterPages={ARRAY_SIZE(pages),pages};

int bounded(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
float boundedFloat(float v,float lo,float hi){return v!=v?lo:v<lo?lo:v>hi?hi:v;}
int rounded(float v){return static_cast<int>(v+0.5f);}
struct Schmitt {
    bool high=false; int bus=-1;
    bool edge(float value,int newBus) {
        if(newBus!=bus){bus=newBus;high=value>=1.0f;return false;}
        if(high){if(value<=0.2f)high=false;return false;}
        if(value>=1.0f){high=true;return true;} return false;
    }
};
struct Voice {
    Engine engine{};
    MusicalTiming timing{};
    std::atomic<uint32_t> pending{0},velocity{127};
    Schmitt inputs[4];
    uint32_t ccCountdown=0;
    int lastCc=-1,previousDest=0,previousChannel=1,previousCc=-1;
    int noteDest=0,noteChannel=1,noteNumber=60;
    bool noteOn=false;
    std::atomic<uint32_t> latestMillivolts{0},latestIndex{0},latestActive{0};
};
struct PresetBinding {int16_t Preset::*field; VoiceParameter parameter; int offset;};
constexpr PresetBinding presetBindings[]={
    {&Preset::bursts,VBursts,0},{&Preset::shape,VEnvelopeShape,0},{&Preset::slope,VSlope,0},
    {&Preset::level,VLevel,0},{&Preset::retrigger,VRetriggerMode,0},{&Preset::loop,VLoop,0},
    {&Preset::mute,VMute,0},{&Preset::widthRatio,VWidthRatio,0},{&Preset::intervalRatio,VIntervalRatio,1},
    {&Preset::glideUp,VGlideRiseRatio,0},{&Preset::glideDown,VGlideFallRatio,0},
    {&Preset::cvEnabled,VCvEnabled,0},{&Preset::velocityEnabled,VVelocityEnabled,0},
    {&Preset::cvDepth,VCvDepth,0},{&Preset::levelTrim,VLevelTrim,0},
    {&Preset::countDepth,VCountCvDepth,0},{&Preset::timeDepth,VTimeCvDepth,0},
    {&Preset::shapeDepth,VShapeCvDepth,0},{&Preset::slopeDepth,VSlopeCvDepth,0},
    {&Preset::widthMilliseconds,VWidth,0},{&Preset::rateCentihz,VRate,0},
    {&Preset::timingMode,VTimingMode,0},{&Preset::referenceCurves,VReferenceCurves,0},
    {&Preset::glideRiseMs,VGlideRise,0},{&Preset::glideFallMs,VGlideFall,0}
};
// Atomic fixed storage also permits preset serialisation while audio is running.
struct SavedSlot {
    std::atomic<uint32_t> version{0};
    std::atomic<uint32_t> fields[ARRAY_SIZE(presetBindings)]{};
};
struct Algorithm : _NT_algorithm {
    Voice voices[2];
    MusicalClock reference{};
    Schmitt clock,allInputs[4];
    std::atomic<uint32_t> allPending{0};
    std::atomic<uint32_t> savedMasks[2]{};
    SavedSlot slots[2][16];
    uint32_t sampleRate=0,referenceRemaining=0;
    bool transportOn=false;
    int transportDest=0;
};
int value(const Algorithm& a,unsigned g,VoiceParameter p){return a.v[voiceMap[g][p]];}
bool enabled(const Algorithm& a,unsigned g){return g==0||a.v[Generator2Enabled]!=0;}
void setParameter(Algorithm& a,Parameter p,int v){
    const int index=NT_algorithmIndex(&a);
    if(index>=0)NT_setParameterFromAudio(static_cast<uint32_t>(index),static_cast<uint32_t>(p)+NT_parameterOffset(),static_cast<int16_t>(v));
}
void setVoiceParameter(Algorithm& a,unsigned g,VoiceParameter p,int v){setParameter(a,voiceMap[g][p],v);}
void noteOff(Voice& v){
    if(v.noteOn)NT_sendMidi3ByteMessage(static_cast<uint32_t>(v.noteDest),static_cast<uint8_t>(0x80|(v.noteChannel-1)),static_cast<uint8_t>(v.noteNumber),0);
    v.noteOn=false;
}
void transportStop(Algorithm& a){
    if(a.transportOn)NT_sendMidiByte(static_cast<uint32_t>(a.transportDest),0xFC);
    a.transportOn=false;
}
void queue(Voice& v,uint32_t flags,uint32_t velocity=127){
    if(flags&triggerFlag)v.velocity.store(velocity,std::memory_order_relaxed);
    v.pending.fetch_or(flags,std::memory_order_release);
}
void saveSlot(Algorithm& a,unsigned g,unsigned slot,const Preset& source){
    const Preset p=boundedPreset(source);
    auto& saved=a.slots[g][slot];
    saved.version.fetch_add(1,std::memory_order_seq_cst);
    for(unsigned i=0;i<ARRAY_SIZE(presetBindings);++i)
        saved.fields[i].store(static_cast<uint16_t>(p.*(presetBindings[i].field)),std::memory_order_seq_cst);
    saved.version.fetch_add(1,std::memory_order_seq_cst);
    a.savedMasks[g].fetch_or(uint32_t{1}<<slot,std::memory_order_seq_cst);
}
bool tryLoadSlot(const Algorithm& a,unsigned g,unsigned slot,Preset& result){
    if(!(a.savedMasks[g].load(std::memory_order_seq_cst)&(uint32_t{1}<<slot))){result=factoryPreset(g,slot);return true;}
    const auto& saved=a.slots[g][slot];
    for(unsigned attempt=0;attempt<4;++attempt){
        const uint32_t before=saved.version.load(std::memory_order_seq_cst);
        if(before&1)continue;
        Preset p;
        for(unsigned i=0;i<ARRAY_SIZE(presetBindings);++i)
            p.*(presetBindings[i].field)=static_cast<int16_t>(saved.fields[i].load(std::memory_order_seq_cst));
        if(before==saved.version.load(std::memory_order_seq_cst)){result=boundedPreset(p);return true;}
    }
    return false;
}
Preset capturePreset(const Algorithm& a,unsigned g){
    Preset p;
    for(const auto& b:presetBindings)p.*(b.field)=static_cast<int16_t>(value(a,g,b.parameter)-b.offset);
    return boundedPreset(p);
}
void applyPreset(Algorithm& a,unsigned g,const Preset& p){
    for(const auto& b:presetBindings)setVoiceParameter(a,g,b.parameter,p.*(b.field)+b.offset);
}
void applyPerformancePreset(Algorithm& a,unsigned slot){
    const PerformancePreset scene=performancePreset(slot);
    for(unsigned g=0;g<2;++g){
        applyPreset(a,g,scene.voices[g]);
        setVoiceParameter(a,g,VDivision,scene.division[g]);
        setVoiceParameter(a,g,VCvDivider,scene.cvDivider[g]);
        setVoiceParameter(a,g,VFirstBurst,scene.firstBurst[g]);
    }
    setParameter(a,ClockSource,scene.clockSource);
    setParameter(a,SharedClockSource,scene.sharedClockSource);
    setParameter(a,Tempo,scene.tempo);
    setParameter(a,CvResponse,scene.cvResponse);
    setParameter(a,Generator2Enabled,scene.generator2Enabled);
    setParameter(a,CvPpqn,scene.cvPpqn);
    setParameter(a,PulseWidth,scene.pulseWidth);
}
void calculateRequirements(_NT_algorithmRequirements& req,const int32_t*){
    req={};req.numParameters=ParameterCount;req.dtc=sizeof(Algorithm);
}
_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,const _NT_algorithmRequirements&,const int32_t*){
    auto* a=new(ptrs.dtc)Algorithm{};
    a->parameters=parameters;a->parameterPages=&parameterPages;return a;
}
void parameterChanged(_NT_algorithm* self,int p){
    auto& a=*static_cast<Algorithm*>(self);
    if(!a.v||p<0||p>=ParameterCount||!a.v[p])return;
    if(p==LoadPackPreset){
        a.allPending.fetch_or(packFlag,std::memory_order_release);
        setParameter(a,LoadPackPreset,0);
        return;
    }
    constexpr VoiceParameter actions[]={VManualTrigger,VManualReset,VManualStep,VManualStop,VPresetLoad,VPresetSave,VPresetFactory};
    constexpr uint32_t flags[]={triggerFlag,resetFlag,stepFlag,stopFlag,loadFlag,saveFlag,factoryFlag};
    for(unsigned g=0;g<2;++g)for(unsigned i=0;i<ARRAY_SIZE(actions);++i)if(p==voiceMap[g][actions[i]]){
        queue(a.voices[g],flags[i]);setParameter(a,static_cast<Parameter>(p),0);return;
    }
    const uint32_t all=p==ManualTriggerAll?triggerFlag:p==ManualResetAll?resetFlag:p==ManualStepAll?stepFlag:p==ManualStopAll?stopFlag:0;
    if(all){a.allPending.fetch_or(all,std::memory_order_release);setParameter(a,static_cast<Parameter>(p),0);}
}
void midiMessage(_NT_algorithm* self,uint8_t status,uint8_t data1,uint8_t data2){
    auto& a=*static_cast<Algorithm*>(self); data1&=127;data2&=127;
    for(unsigned g=0;g<2;++g){
        if(!enabled(a,g))continue;
        Voice& v=a.voices[g];const int channel=value(a,g,VMidiInputChannel);
        if(channel==0||(channel!=17&&channel!=(status&15)+1))continue;
        if((status&0xF0)==0x90&&data2){
            if(data1==value(a,g,VMidiResetNote))queue(v,resetFlag);
            else if(data1==value(a,g,VMidiStopNote))queue(v,stopFlag);
            else if(data1==value(a,g,VMidiTriggerNote))queue(v,triggerFlag,data2);
            else if(data1==value(a,g,VMidiStepNote))queue(v,stepFlag);
        }else if((status&0xF0)==0xB0){
            const int ccBase=g==0?20:30;
            const int control=static_cast<int>(data1)-ccBase;
            switch(control){
            case 0:setVoiceParameter(a,g,VBursts,1+(data2*39+63)/127);break;
            case 1:setVoiceParameter(a,g,VEnvelopeShape,(data2*7+63)/127);break;
            case 2:setVoiceParameter(a,g,VWidth,20+(data2*2480+63)/127);break;
            case 3:setVoiceParameter(a,g,VRate,data2==0?0:24+((data2-1)*5076+63)/126);break;
            case 4:setVoiceParameter(a,g,VSlope,-100+(data2*200+63)/127);break;
            case 5:setVoiceParameter(a,g,VLevel,(data2*100+63)/127);break;
            default:break;
            }
            if(g==0&&data1==26)setParameter(a,Tempo,200+(data2*2800+63)/127);
            if(g==0&&data2>=64){
                if(data1==115)queue(v,triggerFlag);
                if(data1==116)queue(v,resetFlag);
                if(data1==117)queue(v,stepFlag);
                if(data1==118)queue(v,stopFlag);
            }
            if(g==1&&data2>=64){
                if(data1==110)queue(v,triggerFlag);
                if(data1==111)queue(v,resetFlag);
                if(data1==112)queue(v,stepFlag);
                if(data1==113)queue(v,stopFlag);
            }
            if(data1==120||data1==123)queue(v,resetFlag);
        }
    }
}
void midiRealtime(_NT_algorithm* self,uint8_t byte){
    auto& a=*static_cast<Algorithm*>(self);
    a.reference.ingestMidiRealtime(byte);
    for(unsigned g=0;g<2;++g){
        auto& v=a.voices[g];
        const bool absolute=value(a,g,VTimingMode)==0;
        if(absolute)v.engine.ingestMidiRealtime(byte);
        if((byte==0xFC&&absolute)||byte==0xFF){v.pending.store(byte==0xFF?resetFlag:0,std::memory_order_release);noteOff(v);}
    }
    if(byte==0xFF){a.allPending.store(resetFlag,std::memory_order_release);transportStop(a);}
}
const float* input(const Algorithm& a,Parameter p,float* buses,int frames){
    const int bus=a.v[p];return bus>=1&&bus<=kNT_lastBus?buses+static_cast<std::ptrdiff_t>(bus-1)*frames:nullptr;
}
struct Output{float* ptr;bool replace;};
Output output(const Algorithm& a,Parameter p,float* buses,int frames){
    const int bus=a.v[p];return {bus>=1&&bus<=kNT_lastBus?buses+static_cast<std::ptrdiff_t>(bus-1)*frames:nullptr,a.v[p+1]!=0};
}
void write(Output out,int frame,float v){if(out.ptr){if(out.replace)out.ptr[frame]=v;else out.ptr[frame]+=v;}}
struct CvSample{float volts[6];bool present[6];};
float modulated(float base,float target,float depth){return base+(target-base)*depth;}
Config config(const Algorithm& a,unsigned g,const ReferenceSample& reference,const CvSample& cv,uint8_t& interval){
    Config c;
    c.bursts=static_cast<uint8_t>(bounded(value(a,g,VBursts),1,40));
    c.shape=static_cast<Shape>(bounded(value(a,g,VEnvelopeShape),0,7));
    c.widthSeconds=static_cast<float>(bounded(value(a,g,VWidth),20,2500))*0.001f;
    c.slope=static_cast<float>(bounded(value(a,g,VSlope),-100,100))*0.01f;
    c.level=static_cast<float>(bounded(value(a,g,VLevel),0,100))*0.1f;
    c.rateHz=static_cast<float>(bounded(value(a,g,VRate),0,5100))*0.01f;
    c.bpm=static_cast<float>(bounded(a.v[Tempo],200,3000))*0.1f;
    c.clockMode=static_cast<Config::ClockMode>(bounded(a.v[ClockSource],0,4));
    c.midiTicksPerBurst=divisionTicks[bounded(value(a,g,VDivision),0,6)];
    c.cvDivider=static_cast<uint8_t>(bounded(value(a,g,VCvDivider),1,64));
    c.cvPpqn=ppqnValues[bounded(a.v[CvPpqn],0,6)];
    c.retrigger=static_cast<Config::Retrigger>(bounded(value(a,g,VRetriggerMode),0,1));
    c.waitForClock=value(a,g,VFirstBurst)!=0;
    c.pulseSeconds=static_cast<float>(bounded(a.v[PulseWidth],1,20))*0.001f;
    c.loop=value(a,g,VLoop)!=0;c.mute=value(a,g,VMute)!=0;
    c.velocityEnabled=value(a,g,VVelocityEnabled)!=0;
    const int shapeResponse=bounded(value(a,g,VReferenceCurves),0,2);
    c.referenceCurves=shapeResponse!=0;
    c.levelTrim=static_cast<float>(bounded(value(a,g,VLevelTrim),0,100))*0.01f;
    c.glideRiseSeconds=static_cast<float>(bounded(value(a,g,VGlideRise),0,30000))*0.001f;
    c.glideFallSeconds=static_cast<float>(bounded(value(a,g,VGlideFall),0,30000))*0.001f;
    const bool musical=value(a,g,VTimingMode)!=0;
    int width=bounded(value(a,g,VWidthRatio),0,15);
    interval=static_cast<uint8_t>(bounded(value(a,g,VIntervalRatio),0,16));
    const float depth=value(a,g,VCvEnabled)?static_cast<float>(bounded(value(a,g,VCvDepth),0,100))*0.01f:0;
    const float countDepth=depth*static_cast<float>(bounded(value(a,g,VCountCvDepth),0,100))*0.01f;
    const float timeDepth=depth*static_cast<float>(bounded(value(a,g,VTimeCvDepth),0,100))*0.01f;
    const float shapeDepth=depth*static_cast<float>(bounded(value(a,g,VShapeCvDepth),0,100))*0.01f;
    const float slopeDepth=depth*static_cast<float>(bounded(value(a,g,VSlopeCvDepth),0,100))*0.01f;
    const bool bipolar=bounded(a.v[CvResponse],0,1)!=0;
    float normal[6],signedNormal[6];
    for(unsigned i=0;i<6;++i){
        const float voltage=cv.volts[i]==cv.volts[i]?cv.volts[i]:0.0f;
        signedNormal[i]=boundedFloat(voltage*0.2f,-1,1);
        normal[i]=boundedFloat(signedNormal[i],0,1);
    }
    if(cv.present[0]){
        const float count=bipolar?static_cast<float>(c.bursts)+39.0f*signedNormal[0]*countDepth
            :modulated(c.bursts,1+static_cast<float>(c.bursts-1)*normal[0],countDepth);
        c.bursts=static_cast<uint8_t>(rounded(boundedFloat(count,1,40)));
    }
    if(cv.present[1]){
        if(musical){
            const float position=bipolar?static_cast<float>(width)+15.0f*signedNormal[1]*timeDepth
                :modulated(static_cast<float>(width),normal[1]*15,timeDepth);
            width=rounded(boundedFloat(position,0,15));
        }else if(bipolar){
            c.widthSeconds=boundedFloat(c.widthSeconds*modulationScale(4.0f*signedNormal[1]*timeDepth),0.02f,2.5f);
        }else c.widthSeconds=modulated(c.widthSeconds,0.02f+normal[1]*2.48f,timeDepth);
    }
    if(cv.present[2]&&timeDepth>0.0f){
        if(musical){
            const float base=static_cast<float>(interval==0?0:interval-1)/15.0f;
            const float position=boundedFloat(bipolar?base+signedNormal[2]*timeDepth
                :modulated(base,normal[2],timeDepth),0,1);
            // A bipolar zero is neutral, including the first grid position.
            if(!bipolar||signedNormal[2]!=0.0f){
                const float offThreshold=bipolar?0.0f:0.005f;
                interval=position>offThreshold?static_cast<uint8_t>(rounded(position*15.0f)+1):0;
            }
        }else if(bipolar){
            if(c.rateHz>0.0f)c.rateHz=boundedFloat(c.rateHz*modulationScale(4.0f*signedNormal[2]*timeDepth),0,51);
        }else c.rateHz=modulated(c.rateHz,normal[2]*51,timeDepth);
    }
    if(cv.present[3])c.slope=boundedFloat(bipolar?c.slope+signedNormal[3]*slopeDepth
        :modulated(c.slope,c.slope*signedNormal[3],slopeDepth),-1,1);
    float shape=static_cast<float>(c.shape);
    if(cv.present[4]){
        const float target=shapeResponse==2?normal[4]*7.0f
            :static_cast<float>(bounded(static_cast<int>(normal[4]*8.0f),0,7));
        shape=boundedFloat(bipolar?shape+7.0f*signedNormal[4]*shapeDepth
            :modulated(shape,target,shapeDepth),0,7);
        c.shape=static_cast<Shape>(rounded(shape));
    }
    if(shapeResponse==2)c.shapePosition=shape;
    if(cv.present[5])c.level=boundedFloat(bipolar?c.level+10.0f*signedNormal[5]*depth
        :modulated(c.level,c.level*normal[5],depth),0,10);
    if(musical){
        c.clockMode=Config::Cv;c.cvDivider=1;c.preserveClockPhase=true;
        c.widthSeconds=reference.periodSeconds*musicalRatio(static_cast<uint8_t>(width));
        c.glideRiseSeconds=reference.periodSeconds*glideRatio(static_cast<uint8_t>(bounded(value(a,g,VGlideRiseRatio),0,16)));
        c.glideFallSeconds=reference.periodSeconds*glideRatio(static_cast<uint8_t>(bounded(value(a,g,VGlideFallRatio),0,16)));
    }
    return c;
}
void emitVoiceMidi(Algorithm& a,unsigned g,const Sample& s){
    Voice& v=a.voices[g];
    const int dest=bounded(value(a,g,VMidiDestination),0,7),channel=bounded(value(a,g,VMidiOutputChannel),1,16);
    const int note=bounded(value(a,g,VMidiOutputNote),0,127),cc=bounded(value(a,g,VMidiEnvelopeCc),-1,119);
    if(v.noteOn&&(dest!=v.noteDest||channel!=v.noteChannel||note!=v.noteNumber||!value(a,g,VMidiNotes)||value(a,g,VMute)))noteOff(v);
    if(dest!=v.previousDest||channel!=v.previousChannel||cc!=v.previousCc){
        if(v.previousDest&&v.previousCc>=0)NT_sendMidi3ByteMessage(static_cast<uint32_t>(v.previousDest),static_cast<uint8_t>(0xB0|(v.previousChannel-1)),static_cast<uint8_t>(v.previousCc),0);
        v.lastCc=-1;v.ccCountdown=0;v.previousDest=dest;v.previousChannel=channel;v.previousCc=cc;
    }
    if(s.burstStarted){
        noteOff(v);
        if(dest&&value(a,g,VMidiNotes)&&!value(a,g,VMute)&&s.burstLevel>0){
            v.noteDest=dest;v.noteChannel=channel;v.noteNumber=note;v.noteOn=true;
            const int velocity=bounded(rounded(s.burstLevel*12.7f),1,127);
            NT_sendMidi3ByteMessage(static_cast<uint32_t>(dest),static_cast<uint8_t>(0x90|(channel-1)),static_cast<uint8_t>(note),static_cast<uint8_t>(velocity));
        }
    }else if(s.burstEnded||s.active==0)noteOff(v);
    if(v.ccCountdown)--v.ccCountdown;
    const int ccValue=bounded(rounded(s.envelope*12.7f),0,127);
    const bool finalZero=ccValue==0&&s.active==0&&v.lastCc>0;
    if(dest&&cc>=0&&(!v.ccCountdown||finalZero)&&ccValue!=v.lastCc){
        NT_sendMidi3ByteMessage(static_cast<uint32_t>(dest),static_cast<uint8_t>(0xB0|(channel-1)),static_cast<uint8_t>(cc),static_cast<uint8_t>(ccValue));
        v.lastCc=ccValue;v.ccCountdown=a.sampleRate/static_cast<uint32_t>(bounded(value(a,g,VMidiCcRate),1,100));
    }
}

void step(_NT_algorithm* self,float* buses,int numFramesBy4){
    auto& a=*static_cast<Algorithm*>(self);
    if(!buses||!a.v||numFramesBy4<=0||static_cast<uint32_t>(numFramesBy4)>NT_globals.maxFramesPerStep/4)return;
    const int frames=numFramesBy4*4;
    if(a.sampleRate!=NT_globals.sampleRate){
        a.sampleRate=NT_globals.sampleRate;a.reference.setSampleRate(static_cast<float>(a.sampleRate));
        for(auto& v:a.voices){v.engine.setSampleRate(static_cast<float>(a.sampleRate));v.ccCountdown=0;}
    }
    uint32_t allFlags=a.allPending.exchange(0,std::memory_order_acquire);
    uint32_t flags[2];
    for(unsigned g=0;g<2;++g)flags[g]=a.voices[g].pending.exchange(0,std::memory_order_acquire);
    if(allFlags&packFlag){
        applyPerformancePreset(a,static_cast<unsigned>(bounded(a.v[PackPreset],0,15)));
        // Loading a scene is silent and takes precedence over pending triggers,
        // steps and voice-bank actions. Existing user memories stay untouched.
        flags[0]=flags[1]=0;
        allFlags=resetFlag;
    }
    for(unsigned g=0;g<2;++g){
        const unsigned slot=static_cast<unsigned>(bounded(value(a,g,VPresetSlot),1,16)-1);
        if(flags[g]&saveFlag)saveSlot(a,g,slot,capturePreset(a,g));
        if(flags[g]&factoryFlag)a.savedMasks[g].fetch_and(~(uint32_t{1}<<slot),std::memory_order_release);
        if(flags[g]&(loadFlag|factoryFlag)){
            Preset preset;
            if(tryLoadSlot(a,g,slot,preset)){applyPreset(a,g,preset);flags[g]|=resetFlag;}
        }
    }
    constexpr VoiceParameter inParameters[]={VTriggerInput,VResetInput,VStopInput,VStepInput};
    constexpr VoiceParameter outParameters[]={VEnvelopeOutput,VGateOutput,VActiveOutput,VEocOutput,VClockOutput,VLevelsOutput,VProgressOutput,VBurstOutput};
    constexpr Parameter sharedInputs[]={TriggerAllInput,ResetAllInput,StopAllInput,StepAllInput};
    constexpr Parameter cvInputs[]={CountCvInput,WidthCvInput,IntervalCvInput,SlopeCvInput,ShapeCvInput,AmplitudeCvInput};
    const float* ins[2][4];Output outs[2][8];
    for(unsigned g=0;g<2;++g){
        for(unsigned j=0;j<4;++j)ins[g][j]=input(a,voiceMap[g][inParameters[j]],buses,frames);
        for(unsigned j=0;j<8;++j)outs[g][j]=output(a,voiceMap[g][outParameters[j]],buses,frames);
    }
    const float* cvPtrs[6];for(unsigned j=0;j<6;++j)cvPtrs[j]=input(a,cvInputs[j],buses,frames);
    const float* allPtrs[4];for(unsigned j=0;j<4;++j)allPtrs[j]=input(a,sharedInputs[j],buses,frames);
    const float* clk=input(a,ClockInput,buses,frames);
    const Output mix=output(a,MixOutput,buses,frames),busy=output(a,BusyAllOutput,buses,frames);
    const Output refOut=output(a,ReferenceClockOutput,buses,frames),focusOut=output(a,FocusProgressOutput,buses,frames);
    const int dest=bounded(a.v[MidiDestination],0,7);
    if(a.transportOn&&(dest!=a.transportDest||!a.v[MidiTransportOutput]))transportStop(a);
    ReferenceConfig referenceConfig;
    referenceConfig.source=static_cast<ReferenceConfig::Source>(bounded(a.v[SharedClockSource],0,3));
    referenceConfig.bpm=static_cast<float>(bounded(a.v[Tempo],200,3000))*0.1f;
    referenceConfig.cvPpqn=ppqnValues[bounded(a.v[CvPpqn],0,6)];
    for(int i=0;i<frames;++i){
        // Snapshot every input before any output; routing may share the same bus.
        CvSample cv{};for(unsigned j=0;j<6;++j){cv.present[j]=cvPtrs[j]!=nullptr;cv.volts[j]=cvPtrs[j]?cvPtrs[j][i]:0;}
        bool allEdges[4];for(unsigned j=0;j<4;++j)allEdges[j]=a.allInputs[j].edge(allPtrs[j]?allPtrs[j][i]:0,a.v[sharedInputs[j]]);
        const bool clockEdge=a.clock.edge(clk?clk[i]:0,a.v[ClockInput]);
        bool edges[2][4];
        for(unsigned g=0;g<2;++g)for(unsigned j=0;j<4;++j)
            edges[g][j]=a.voices[g].inputs[j].edge(ins[g][j]?ins[g][j][i]:0,value(a,g,inParameters[j]));
        constexpr uint32_t eventFlags[]={triggerFlag,resetFlag,stopFlag,stepFlag};
        for(unsigned j=0;j<4;++j)if(allEdges[j])allFlags|=eventFlags[j];
        const bool resetAll=(allFlags&resetFlag)!=0;
        const ReferenceSample reference=a.reference.tick(referenceConfig,clockEdge,resetAll);
        if(reference.reset)a.referenceRemaining=0;
        if(reference.beat)a.referenceRemaining=static_cast<uint32_t>(static_cast<float>(a.sampleRate)*static_cast<float>(bounded(a.v[PulseWidth],1,20))*0.001f+0.5f);
        const float referenceVoltage=a.referenceRemaining?5.0f:0.0f;
        if(a.referenceRemaining)--a.referenceRemaining;
        Sample samples[2];
        for(unsigned g=0;g<2;++g){
            Voice& v=a.voices[g];
            const bool musical=value(a,g,VTimingMode)!=0;
            uint8_t interval=0;const Config c=config(a,g,reference,cv,interval);
            const bool resetEdge=edges[g][1]||((flags[g]|allFlags)&resetFlag)||(musical&&reference.reset);
            const bool stopEdge=edges[g][2]||((flags[g]|allFlags)&stopFlag)||(musical&&reference.stop)||!enabled(a,g);
            const bool triggerEdge=edges[g][0]||((flags[g]|allFlags)&triggerFlag)||(musical&&reference.start);
            const bool manualStep=edges[g][3]||((flags[g]|allFlags)&stepFlag);
            const bool timedEdge=musical?v.timing.tick(reference,interval):clockEdge;
            if(resetEdge||stopEdge)noteOff(v);
            if(resetEdge||stopEdge)v.timing.reset();
            if(triggerEdge&&!resetEdge&&!stopEdge){
                const bool fullVelocity=edges[g][0]||(allFlags&triggerFlag)||(musical&&reference.start);
                v.engine.trigger(c,fullVelocity?1.0f:static_cast<float>(v.velocity.load(std::memory_order_relaxed))/127.0f);
            }
            samples[g]=v.engine.tick(c,false,resetEdge,timedEdge,manualStep,stopEdge);
            if(samples[g].burstStarted)noteOff(v);
        }
        flags[0]=flags[1]=allFlags=0;
        const bool sounding=samples[0].active!=0||samples[1].active!=0;
        const bool started=samples[0].burstStarted||samples[1].burstStarted;
        const bool restart=(samples[0].burstStarted&&samples[0].index==0&&samples[1].active==0)
            ||(samples[1].burstStarted&&samples[1].index==0&&samples[0].active==0);
        if(!sounding||(restart&&a.transportOn))transportStop(a);
        if(started&&dest&&a.v[MidiTransportOutput]&&!a.transportOn){
            NT_sendMidiByte(static_cast<uint32_t>(dest),0xFA);a.transportOn=true;a.transportDest=dest;
        }
        for(unsigned g=0;g<2;++g){
            const Sample& s=samples[g];Voice& v=a.voices[g];
            emitVoiceMidi(a,g,s);
            const float values[]={s.envelope,s.gate,s.active,s.eoc,s.clock,s.levels,s.progress,s.burst};
            for(unsigned j=0;j<8;++j)write(outs[g][j],i,values[j]);
            v.latestMillivolts.store(static_cast<uint32_t>(boundedFloat(s.envelope,0,10)*1000),std::memory_order_relaxed);
            v.latestIndex.store(s.active!=0?static_cast<uint32_t>(s.index)+1:0,std::memory_order_relaxed);
            v.latestActive.store(s.active!=0,std::memory_order_relaxed);
        }
        const bool musicalClock=value(a,0,VTimingMode)||(enabled(a,1)&&value(a,1,VTimingMode));
        if(dest&&a.v[MidiClockOutput]&&(musicalClock?reference.midiClock:samples[0].midiClock))
            NT_sendMidiByte(static_cast<uint32_t>(dest),0xF8);
        write(mix,i,boundedFloat(samples[0].envelope+samples[1].envelope,0,10));
        write(busy,i,sounding?10.0f:0.0f);write(refOut,i,referenceVoltage);
        write(focusOut,i,samples[bounded(a.v[FocusGenerator],0,1)].progress);
    }
}
bool draw(_NT_algorithm* self){
    const auto& a=*static_cast<Algorithm*>(self);
    const int focus=bounded(a.v[FocusGenerator],0,1);
    NT_drawText(4,22,"Multi Burst NT 0.4");
    for(unsigned g=0;g<2;++g){
        const Voice& v=a.voices[g];const int y=34+static_cast<int>(g)*11;
        NT_drawText(4,y,focus==static_cast<int>(g)?">":" ",15);
        NT_drawText(13,y,g==0?"G1":"G2",15);
        const bool active=v.latestActive.load(std::memory_order_relaxed)!=0;
        NT_drawText(34,y,!enabled(a,g)?"OFF":value(a,g,VMute)?"MUTE":active?"RUN":"READY",active?15:8);
        char text[16]={};NT_intToString(text,static_cast<int32_t>(v.latestIndex.load(std::memory_order_relaxed)));NT_drawText(83,y,text);
        NT_drawShapeI(kNT_box,113,y-7,251,y,8);
        const int width=bounded(static_cast<int>(v.latestMillivolts.load(std::memory_order_relaxed)*136/10000),0,136);
        if(width)NT_drawShapeI(kNT_rectangle,114,y-6,114+width,y-1,15);
    }
    NT_drawText(4,56,"L:trig C:reset R:step  encL:focus/stop",8,kNT_textLeft,kNT_textTiny);
    return false;
}
uint32_t hasCustomUi(_NT_algorithm*){return kNT_potButtonL|kNT_potButtonC|kNT_potButtonR|kNT_encoderL|kNT_encoderButtonL|kNT_encoderButtonR;}
void customUi(_NT_algorithm* self,const _NT_uiData& data){
    auto& a=*static_cast<Algorithm*>(self);
    const uint16_t pressed=data.controls&~data.lastButtons;
    const unsigned focus=static_cast<unsigned>(bounded(a.v[FocusGenerator],0,1));
    if(data.encoders[0]){
        const int index=NT_algorithmIndex(&a);
        if(index>=0)NT_setParameterFromUi(static_cast<uint32_t>(index),NT_parameterOffset()+FocusGenerator,static_cast<int16_t>(focus==0?1:0));
    }
    uint32_t flags=0;
    if(pressed&kNT_potButtonL)flags|=triggerFlag;
    if(pressed&kNT_potButtonC)flags|=resetFlag;
    if(pressed&kNT_potButtonR)flags|=stepFlag;
    if(pressed&kNT_encoderButtonL)flags|=stopFlag;
    if(flags)queue(a.voices[focus],flags);
    if(pressed&kNT_encoderButtonR)a.allPending.fetch_or(triggerFlag,std::memory_order_release);
}
void serialise(_NT_algorithm* self,_NT_jsonStream& stream){
    const auto& a=*static_cast<Algorithm*>(self);
    stream.addMemberName("multiBurstBankVersion");stream.addNumber(1);
    stream.addMemberName("multiBurstBanks");stream.openArray();
    for(unsigned g=0;g<2;++g)for(unsigned slot=0;slot<16;++slot){
        if(!(a.savedMasks[g].load(std::memory_order_acquire)&(uint32_t{1}<<slot)))continue;
        Preset p;
        // Serialisation runs outside the audio callback; never replace an
        // override with its factory sound if a concurrent Save is in progress.
        while(!tryLoadSlot(a,g,slot,p)){}
        stream.addNumber(static_cast<int>(g));stream.addNumber(static_cast<int>(slot));
        for(const auto& b:presetBindings)stream.addNumber(p.*(b.field));
    }
    stream.closeArray();
}
bool deserialise(_NT_algorithm* self,_NT_jsonParse& parse){
    auto& a=*static_cast<Algorithm*>(self);
    int members=0;if(!parse.numberOfObjectMembers(members))return false;
    PresetBank staged;bool haveBank=false,haveVersion=false;
    for(int member=0;member<members;++member){
        if(parse.matchName("multiBurstBankVersion")){
            int version=0;if(!parse.number(version)||version!=1)return false;haveVersion=true;
        }else if(parse.matchName("multiBurstBanks")){
            constexpr int recordSize=static_cast<int>(ARRAY_SIZE(presetBindings))+2;
            int elements=0;if(!parse.numberOfArrayElements(elements)||elements<0||elements>32*recordSize||elements%recordSize)return false;
            const int entries=elements/recordSize;
            haveBank=true;
            for(int entry=0;entry<entries;++entry){
                int g=0,slot=0;
                if(!parse.number(g)||!parse.number(slot)||g<0||g>1||slot<0||slot>15)return false;
                Preset p;
                for(const auto& b:presetBindings){
                    int v=0;if(!parse.number(v)||v<-32768||v>32767)return false;
                    p.*(b.field)=static_cast<int16_t>(v);
                }
                staged.save(static_cast<unsigned>(g),static_cast<unsigned>(slot),p);
            }
        }else if(!parse.skipMember())return false;
    }
    if(haveBank&&!haveVersion)return false;
    if(haveBank){
        a.savedMasks[0].store(0,std::memory_order_release);a.savedMasks[1].store(0,std::memory_order_release);
        for(unsigned g=0;g<2;++g)for(unsigned slot=0;slot<16;++slot)
            if(staged.isSaved(g,slot))saveSlot(a,g,slot,staged.load(g,slot));
    }
    return true;
}
int parameterString(_NT_algorithm*,int p,int v,char* text){
    if(p==MidiInputChannel||p==G2MidiInputChannel){
        if(v==0||v==17){const char* s=v==0?"Off":"Omni";int n=0;while(s[n]){text[n]=s[n];++n;}text[n]=0;return n;}
        return NT_intToString(text,v);
    }
    if((p==MidiEnvelopeCc||p==G2MidiEnvelopeCc)&&v<0){text[0]='O';text[1]='f';text[2]='f';text[3]=0;return 3;}
    return 0;
}
const _NT_factory factory={
    .guid=NT_MULTICHAR('M','b','N','t'),
    .name="Multi Burst NT",
    .description="Dual burst envelopes, musical ratios, CV and MIDI",
    .numSpecifications=0,.specifications=nullptr,.calculateStaticRequirements=nullptr,.initialise=nullptr,
    .calculateRequirements=calculateRequirements,.construct=construct,.parameterChanged=parameterChanged,.step=step,
    .draw=draw,.midiRealtime=midiRealtime,.midiMessage=midiMessage,.tags=kNT_tagUtility,
    .hasCustomUi=hasCustomUi,.customUi=customUi,.setupUi=nullptr,.serialise=serialise,.deserialise=deserialise,
    .midiSysEx=nullptr,.parameterUiPrefix=nullptr,.parameterString=parameterString
};
} // namespace
extern "C" uintptr_t pluginEntry(_NT_selector selector,uint32_t data){
    switch(selector){
    case kNT_selector_version:return multi_burst::requiredApiVersion;
    case kNT_selector_numFactories:return 1;
    case kNT_selector_factoryInfo:return data==0?reinterpret_cast<uintptr_t>(&factory):0;
    default:return 0;
    }
}

