#pragma once
// Optional: compiled only when Bloom/MatchRecorder is installed.
// No reverse dependency: removing MatchRecorder leaves CustomComponents unchanged.
#include <matchrecorder/replay_api.h>
#include "integration/ReplayVisualProvider.hpp"
#include <matchrecorder/store.h>
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/AbilityRegistry.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "engine/SimulationChanges.hpp"
#include "history/Serialization.hpp"
#include "core/FrontendTickDispatcher.hpp"
#include "core/SceneViewContext.hpp"
#include "engine/MatchAccess.hpp"
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <cstdint>
#include <cmath>
#include <string>
#include <atomic>

namespace CustomComponents::Integration::ReplayProvider {
using Obj=BNM::IL2CPP::Il2CppObject;
using List=BNM::Structures::Mono::List<Obj*>;
inline std::atomic<bool> historyDirty{false};
inline void FlushHistory(){
    if(!historyDirty.exchange(false))return;
    Obj* mapper=Core::SceneViewContext::RootedMapper();
    static auto field=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();
    if(mapper && field.IsValid())MatchRecorder::ExtensionStateSettled(field[mapper].Get());
}
inline bool Capture(void* rawModel,std::string& json){
    auto* model=reinterpret_cast<Obj*>(rawModel);
    if(!model)return false;
    static auto entitiesField=BNM::Class("PvZCards.Engine","EntityModel")
        .GetField("Entities").cast<List*>();
    auto ef=entitiesField;
    List* entities=ef.IsValid()?ef[model].Get():nullptr;
    if(!entities || entities->GetSize()<0 || entities->GetSize()>5000)return false;
    json="{\"extensionVersion\":1,\"abilities\":[";
    bool firstAbility=true;
    AbilityRegistry::ForEach([&](const AbilityDescriptor& item){
        if(!firstAbility)json+=',';
        firstAbility=false;
        json+="{\"id\":"+std::to_string(item.id)+",\"name\":"+
            MatchRecorder::JsonString(item.name?item.name:"")+",\"storage\":"+
            std::to_string(static_cast<int>(item.storage))+"}";
    });
    json+="],\"schemas\":[";
    bool firstSchema=true;
    for(const auto& schema:Systems::CustomFieldBridgeSystem::schemas){
        if(!firstSchema)json+=',';
        firstSchema=false;
        json+="{\"type\":"+MatchRecorder::JsonString(schema.namespaze+"."+schema.typeName)+",\"fields\":[";
        bool firstField=true;
        for(const auto& field:schema.fields){
            if(!firstField)json+=',';
            firstField=false;
            json+="{\"name\":"+MatchRecorder::JsonString(field.name)+",\"kind\":"+
                std::to_string(static_cast<int>(field.kind))+"}";
        }
        json+="]}";
    }
    json+="],\"entities\":[";
    bool firstEntity=true;
    for(auto* entity:entities->ToVector()){
        if(!entity)continue;
        std::string components;
        bool firstComponent=true;
        for(auto& schema:Systems::CustomFieldBridgeSystem::schemas){
            auto type=BNM::Class(schema.namespaze,schema.typeName);
            if(!type.IsValid())continue;
            Obj* component=Engine::Match::Component(entity,type.GetMonoType());
            if(!component)continue;
            if(!firstComponent)components+=',';
            firstComponent=false;
            components+="{\"type\":"+MatchRecorder::JsonString(schema.namespaze+"."+schema.typeName)+",\"values\":{";
            bool firstValue=true;
            for(auto& field:schema.fields){
                const auto value=Systems::CustomFieldBridgeSystem::ReadValue(component,field);
                if(!value.present)continue;
                if(!firstValue)components+=',';
                firstValue=false;
                components+=MatchRecorder::JsonString(field.name)+":";
                switch(field.kind){
                    case Systems::CustomFieldBridgeSystem::FieldKind::Bool:
                        components+=(value.value.boolValue?"true":"false");break;
                    case Systems::CustomFieldBridgeSystem::FieldKind::Int32:
                        components+=std::to_string(value.value.intValue);break;
                    case Systems::CustomFieldBridgeSystem::FieldKind::Float:
                        components+=(std::isfinite(value.value.floatValue) ? std::to_string(value.value.floatValue) : "null");break;
                    case Systems::CustomFieldBridgeSystem::FieldKind::String:
                        components+=(value.value.stringValue
                            ?MatchRecorder::JsonString(value.value.stringValue->str()):"null");break;
                }
            }
            components+="}}";
        }
        if(firstComponent)continue;
        if(!firstEntity)json+=',';
        firstEntity=false;
        json+="{\"id\":"+std::to_string(Engine::Match::Id(entity))+",\"components\":["+components+"]}";
        if(json.size()>4u*1024u*1024u)return false;
    }
    json+="]}";
    return json.size()<=5u*1024u*1024u;
}
inline void OnSettled(Obj* model){
    // Never serialize entire probe models or perform file I/O in simulation.
    // Functional records preserve each mutation; the optional state snapshot
    // is coalesced to the owning frontend's next tick.
    if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())historyDirty=true;
}
inline bool Register(){
    return ReplayVisualProvider::Register() && MatchRecorder::RegisterExtension("CustomComponents",&Capture)
        && Engine::SimulationChanges::settled.Add(&OnSettled)
        && Core::FrontendTickDispatcher::additionalPresentationTicks.Add(FlushHistory);
}
}

