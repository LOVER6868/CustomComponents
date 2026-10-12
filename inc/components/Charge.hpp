#pragma once

#include <BNM/ClassesManagement.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Traits {

    // Charge is a native GrantableAbility. All state lives in the inherited
    // GrantableAbility.Counters table; no parallel ModifiableInt/custom fields.
    struct Charge : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            Charge,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "Charge", "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "GrantableAbility", "EngineLib.dll"
            ).Build(),
            {}
        );
    };

} // namespace CustomComponents::Traits
