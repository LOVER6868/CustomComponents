#include <bloom/bloom.h>
#include "startup/Module.hpp"
#if __has_include(<matchrecorder/replay_api.h>)
#include "integration/ReplayProvider.hpp"
#endif

namespace {
void Init() {
    CustomComponents::Startup::Initialize();
#if __has_include(<matchrecorder/replay_api.h>)
    // MatchRecorder is optional. Only attach the provider if the replay
    // module is actually present and its own configuration enables capture.
    // Do not use BLOOM_CONFIG_BOOL here: this file belongs to CustomComponents.
    if (Bloom::FindModule("MatchRecorder") &&
        Bloom::ConfigBool("MatchRecorder", "General", "Enabled", true) &&
        Bloom::ConfigBool("MatchRecorder", "Diagnostics", "CaptureCustomState", true)) {
        CustomComponents::Integration::ReplayProvider::Register();
    }
#endif
}
}

// Modules share one SetCardData owner and subscribe through this adapter.
extern "C" bool Bloom_RegisterSetCardDataObserver(
    void (*observer)(void*, int, void*, const void*)) {
    return CustomComponents::Presentation::Pick3CardMetadata::RegisterSharedObserver(observer);
}

BLOOM_REGISTER_PNP(Init);