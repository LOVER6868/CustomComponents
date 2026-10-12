#pragma once
// Phase 5 stage 2: registration only. Stock gameplay remains owned by
// MoveToDiscardPileSystem and the existing DiscardPileZoneEffectsSystem hook.
#include <BNM/Class.hpp>
#include "components/DiscardFromHandEffectDescriptor.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "log.h"
namespace CustomComponents::Systems::DiscardFromHandSystem {
inline bool Initialize() {
    if (CustomComponents::CustomComponentRegistry::FindByName(
            "PvZCards.Engine.Components", "DiscardFromHandEffectDescriptor"))
        return true;
    const bool ok = CustomComponents::CustomComponentRegistry::Register({
        "DiscardFromHandEffectDescriptor", "PvZCards.Engine.Components", "EngineLib.dll",
        "DestroyCardEffectDescriptor", "PvZCards.Engine.Components", "EngineLib.dll"
    });
    LOG_INFO("CustomComponents/DiscardFromHand: descriptor registered=%d", (int)ok);
    return ok;
}
}
