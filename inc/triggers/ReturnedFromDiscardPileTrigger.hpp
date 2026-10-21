#pragma once

#include <BNM/ClassesManagement.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Components {

    // JSON-facing, stock-shaped trigger marker.
    //
    // Serialized form:
    // {
    //   "$type": "PvZCards.Engine.Components.ReturnedFromDiscardPileTrigger, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
    //   "$data": {}
    // }
    //
    // Runtime dispatch is handled by CustomTriggerSystem.  This remains a real
    // managed MarkerComponent and implements PvZCards.Engine.Trigger so it has
    // the same managed shape as EA's marker triggers.
    struct ReturnedFromDiscardPileTrigger : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            ReturnedFromDiscardPileTrigger,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "ReturnedFromDiscardPileTrigger",
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
