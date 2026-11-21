#pragma once
#include <BNM/ClassesManagement.hpp>
#include "core/CustomComponentRegistry.hpp"
#include "engine/MatchAccess.hpp"

namespace CustomComponents::Zones::MarkedForDiscard {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Component:Obj {
    BNM_CustomClass(Component,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkedForDiscard","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
};
inline bool Initialize(){return CustomComponentRegistry::Register({"MarkedForDiscard","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"});}
inline void Mark(Obj* card){
    static auto type=BNM::Class("PvZCards.Engine.Components","MarkedForDiscard");
    if(card && type.IsValid())Engine::Match::Component(card,type.GetMonoType(),true);
}
inline void Clear(Obj* card){
    static auto type=BNM::Class("PvZCards.Engine.Components","MarkedForDiscard");
    static auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();
    if(!card || !type.IsValid() || !remove.IsValid() || !Engine::EntityAccess::Component(card,"MarkedForDiscard"))return;
    Obj* marker=Core::Metadata::Bind(remove,card)(type.GetMonoType());
    if(marker)Core::ManagedPool::Return(Engine::Match::Instances(card),marker);
}
}
