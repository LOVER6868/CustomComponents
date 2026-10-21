#pragma once

// Opt-in verbose instrumentation for generic serializer, trigger and lane
// preflight diagnostics. Normal discard/return, lifecycle protection, errors
// and animation-stall reporting are NOT governed by this switch.
// Set -DCUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS=1 when compiling the module
// for a short diagnostic run. This does not install any additional hooks.
#ifndef CUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS
#define CUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS 0
#endif

namespace CustomComponents::Diagnostics {
    inline constexpr bool kVerbose = CUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS != 0;
}

// Release builds omit incidental tracing in gameplay hooks and UI ticks.
// When needed, rebuild with -DCUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS=1.
#include "log.h"
#if CUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS
#define CC_DIAG(...) LOG_INFO(__VA_ARGS__)
#else
#define CC_DIAG(...) ((void)0)
#endif
