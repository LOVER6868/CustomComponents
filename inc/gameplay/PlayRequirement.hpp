#pragma once
#include <atomic>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "gameplay/PlayRequirementPolicy.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/PlayValidation.hpp"
#include "engine/TriggerEligibility.hpp"
#include "engine/EffectGuards.hpp"
namespace CustomComponents::PlayRequirements {
using Obj=Engine::Match::Obj;
using IntList=BNM::Structures::Mono::List<int>;
struct Component:Obj {
    BNM_CustomClass(Component,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","PlayRequirement","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","EntityFilter","EngineLib.dll").Build(),{});
};
// Committed eligibility survives native model copies and is not recomputed
// after payment. This prevents resource requirements changing mid-resolution.
struct State:Obj {
    int Owner=-1;
    BNM::Structures::Mono::String* Results=nullptr;
    BNM_CustomClass(State,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","PlayRequirementState","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(Owner,BNM::Defaults::Get<int>(),"Owner");
    BNM_CustomField(Results,BNM::Defaults::Get<BNM::Structures::Mono::String*>(),"Results");
    void SetResults(BNM::Structures::Mono::String* text){if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&Results),text);else Results=text;}
    void DeepCopyFrom(Obj* source,Obj*){auto* state=reinterpret_cast<State*>(source);Owner=state?state->Owner:-1;SetResults(state?state->Results:nullptr);}
    void HandleReturnToPool(Obj* instances){DeepCopyFrom(nullptr,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline int FieldInt(Obj* object,const char* name){if(!object)return -1;auto field=BNM::Class(object).GetField(name).cast<int>();return field.IsValid()?field[object].Get():-1;}
inline bool IsTrick(Obj* card){return Engine::EntityAccess::Component(card,"Burst")!=nullptr;}
inline bool UsesRequirements(Obj* model,Obj* card) {
    if(!IsTrick(card))return false;
    Obj* association=Engine::EntityAccess::Component(card,"EffectEntities");
    static auto metadata=BNM::Class("PvZCards.Engine.Components","EntityAssociation").GetField("EntityIds").cast<IntList*>();auto field=metadata;
    auto* list=association?field[association].Get():nullptr;
    if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return false;
    for(int i=0;i<list->size;++i){Obj* effect=Engine::EntityAccess::Find(model,list->items->m_Items[i]);if(Engine::EntityAccess::Component(effect,"PlayTrigger") && Engine::EntityAccess::Component(effect,"PlayRequirement"))return true;}
    return false;
}
inline int Source(Obj* effect){static auto metadata=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();auto field=metadata;Obj* link=Engine::EntityAccess::Component(effect,"EffectEntity");return link?field[link].Get():-1;}
inline Obj* Model(Obj* context){static auto metadata=BNM::Class("PvZCards.Engine","EntityQueryContext").GetField("<Model>k__BackingField").cast<Obj*>();auto field=metadata;return context?field[context].Get():nullptr;}
inline bool Matches(Obj* requirement,Obj* card,Obj* context) {
    static auto queryMetadata=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();auto field=queryMetadata;
    Obj* query=requirement && field.IsValid()?field[requirement].Get():nullptr;
    if(!query || !context)return false;
    auto matches=BNM::Class(query).GetMethod("Matches",2).cast<bool>();if(!matches.IsValid())return false;
    bool allowed=false;auto exception=BNM::TryInvoke([&]{allowed=matches[query](card,context);});
    if(exception.IsValid()){static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<8)LOG_ERR("CustomComponents/PlayRequirement: query failed; ability rejected");}
    return !exception.IsValid() && allowed;
}
inline std::vector<Decision> Evaluate(Obj* model,Obj* card) {
    std::vector<Decision> decisions;if(!model || !IsTrick(card))return decisions;
    Obj* association=Engine::EntityAccess::Component(card,"EffectEntities");
    static auto idsMetadata=BNM::Class("PvZCards.Engine.Components","EntityAssociation").GetField("EntityIds").cast<IntList*>();auto ids=idsMetadata;
    IntList* list=association?ids[association].Get():nullptr;
    if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return decisions;
    for(int i=0;i<list->size;++i){Obj* effect=Engine::EntityAccess::Find(model,list->items->m_Items[i]);if(effect && Engine::EntityAccess::Component(effect,"PlayTrigger"))decisions.push_back({list->items->m_Items[i],Engine::EntityAccess::Component(effect,"PlayRequirement")!=nullptr,true});}
    if(!HasRequirements(decisions))return decisions;
    Obj* instances=Engine::Match::Instances(card);static auto type=BNM::Class("PvZCards.Engine","EntityQueryContext");
    Obj* context=Core::ManagedPool::Acquire(instances,type);Core::ManagedRuntime::Root root(context);
    struct Return {Obj* instances;Obj* context;~Return(){Core::ManagedPool::Return(instances,context);}}giveBack{instances,context};
    auto modelField=type.GetField("<Model>k__BackingField").cast<Obj*>();auto selfField=type.GetField("SelfId").cast<int>();
    auto source=type.GetMethod("set_SourceId",1).cast<void>();auto target=type.GetMethod("set_TargetId",1).cast<void>();auto effectId=type.GetMethod("set_EffectEntityId",1).cast<void>();
    const bool ready=context && modelField.IsValid() && selfField.IsValid() && source.IsValid() && target.IsValid() && effectId.IsValid();
    if(ready){modelField[context].Set(model);selfField[context].Set(Engine::Match::Id(card));source[context](Engine::Match::Id(card));target[context](-1);}
    for(auto& decision:decisions)if(decision.required){Obj* effect=Engine::EntityAccess::Find(model,decision.effect);if(ready)effectId[context](decision.effect);decision.allowed=ready && Matches(Engine::EntityAccess::Component(effect,"PlayRequirement"),card,context);}
    return decisions;
}
inline bool Validate(const Engine::PlayValidation::Context& context,int){
    Obj* card=Engine::EntityAccess::Find(context.model,FieldInt(context.play,"CardId"));
    auto decisions=Evaluate(context.model,card);if(!HasRequirements(decisions))return true;
    for(auto& decision:decisions)if(decision.allowed){Obj* effect=Engine::EntityAccess::Find(context.model,decision.effect);decision.targets=effect && Engine::PlayValidation::HasPrimaryQueryMatch(context.model,card,effect);}
    return MayCast(decisions);
}
inline bool Ability(const Engine::PlayValidation::Ability& context,int){
    if(!IsTrick(context.card) || !Engine::EntityAccess::Component(context.effect,"PlayTrigger") || !Engine::EntityAccess::Component(context.effect,"PlayRequirement"))return true;
    for(auto decision:Evaluate(context.model,context.card))if(decision.effect==Engine::Match::Id(context.effect))return decision.allowed;
    return false;
}
inline bool Prepare(const Engine::EffectGuards::Context& context) {
    if(!context.effect || !context.effect->klass || std::string_view(context.effect->klass->name)!="PlayCardEffect")return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    Obj* card=Engine::EntityAccess::Find(model,FieldInt(context.effect,"TargetId"));
    auto decisions=Evaluate(model,card);if(!HasRequirements(decisions))return true;
    const auto reject=[&](){auto cancel=BNM::Class("PvZCards.Engine","Effect").GetMethod("Cancel",0).cast<void>();if(cancel.IsValid())cancel[context.effect]();return false;};
    if(!MayPlay(decisions))return reject();
    Obj* component=Engine::Match::Component(card,BNM::Class("PvZCards.Engine.Components","PlayRequirementState").GetMonoType(),true);
    if(!component)return reject();
    auto* state=reinterpret_cast<State*>(component);state->Owner=Engine::Match::Id(card);state->SetResults(BNM::CreateMonoString(Encode(decisions)));
    return state->Results?true:reject();
}
inline bool Trigger(const Engine::TriggerEligibility::Context& context,int) {
    Obj* requirement=Engine::EntityAccess::Component(context.effectEntity,"PlayRequirement");
    if(!requirement || !Engine::EntityAccess::Component(context.effectEntity,"PlayTrigger"))return true;
    Obj* model=Model(context.query);Obj* card=Engine::EntityAccess::Find(model,Source(context.effectEntity));if(!model || !card)return false;if(!IsTrick(card))return true;
    auto* state=reinterpret_cast<State*>(Engine::EntityAccess::Component(card,"PlayRequirementState"));
    const auto processing=Engine::EffectGuards::current;
    const bool committing=processing.effect && processing.effect->klass && std::string_view(processing.effect->klass->name)=="PlayCardEffect" && FieldInt(processing.effect,"TargetId")==Engine::Match::Id(card);
    if(state && state->Owner==Engine::Match::Id(card) && state->Results && (committing || !Engine::EntityAccess::Component(card,"HandElement"))){
        bool allowed=false;return Lookup(state->Results->str(),Engine::Match::Id(context.effectEntity),allowed) && allowed;
    }
    return Matches(requirement,card,context.query);
}
inline bool Initialize() {
    Engine::PlayValidation::usesRequirements=UsesRequirements;
    auto marker=BNM::Class("PvZCards.Engine.Components","MarkerComponent");
    return CustomComponentRegistry::Register({"PlayRequirement","PvZCards.Engine.Components","EngineLib.dll","EntityFilter","PvZCards.Engine.Components","EngineLib.dll"}) &&
        CustomComponentRegistry::Register({"PlayRequirementState","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","PlayRequirementState"),marker) &&
        Engine::PlayValidation::InitializeAbilitySelection() && Engine::PlayValidation::rules.Add(Validate) && Engine::PlayValidation::abilityRules.Add(Ability) &&
        Engine::TriggerEligibility::rules.Add(Trigger) && Engine::EffectGuards::RegisterPreparation(Prepare,-10);
}
}
