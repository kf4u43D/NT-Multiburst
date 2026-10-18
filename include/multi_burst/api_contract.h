#pragma once
#include <distingnt/api.h>

namespace multi_burst {
// The SDK may describe unreleased firmware. Advertise the API this build needs,
// independently of kNT_apiVersionCurrent, to keep existing firmware loadable.
#if defined(MULTI_BURST_NT_EMU)
constexpr uint32_t requiredApiVersion = kNT_apiVersion8;
#else
constexpr uint32_t requiredApiVersion = kNT_apiVersion13;
#endif
static_assert(requiredApiVersion <= kNT_apiVersionCurrent, "The pinned SDK is too old for this build");
}
