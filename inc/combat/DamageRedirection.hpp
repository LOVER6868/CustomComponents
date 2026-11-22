#pragma once
#include "history/DomainRecords.hpp"
#include <atomic>
#include "engine/BoardPlacement.hpp"
#include "core/HookRegistry.hpp"
#include "core/MetadataBinding.hpp"
#include "core/SceneViewContext.hpp"
#include "engine/EffectCreationEvents.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/EffectLifecycleEvents.hpp"
#include "core/WeakObjectSettings.hpp"

namespace CustomComponents::Combat::DamageRedirection {
using Obj=BNM::IL2CPP::Il2CppObject;
using IntList=BNM::Structures::Mono::List<int>;
inline thread_local Obj* incoming=nullptr;
inline thread_local unsigned frontQueried=0,frontAccepted=0;
inline std::atomic<unsigned> diagnosticBudget{0};
inline void (*originalProcess)(Obj*,Obj*,const void*)=nullptr;
inline bool (*originalFront)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline Obj* (*originalSelect)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline bool (*originalValidTargets)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline BNM::Field<Obj*> contextModel;
inline BNM::Field<int> contextSelf,effectTarget;
inline BNM::Field<IntList*> lanes;
inline BNM::Method<IntList*> fighters;
inline BNM::Method<int> damageAmount;
inline BNM::Field<int> currentDamage;
inline BNM::Field<bool> cancelled;
struct Origin {int target=-1;std::uint64_t record=0;};
inline Core::WeakObjectSettings<Origin> origins;
struct DamageTrace {unsigned long long event;int target,damage;Origin origin;};
inline thread_local std::vector<DamageTrace> traces;
inline bool Scene(Obj* model){return model && reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity();}
inline void Created(Obj*,Obj* context,Obj* result){
    if(!incoming || !context || Core::Performance::aiDepth)return;
    const auto* list=reinterpret_cast<Engine::EntityAccess::List*>(result);if(!Engine::EntityAccess::Valid(list))return;
    static std::atomic<unsigned> samples{0};
    for(int i=0;i<list->size;++i){Obj* effect=list->items->m_Items[i];
        if(effect && effect->klass && std::string_view(effect->klass->name)=="RedirectedDamageEffect"){
            const int original=Core::Metadata::Bind(effectTarget,incoming).Get(),target=Core::Metadata::Bind(effectTarget,effect).Get();
            const auto id=History::Domain::Emit(Core::Metadata::Bind(contextModel,context).Get(),History::Kind::DamageRule,
                History::DamageRulePayload{"redirection","created",original,target,Core::Metadata::Bind(damageAmount,incoming)(),Core::Metadata::Bind(damageAmount,effect)(),false},target);
            origins.Set(effect,{original,id});
        }
        if(effect && effect->klass && std::string_view(effect->klass->name)=="RedirectedDamageEffect" && samples.fetch_add(1)<80)
            LOG_INFO("CustomComponents/DamageRedirect/CREATED: originalTarget=%d target=%d amount=%d",Core::Metadata::Bind(effectTarget,incoming).Get(),Core::Metadata::Bind(effectTarget,effect).Get(),Core::Metadata::Bind(damageAmount,effect)());
    }
}
inline void Before(const Engine::EffectGuards::Context& context){
    Obj* effect=context.effect;if(!effect || !effect->klass || std::string_view(effect->klass->name)!="RedirectedDamageEffect")return;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));if(!model || Core::Performance::aiDepth)return;
    const int id=Core::Metadata::Bind(effectTarget,effect).Get();
    Obj* health=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,id),"Health");
    traces.push_back({context.event,id,health?Core::Metadata::Bind(currentDamage,health).Get():-1,origins.Get(effect)});
}
inline void After(const Engine::EffectGuards::Context& context){
    if(traces.empty() || traces.back().event!=context.event)return;
    const auto trace=traces.back();traces.pop_back();
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    Obj* health=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,trace.target),"Health");
    History::Domain::Emit(model,History::Kind::DamageRule,
        History::DamageRulePayload{"redirection","resolved",trace.origin.target,trace.target,trace.damage,
            health?Core::Metadata::Bind(currentDamage,health).Get():-1,Core::Metadata::Bind(cancelled,context.effect).Get()},trace.target,trace.origin.record);
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<80)
        LOG_INFO("CustomComponents/DamageRedirect/RESULT: target=%d amount=%d damageBefore=%d damageAfter=%d cancelled=%d",trace.target,Core::Metadata::Bind(damageAmount,context.effect)(),trace.damage,health?Core::Metadata::Bind(currentDamage,health).Get():-1,int(Core::Metadata::Bind(cancelled,context.effect).Get()));
}

