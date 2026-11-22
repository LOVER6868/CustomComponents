#pragma once
// Patch 1.7.5 temporary PF3 diagnostics: compile with
// -DCUSTOMCOMPONENTS_PF3_REPAIR_TRACE=0 for the release build.
// No additional native hooks. All event sampling is bounded at call sites.
#ifndef CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
#define CUSTOMCOMPONENTS_PF3_REPAIR_TRACE 0
#endif
#include "log.h"
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
#define PF3_REPAIR_TRACE(...) LOG_INFO(__VA_ARGS__)
#else
#define PF3_REPAIR_TRACE(...) ((void)0)
#endif
