#pragma once

#include <array>
#include <optional>
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Method.hpp>
#include "core/HookRegistry.hpp"
#include "core/NativeConstruction.hpp"
#include "engine/EffectDurations.hpp"
#include "core/Performance.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/MatchAccess.hpp"
#include "queries/LaneQueries.hpp"
#include "log.h"
#include "variables/Storage.hpp"

namespace CustomComponents::CostModifierLifetime {
using Obj=BNM::IL2CPP::Il2CppObject;
// Implement the stock IntModifierMatcher interface. The native buff removers
// then produce AttackBuffExpirationRecord / HealthBuffExpirationRecord and
// perform their usual recalculation, rather than bypassing stock history.
struct DurationMatcher : Obj {
    int Duration=0;
    BNM_CustomClass(DurationMatcher,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Utilities","CustomDurationMatcher","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("System","Object","mscorlib.dll").Build(),{},
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Utilities","IntModifierMatcher","EngineLib.dll").Build());
    BNM_CustomField(Duration,BNM::Defaults::Get<int>(),"Duration");
    void Construct(){Core::NativeConstruction::Base(this);}
    bool Matches(Obj* modifier){
        static auto field=BNM::Class("PvZCards.Engine.Components.Data","IntModifier").GetField("Duration").cast<int>();
        return modifier && field.IsValid() && field[modifier].Get()==Duration;
    }
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components.Data","IntModifier","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
};
using ObjList=BNM::Structures::Mono::List<Obj*>;
inline Obj* Model(Obj* system,const char* systemName,const char* field){
    auto f=BNM::Class("PvZCards.Core.Systems",systemName).GetField(field).cast<Obj*>();
    return system && f.IsValid()?f[system].Get():nullptr;
}
inline Obj* Target(Obj* model,Obj* effect){
    static const auto target=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();auto field=target;
    return model && effect && field.IsValid()?Engine::EntityAccess::Find(model,field[effect].Get()):nullptr;
}
inline void Clear(Obj* card,int duration){
    Obj* cost=Engine::EntityAccess::Component(card,"SunCost");
    if(!cost)return;
    static auto amountMetadata=BNM::Class("PvZCards.Engine.Components","SunCost").GetField("SunCostValue").cast<Obj*>();
    auto amountField=amountMetadata;
    Obj* amount=amountField.IsValid()?amountField[cost].Get():nullptr;
    if(!amount)return;
    static auto valueType=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt");
    static auto modsMetadata=valueType.GetField("Modifiers").cast<ObjList*>();
    auto mods=modsMetadata;
    ObjList* list=mods.IsValid()?mods[amount].Get():nullptr;
    if(!Engine::EntityAccess::Valid(list) || list->size==0)return;
    static auto removeMetadata=valueType.GetMethod("RemoveModifier",2).cast<void>();
    static auto durationMetadata=BNM::Class("PvZCards.Engine.Components.Data","IntModifier").GetField("Duration").cast<int>();
    auto remove=removeMetadata;auto durationField=durationMetadata;
    Obj* instances=Engine::Match::Instances(card);
    if(!remove.IsValid() || !durationField.IsValid() || !instances)return;
    for(int i=list->size-1;i>=0;--i){
        Obj* modifier=list->items->m_Items[i];
        if(modifier && (duration<0 || durationField[modifier].Get()==duration))
            remove[amount](modifier,instances);
    }
}
inline void ClearOnAllCards(Obj* model,int duration){
    Core::Performance::Scope timing(Core::Performance::Area::Maintenance,"cost-expiry-scan");
    auto api=Engine::EntityAccess::Api();
    ObjList* entities=model && api.entities.IsValid()?api.entities[model].Get():nullptr;
    if(!Engine::EntityAccess::Valid(entities))return;
    // Enumerate all card entities, not only cards currently on the board:
    // cost modifiers may have been applied to cards in hands and decks.
    for(int i=0;i<entities->size;++i){
        Obj* card=entities->items->m_Items[i];
        if(Engine::EntityAccess::Component(card,"Card"))Clear(card,duration);
    }
}
inline bool HasModifier(Obj* card,const char* componentName,BNM::Field<Obj*> amountField,int duration){
    Obj* component=Engine::EntityAccess::Component(card,componentName);
    if(!component)return false;

    Obj* amount=amountField.IsValid()?amountField[component].Get():nullptr;
    if(!amount)return false;
    static const auto modsField=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt").GetField("Modifiers").cast<ObjList*>();auto mods=modsField;
    ObjList* list=mods.IsValid()?mods[amount].Get():nullptr;
    if(!Engine::EntityAccess::Valid(list))return false;
    static const auto durationMetadata=BNM::Class("PvZCards.Engine.Components.Data","IntModifier").GetField("Duration").cast<int>();auto durationField=durationMetadata;
    if(!durationField.IsValid())return false;
    for(int i=0;i<list->size;++i){
        Obj* mod=list->items->m_Items[i];
        if(mod && durationField[mod].Get()==duration)return true;
    }
    return false;
}
// Use the game's typed modifier-removal APIs for the new phase boundaries;
// never remove the complete stack or an unrelated source's modifier.
inline void ExpireNativeBuffs(Obj* model,int duration){
    if(!model || duration<2 || !Engine::EffectDurations::Timed(duration))return;
    auto api=Engine::EntityAccess::Api();
    ObjList* entities=api.entities.IsValid()?api.entities[model].Get():nullptr;
    if(!Engine::EntityAccess::Valid(entities))return;
    Core::Performance::Scope timing(Core::Performance::Area::NativeBuffExpiry,"phase-expiry");
    static constexpr const char* components[]={"Attack","Health","SunCost"};
    static constexpr const char* amounts[]={"AttackValue","MaxHealth","SunCostValue"};
    static constexpr const char* services[]={"AttackBuffRemover","HealthBuffRemover","SunCostBuffRemover"};
    static const auto amountFields=[]{
        std::array<BNM::Field<Obj*>,3> result;
        for(std::size_t i=0;i<result.size();++i)
            result[i]=BNM::Class("PvZCards.Engine.Components",components[i]).GetField(amounts[i]).cast<Obj*>();
        return result;
    }();
    std::array<std::shared_ptr<Core::ManagedRuntime::Root>,3> removers{};
    std::array<BNM::Method<void>,3> methods{};
    std::optional<Core::ManagedRuntime::Root> matcher;
    for(int i=0,count=entities->size;i<count;++i){
        Obj* card=entities->items->m_Items[i];
        if(!Engine::EntityAccess::Component(card,"Card"))continue;
        for(std::size_t stat=0;stat<removers.size();++stat){
            if(!HasModifier(card,components[stat],amountFields[stat],duration))continue;

            // Most plays have no expiring modifier. Resolve services and
            // create the native matcher only when there is work to do.
            if(!matcher){
                static const auto type=BNM::Class("PvZCards.Engine.Utilities","CustomDurationMatcher");
                static const auto ctorMetadata=type.GetMethod(".ctor",0).cast<void>();
                static const auto durationMetadata=type.GetField("Duration").cast<int>();
                auto ctor=ctorMetadata;
                auto durationField=durationMetadata;
                if(!type.IsValid() || !ctor.IsValid() || !durationField.IsValid())return;
                matcher.emplace(type.CreateNewInstance());
                if(!matcher->Get())return;
                ctor[matcher->Get()]();
                durationField[matcher->Get()].Set(duration);
            }
            if(!removers[stat]){
                const auto type=BNM::Class("PvZCards.Core.Systems",services[stat]);
                removers[stat]=Engine::Match::Service(Engine::Match::Instances(card),type);
                methods[stat]=type.GetMethod("RemoveBuffsAndRecordInHistory",2).cast<void>();
                if(!removers[stat] || !removers[stat]->Get() || !methods[stat].IsValid()){
                    LOG_ERR("CustomComponents/Duration: %s unavailable",services[stat]);return;
                }
            }
            methods[stat][removers[stat]->Get()](card,matcher->Get());
        }
    }
}
inline void OnExpiryBoundary(Obj* model,int duration){
    if(!model || !Engine::EffectDurations::Timed(duration))return;
    if(duration==1){
        ClearOnAllCards(model,1);Variables::ExpireTimed(model,1);
    }else{
        // NextFighter and all future custom phase boundaries also expire
        // ordinary Attack/Health/SunCost IntModifiers via stock removers.
        ExpireNativeBuffs(model,duration);
        Variables::ExpireTimed(model,duration);
    }
    // The play-to-lane hook below retains the original new-fighter guard for
    // cases where that boundary precedes stock NextFighterAbilityRemovalSystem.
}
// Disables only the stock on-play cost reset. Other PlayCardEffect
// handlers, cost payment and validation still run through the router.
inline void(*originalResetOnPlay)(Obj*,Obj*,const void*)=nullptr;
inline void NoResetOnPlay(Obj*,Obj*,const void*){}
inline void(*originalBounce)(Obj*,Obj*,const void*)=nullptr;
inline void OnBounce(Obj* self,Obj* effect,const void* method){
    Obj* card=Target(Model(self,"ReturnToHandFromPlaySystem","<Model>k__BackingField"),effect);
    if(card && Queries::LaneTopology::LaneEntity(Model(self,"ReturnToHandFromPlaySystem","<Model>k__BackingField"),card))Clear(card,-1);
    originalBounce(self,effect,method);
}
inline void(*originalDiscard)(Obj*,Obj*,const void*)=nullptr;
inline void OnDiscard(Obj* self,Obj* effect,const void* method){
    Obj* card=Target(Model(self,"DiscardFromPlaySystem","<Model>k__BackingField"),effect);
    if(card && Queries::LaneTopology::LaneEntity(Model(self,"DiscardFromPlaySystem","<Model>k__BackingField"),card))Clear(card,-1);
    originalDiscard(self,effect,method);
}
inline void(*originalTurnEnd)(Obj*,Obj*,const void*)=nullptr;
inline void OnTurnEnd(Obj* self,Obj* effect,const void* method){
    // Stock handles Attack/Health EndOfTurn expirations. The central
    // duration observer handles SunCost and variable overlays after the
    // authoritative ability-expiration boundary.
    Core::Performance::Scope timing(Core::Performance::Area::Maintenance,"native-turn-buff-removal");
    originalTurnEnd(self,effect,method);
}
inline void(*originalPlayFighter)(Obj*,Obj*,const void*)=nullptr;
inline void OnPlayFighter(Obj* self,Obj* effect,const void* method){
    Core::Performance::Scope timing(Core::Performance::Area::FighterPlacement,"fighter-handler");
    Obj* model=Model(self,"PlayFighterToLanesSystem","<Model>k__BackingField");
    Obj* card=Target(model,effect);
    const bool alreadyOnBoard=card && Queries::LaneTopology::LaneEntity(model,card);
    {
        Core::Performance::Scope stock(Core::Performance::Area::FighterPlacement,"stock-place-fighter");
        originalPlayFighter(self,effect,method);
    }
    // NextFighter expires only after a NEW fighter entered play.
    if(card && !alreadyOnBoard && Queries::LaneTopology::LaneEntity(model,card))
    {
        Core::Performance::Scope expiry(Core::Performance::Area::FighterExpiry,"next-fighter-expiry");
        ClearOnAllCards(model,2);
        ExpireNativeBuffs(model,2);
        Variables::ExpireTimed(model,2);
    }
}
inline bool Initialize(){
    using namespace CustomComponents::Core;
    return Hooks::Install(BNM::Class("PvZCards.Core.Systems","ResetSunCostOnPlaySystem").GetMethod("ProcessEffect",1),NoResetOnPlay,originalResetOnPlay) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","ReturnToHandFromPlaySystem").GetMethod("ProcessEffect",1),OnBounce,originalBounce) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","DiscardFromPlaySystem").GetMethod("ProcessEffect",1),OnDiscard,originalDiscard) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","EndOfTurnBuffRemovalSystem").GetMethod("ProcessEffect",1),OnTurnEnd,originalTurnEnd) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","PlayFighterToLanesSystem").GetMethod("ProcessEffect",1),OnPlayFighter,originalPlayFighter) &&
        Engine::AbilityDurationEvents::expire.Add(OnExpiryBoundary);
}
}
