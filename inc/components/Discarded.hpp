#pragma once

#include <cstdint>

#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Components {

    // Only actual discards from Hand or Deck count.
    // Cards destroyed from Play do NOT receive this component.
    // Remains on the same entity through ReturnedFromDiscardPileTrigger, then
    // is removed after that event's gameplay effects have drained.
    enum class DiscardSource : std::int32_t {
        None = 0,
        Hand = 1,
        Deck = 2
    };

    struct Discarded : BNM::IL2CPP::Il2CppObject {
        std::int32_t Source{};

        BNM_CustomClass(
            Discarded,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "Discarded", "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "MarkerComponent", "EngineLib.dll"
            ).Build(),
            {}
        );

        BNM_CustomField(
            Source,
            BNM::Defaults::Get<std::int32_t>(),
            "Source"
        );
    };

} // namespace CustomComponents::Components
