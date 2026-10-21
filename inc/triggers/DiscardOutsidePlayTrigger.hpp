#pragma once

#include <BNM/ClassesManagement.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Components {

    // Fires when a card is successfully discarded from an outside-play zone.
    //
    // Current event sources:
    //   - Deck -> DiscardPile (DiscardFromDeckSystem)
    //   - Hand -> DiscardPile (MoveToDiscardPileSystem, only when TargetId was
    //     actually in a Hand before stock processing)
    //
    // It intentionally does NOT fire for cards discarded from play/death.
    struct DiscardOutsidePlayTrigger : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            DiscardOutsidePlayTrigger,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "DiscardOutsidePlayTrigger",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "MarkerComponent",
                "EngineLib.dll"
            ).Build(),
            {},
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "Trigger",
                "EngineLib.dll"
            ).Build()
        );
    };

} // namespace CustomComponents::Components
