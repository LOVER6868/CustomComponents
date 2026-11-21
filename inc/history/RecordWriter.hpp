#pragma once
#include "history/CustomHistory.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/CustomRecordHistoryAnchor.hpp"
#include "engine/EffectGuards.hpp"
#include "core/ModelRegistry.hpp"
namespace CustomComponents::History::Writer {
using Obj=BNM::IL2CPP::Il2CppObject;
inline std::uint64_t Commit(Record record,Obj* model,Obj* instances=nullptr){
    if(!model)return 0;
    // AI scopes describe disposable probes, not committed match actions.
    if(Core::Performance::aiDepth)return 0;
    record.model=reinterpret_cast<std::uintptr_t>(model);record.modelToken=Core::ModelRegistry::Identify(model,true);
    if(Obj* effect=Engine::EffectGuards::current.effect){
        static auto type=BNM::Class("PvZCards.Engine","Effect");
        static auto source=type.GetField("SourceId").cast<int>();
        static auto definition=type.GetField("EffectEntityId").cast<int>();
        if(record.sourceId<0 && source.IsValid())record.sourceId=source[effect].Get();
        if(record.effectId<0 && definition.IsValid())record.effectId=definition[effect].Get();
    }
    if(!instances){Obj* entity=Engine::EntityAccess::Find(model,record.entityId>=0?record.entityId:record.sourceId);if(entity)instances=Engine::Match::Instances(entity);}
    Obj* history=instances?Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","History").GetMonoType()):nullptr;
    if(history){record.history=reinterpret_cast<std::uintptr_t>(history);record.stockHistoryAnchor=Engine::CustomRecordHistoryAnchor::Count(history);}
    if(!record.parentActionId)record.parentActionId=Engine::EffectGuards::current.event;
    if(record.stage==Stage::Observed)record.stage=Stage::Committed;
    return History::Commit(std::move(record));
}
inline Obj* Model(Obj* card){Obj* instances=card?Engine::Match::Instances(card):nullptr;return instances?Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EntityModel").GetMonoType()):nullptr;}
}
