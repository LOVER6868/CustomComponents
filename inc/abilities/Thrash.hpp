#pragma once

#include <BNM/ClassesManagement.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Traits {

    // Thrash is a flag GrantableAbility. Presence/activation is represented by
    // the inherited GrantableAbility.Counters table exactly like stock flags.
    struct Thrash : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            Thrash,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "Thrash", "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "GrantableAbility", "EngineLib.dll"
            ).Build(),
            {}
        );
    };

} // namespace CustomComponents::Traits
