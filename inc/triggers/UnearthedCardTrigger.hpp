#pragma once

#include <BNM/ClassesManagement.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Components {

    // JSON-facing, stock-shaped trigger marker.
    //
    // Globally observable upon verified completion of a PickFromThree-selected operation.
    // Do not emit from popup close or local transport prediction.
    //
    // Serialized form:
    // {
    //   "$type": "PvZCards.Engine.Components.UnearthedCardTrigger, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
    //   "$data": {}
    // }
    //
    // Runtime dispatch is handled by CustomTriggerSystem.  This remains a real
    // managed MarkerComponent and implements PvZCards.Engine.Trigger so it has
    // the same managed shape as EA's marker triggers.
    struct UnearthedCardTrigger : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            UnearthedCardTrigger,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "UnearthedCardTrigger",
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
