#pragma once

// Force-included only for the native Windows library. Firmware continues to
// use the released API 13 and its complete bus range without modification.
#include <cstddef>
#include <distingnt/api.h>

const _NT_globals& multiBurstHostGlobals();

// nt_emu 2.0.2 allocates 12 inputs, 8 outputs and 8 auxiliary buses.
// Parsing the official header before defining these aliases also limits the
// native routing parameter macros to the allocation of the desktop host.
#define kNT_lastBus 28
#define MULTI_BURST_NT_EMU 1
#define NT_globals (multiBurstHostGlobals())
#define pluginEntry multiBurstFirmwarePluginEntry
