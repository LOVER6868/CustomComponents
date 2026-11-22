#pragma once
#include "core/ManagedPool.hpp"
#include "core/MetadataBinding.hpp"
#include "engine/EntityAccess.hpp"

namespace CustomComponents::Engine::EffectEventContext {
// Return an owned pooled query context. The runtime target may now be a lane
// or player; subset queries still refer to the original triggering card.
inline EntityAccess::Obj* Acquire(EntityAccess::Obj* effect,EntityAccess::Obj* instances,EntityAccess::Obj* effectEntity){
    using Obj=EntityAccess::Obj;
    static auto toQuery=BNM::Class("PvZCards.Engine","Effect").GetMethod("ToEntityQueryContext",1).cast<Obj*>();
    static auto saved=BNM::Class("PvZCards.Engine","Effect").GetField("Context").cast<Obj*>();
    static auto queryPayload=BNM::Class("PvZCards.Engine","EntityQueryContext").GetField("EffectContext").cast<Obj*>();
    static auto setSelf=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("SetSelfIdFromEffectEntity",1).cast<void>();
    // sprig-lint: allow names identical parameter names require BNM's type-based overload selection
    static auto copy=BNM::Class("PvZCards.Engine","EffectContext").GetMethod("CopyFrom",{BNM::Class("PvZCards.Engine","EffectContext").GetCompileTimeClass()}).cast<void>();
    if(!effect || !instances || !toQuery.IsValid() || !saved.IsValid() || !queryPayload.IsValid() || !copy.IsValid() || !setSelf.IsValid())return nullptr;
    Obj* context=Core::Metadata::Bind(toQuery,effect)(instances);
    if(!context)return nullptr;
    if(Obj* original=Core::Metadata::Bind(saved,effect).Get()){
        if(Obj* payload=Core::Metadata::Bind(queryPayload,context).Get())Core::Metadata::Bind(copy,payload)(original);
    }
    if(effectEntity)Core::Metadata::Bind(setSelf,context)(effectEntity);
    return context;
}
}
