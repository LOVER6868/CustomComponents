#pragma once
#include <BNM/ClassesManagement.hpp>
namespace CustomComponents::Components {
struct PlayFromDeckToLaneEffectDescriptor:BNM::IL2CPP::Il2CppObject {
    BNM_CustomClass(PlayFromDeckToLaneEffectDescriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","PlayFromDeckToLaneEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor","EngineLib.dll").Build(),{});
};
}