static void Process(Obj* system,Obj* effect,const void* method){
    struct Scope {Obj* previous=incoming;~Scope(){incoming=previous;}} scope;
    const unsigned previousQueries=frontQueried,previousMatches=frontAccepted;
    frontQueried=frontAccepted=0;
    incoming=effect;
    const int target=effect?Core::Metadata::Bind(effectTarget,effect).Get():-1;
    originalProcess(system,effect,method);
    if(diagnosticBudget.fetch_add(1)<128)
        LOG_INFO("CustomComponents/DamageRedirect/PROCESS: effect=%p target=%d frontQueries=%u frontMatches=%u originalCompleted=1",
            effect,target,frontQueried,frontAccepted);
    frontQueried=previousQueries;frontAccepted=previousMatches;
}
static bool ValidTargets(Obj* system,Obj* effectEntity,Obj* context,const void* method){
    const bool valid=originalValidTargets(system,effectEntity,context,method);
    static std::atomic<unsigned> samples{0};
    if(context && Scene(Core::Metadata::Bind(contextModel,context).Get()) && samples.fetch_add(1)<160)
        LOG_INFO("CustomComponents/DamageRedirect/VALID_TARGETS: effectEntity=%d contextSelf=%d allowed=%d effect=%p queryContext=%p",
            Engine::Match::Id(effectEntity),Core::Metadata::Bind(contextSelf,context).Get(),int(valid),incoming,context);
    return valid;
}
static Obj* Select(Obj* system,Obj* candidates,Obj* context,const void* method){
    Obj* selected=originalSelect(system,candidates,context,method);
    static std::atomic<unsigned> samples{0};
    if(context && Scene(Core::Metadata::Bind(contextModel,context).Get()) && samples.fetch_add(1)<160){
        const int self=Core::Metadata::Bind(contextSelf,context).Get();
        Obj* model=Core::Metadata::Bind(contextModel,context).Get();
        Obj* origin=Engine::EntityAccess::Find(model,self);
        LOG_INFO("CustomComponents/DamageRedirect/SELECT: target=%d selected=%d contextSelf=%d environment=%d originOwner=%d candidateCollection=%p frontQueries=%u frontAccepted=%u",
            incoming?Core::Metadata::Bind(effectTarget,incoming).Get():-1,Engine::Match::Id(selected),self,
            int(Engine::EntityAccess::Component(origin,"Environment")!=nullptr),Engine::BoardPlacement::Owner(origin),
            candidates,frontQueried,frontAccepted);
    }
    return selected;
}
static bool InFront(Obj* query,Obj* candidate,Obj* context,const void* method){
    if(!incoming || !candidate || !context)return originalFront(query,candidate,context,method);
    ++frontQueried;
    Obj* model=Core::Metadata::Bind(contextModel,context).Get();
    Obj* origin=Engine::EntityAccess::Find(model,Core::Metadata::Bind(contextSelf,context).Get());
    if(!Engine::EntityAccess::Component(origin,"Environment")){
        const bool native=originalFront(query,candidate,context,method);
        if(native)++frontAccepted;
        return native;
    }
    const int target=Engine::Match::Id(candidate);
    // A redirect to the same victim is not a replacement and can recurse.
    if(target==Core::Metadata::Bind(effectTarget,incoming).Get())return false;
    Obj* position=Engine::EntityAccess::Component(origin,"LaneElement");
    auto* ids=position?Core::Metadata::Bind(lanes,position).Get():nullptr;
    if(!Engine::EntityAccess::Valid(ids))return false;
    const int owner=Engine::BoardPlacement::Owner(origin);
    if(Engine::BoardPlacement::Owner(candidate)!=owner)return false;
    for(int i=0;i<ids->size;++i){
        Obj* lane=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,ids->items->m_Items[i]),"Lane");
        auto* cards=lane?Core::Metadata::Bind(fighters,lane)(owner):nullptr;
        if(!Engine::EntityAccess::Valid(cards))continue;
        const int victim=Core::Metadata::Bind(effectTarget,incoming).Get();
        int firstEligible=-1;
        for(int j=0;j<cards->size;++j){
            const int id=cards->items->m_Items[j];
            if(id==victim)continue;
            auto* fighter=Engine::EntityAccess::Find(model,id);
            if(!fighter||!Engine::EntityAccess::Component(fighter,"Health")||
               Engine::EntityAccess::Component(fighter,"MarkedForDeath"))continue;
            firstEligible=id;
            break;
        }
        if(firstEligible==target){
            ++frontAccepted;
            if(diagnosticBudget.fetch_add(1)<128)
                LOG_INFO("CustomComponents/DamageRedirect/FRONT_ACCEPT: target=%d originalVictim=%d contextSelf=%d lane=%d owner=%d",
                    target,Core::Metadata::Bind(effectTarget,incoming).Get(),Core::Metadata::Bind(contextSelf,context).Get(),ids->items->m_Items[i],owner);
            return true;
        }
    }
    if(diagnosticBudget.fetch_add(1)<128)
        LOG_INFO("CustomComponents/DamageRedirect/FRONT_REJECT: target=%d originalVictim=%d contextSelf=%d owner=%d laneCount=%d reason=NOT_FIRST_SAME_OWNER_FIGHTER",
            target,Core::Metadata::Bind(effectTarget,incoming).Get(),Core::Metadata::Bind(contextSelf,context).Get(),owner,ids->size);
    return false;
}
inline bool Initialize(){
    if(!Engine::EffectLifecycleEvents::copied.Add([](Obj* source,Obj* copy){origins.Set(copy,origins.Get(source));}) ||
       !Engine::EffectLifecycleEvents::returned.Add([](Obj* effect){origins.Set(effect,{});}))return false;
    auto context=BNM::Class("PvZCards.Engine","EntityQueryContext");
    contextModel=context.GetField("<Model>k__BackingField").cast<Obj*>();
    contextSelf=context.GetField("SelfId").cast<int>();
    effectTarget=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    lanes=BNM::Class("PvZCards.Engine.Components","LaneElement").GetField("LaneIds").cast<IntList*>();
    fighters=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetFightersForPlayer",1).cast<IntList*>();
    auto system=BNM::Class("PvZCards.Core.Systems","DamageEffectRedirectorSystem");
    damageAmount=BNM::Class("PvZCards.Engine.Effects","DamageEffect").GetMethod("get_DamageAmount",0).cast<int>();
    currentDamage=BNM::Class("PvZCards.Engine.Components","Health").GetField("CurrentDamage").cast<int>();
    cancelled=BNM::Class("PvZCards.Engine","Effect").GetField("Cancelled").cast<bool>();
    return contextModel.IsValid() && contextSelf.IsValid() && effectTarget.IsValid() && lanes.IsValid() && fighters.IsValid() && damageAmount.IsValid() && currentDamage.IsValid() && cancelled.IsValid() &&
        Engine::EffectCreationEvents::created.Add(Created) && Engine::EffectGuards::before.Add(Before) && Engine::EffectGuards::after.Add(After) &&
        Core::Hooks::Install(system.GetMethod("ProcessEffect",1),Process,originalProcess) &&
        Core::Hooks::Install(system.GetMethod("EffectEntityHasValidTargets",2),ValidTargets,originalValidTargets) &&
        Core::Hooks::Install(system.GetMethod("SelectReplacementEffectEntity",2),Select,originalSelect) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Engine.Queries","InFrontSameLaneQuery").GetMethod("Matches",2),InFront,originalFront);
}
}
