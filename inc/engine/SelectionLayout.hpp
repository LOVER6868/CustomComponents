#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <cstddef>
#include "log.h"
namespace CustomComponents::Engine::SelectionLayout {
inline constexpr std::size_t source=0x24,target=0x28,springboard=0x2C,effectId=0x30,group=0x3C;
inline constexpr std::size_t cancelled=0x45,selectedTargets=0x48,phase=0x50,model=0x10;
inline bool Field(const char* ns,const char* type,const char* name,std::size_t offset) {
    const auto field=BNM::Class(ns,type).GetField(name);
    if(field.IsValid() && field.GetOffset()==offset) return true;
    LOG_ERR("CustomComponents/SelectionLayout: unsupported field %s.%s expected=%zu",type,name,offset);
    return false;
}
inline bool Validate() {
    return Field("PvZCards.Engine","Effect","SourceId",source) &&
        Field("PvZCards.Engine","Effect","TargetId",target) &&
        Field("PvZCards.Engine","Effect","SpringboardId",springboard) &&
        Field("PvZCards.Engine","Effect","EffectEntityId",effectId) &&
        Field("PvZCards.Engine","Effect","EffectGroupId",group) &&
        Field("PvZCards.Engine","Effect","Cancelled",cancelled) &&
        Field("PvZCards.Engine.Effects","SelectTargetEffect","SelectedTargetIds",selectedTargets) &&
        Field("PvZCards.Engine.Effects","SelectTargetEffect","Phase",phase) &&
        Field("PvZCards.Core.Systems","SelectTargetSystem","<Model>k__BackingField",model);
}
}
