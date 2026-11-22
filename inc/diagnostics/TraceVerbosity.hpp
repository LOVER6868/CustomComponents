#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Native-only verbose hook tracing. Compile with
// -DCUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE=1 when investigating a lifecycle bug.
// This never disables hooks, stock calls, recovery guards, or record counters.
#ifndef CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE
#define CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE 0
#endif
#define CC_HOOK_TRACE(...) do { \
    if constexpr (CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE != 0) { CC_SELECTION_DIAG(__VA_ARGS__); } \
} while (false)
