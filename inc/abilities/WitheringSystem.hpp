#include "core/NativeConstruction.hpp"
#pragma once
#include "core/WeakObjectSettings.hpp"
#include "engine/EffectLifecycleEvents.hpp"
#include "zones/CardZoneMembership.hpp"
#include "abilities/Withering.hpp"
#include "engine/EffectRegistration.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/EffectContinuations.hpp"
#include "engine/BoardPlacement.hpp"

#include "effects/DiscardFromHandEffectDescriptor.hpp"
#include "effects/DiscardFromDeckEffectDescriptor.hpp"
#include <unordered_map>
#include <map>
#include "core/SceneViewContext.hpp"

#include <unordered_set>
namespace CustomComponents::Abilities::Withering {
inline std::mutex pendingMutex;
inline std::unordered_map<Core::ModelRegistry::Token,std::unordered_set<int>> pendingByModel;
inline Core::WeakObjectSettings<int> ownedEffects;
struct HandEffect:Obj {
    std::uint8_t inherited[0x40]{};
    BNM_CustomClass(HandEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","WitheringHandEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","MoveToDiscardPileEffect","EngineLib.dll").Build(),{});
    // Stock InstanceManager.CreateInstance uses Activator.CreateInstance,
    // which requires a reflected public parameterless constructor.
    void Construct() {Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
};
struct DeckEffect:Obj {
    std::uint8_t inherited[0x40]{};
    BNM_CustomClass(DeckEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","WitheringDeckEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","DiscardFromDeckEffect","EngineLib.dll").Build(),{});
    // Stock InstanceManager.CreateInstance uses Activator.CreateInstance,
    // which requires a reflected public parameterless constructor.
    void Construct() {Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
};
struct BoardEffect:Obj {
    std::uint8_t inherited[0x38]{};
    BNM_CustomClass(BoardEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","WitheringBoardEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","DestroyCardEffect","EngineLib.dll").Build(),{});
    // Stock InstanceManager.CreateInstance uses Activator.CreateInstance,
    // which requires a reflected public parameterless constructor.
    void Construct() {Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
};
struct Route {const char* runtime;const char* base;int mode;};
inline constexpr Route routes[]={{"WitheringHandEffect","MoveToDiscardPileEffect",1},{"WitheringDeckEffect","DiscardFromDeckEffect",2},{"WitheringBoardEffect","DestroyCardEffect",3}};
inline int RuntimeMode(Obj* effect) {
    if(!effect || !effect->klass || !effect->klass->name)return 0;
    for(const auto& route:routes)if(std::string_view(effect->klass->name)==route.runtime)return route.mode;
    for(const auto& route:routes)if(std::string_view(effect->klass->name)==route.base)return ownedEffects.Get(effect);
    return 0;
}
inline bool InZone(Obj* model,int card,int mode) {
    Obj* entity=Engine::EntityAccess::Find(model,card);if(!entity)return false;
    if(mode==2)return Zones::Membership::InDeck(model,card);
    return GrantModes::MatchesZone(mode,Engine::EntityAccess::Component(entity,"HandElement")!=nullptr,Engine::EntityAccess::Component(entity,"DeckElement")!=nullptr,Engine::EntityAccess::Component(entity,"LaneElement")!=nullptr);
}
inline bool Guard(const Engine::EffectGuards::Context& context,int) {
    int mode=RuntimeMode(context.effect);if(!mode)return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    auto target=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    const int card=target.IsValid()?target[context.effect].Get():-1;
    const bool valid=model && InZone(model,card,mode);
    if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity()){
        static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<64)LOG_INFO("CustomComponents/Withering/EXECUTE_ZONE: card=%d mode=%d valid=%d",card,mode,int(valid));}
    if(!valid){const auto token=Core::ModelRegistry::Identify(model,true);{std::lock_guard<std::mutex> lock(pendingMutex);auto it=pendingByModel.find(token);if(it!=pendingByModel.end())it->second.erase(card);}ownedEffects.Set(context.effect,0);}
    return valid;
}
inline void Completed(const Engine::EffectGuards::Context& context){
    const int mode=RuntimeMode(context.effect);if(!mode)return;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    static auto target=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    if(model && target.IsValid()){const auto token=Core::ModelRegistry::Identify(model,true);const int card=Core::Metadata::Bind(target,context.effect).Get();std::lock_guard<std::mutex> lock(pendingMutex);auto it=pendingByModel.find(token);if(it!=pendingByModel.end())it->second.erase(card);}
    ownedEffects.Set(context.effect,0);
}
inline void Register(Obj* router,Obj* type,Obj* handler,const void* method) {
    for(const auto& route:routes) {
        auto base=BNM::Class("PvZCards.Engine.Effects",route.base);
        if(type==reinterpret_cast<Obj*>(base.GetMonoType()))Engine::EffectRegistration::Alias(router,reinterpret_cast<Obj*>(BNM::Class("PvZCards.Engine.Effects",route.runtime).GetMonoType()),handler,method);
    }
}
inline void Schedule(Obj* instances,Obj* model) {
    if(!model || !instances)return;
    // One stable snapshot; newly granted Withering waits for the next boundary.
    struct Pending {int card;int mode;};std::vector<Pending> pending;
    auto api=Engine::EntityAccess::Api();auto* entities=api.entities[model].Get();if(!Engine::EntityAccess::Valid(entities))return;
    for(int i=0;i<entities->size;++i) {
        Obj* card=entities->items->m_Items[i];
        Obj* withering=Engine::EntityAccess::Component(card,"Withering");
        if(!withering)continue;
        int mode=Mode(withering);int id=Engine::Match::Id(card);
        if(mode && InZone(model,id,mode))pending.push_back({id,mode});
    }
    // The native turn-end effect can be retried after a presentation failure.
    // Never queue a second discard for the same physical card while the first
    // discard is pending. Re-arm only once the card has actually left that zone.
    const auto token=Core::ModelRegistry::Identify(model,true);
    std::unordered_set<int> queued;
    {std::lock_guard<std::mutex> guard(pendingMutex);queued=pendingByModel[token];}
    for(auto it=queued.begin();it!=queued.end();){
        Obj* card=Engine::EntityAccess::Find(model,*it);
        const int mode=Mode(static_cast<Obj*>(Get(card,false)));
        if(!mode || !InZone(model,*it,mode))it=queued.erase(it);
        else ++it;
    }
    {std::lock_guard<std::mutex> guard(pendingMutex);pendingByModel[token]=queued;}
    pending.erase(std::remove_if(pending.begin(),pending.end(),[&](const Pending& p){return queued.count(p.card)>0;}),pending.end());
    if(pending.empty())return;
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<24)
        LOG_INFO("CustomComponents/Withering/QUEUE: pending=%zu usingStockEffectTypes=1",pending.size());
    Obj* router=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EffectRouter").GetMonoType());
    Obj* queue=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EffectCollection").GetMonoType());
    Obj* injectedModel=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EntityModel").GetMonoType());
    if(!router || !queue || injectedModel!=model){LOG_ERR("CustomComponents/Withering: model-bound queue unavailable");return;}
    Engine::RouterContext::RegisterRouter(router,model);
    auto effectType=BNM::Class("PvZCards.Engine","Effect");
    std::vector<Obj*> effects; effects.reserve(pending.size());
    std::vector<std::shared_ptr<Core::ManagedRuntime::Root>> roots;roots.reserve(pending.size());
    auto source=effectType.GetField("SourceId").cast<int>();auto target=effectType.GetField("TargetId").cast<int>();auto player=effectType.GetField("PlayerId").cast<int>();auto effectId=effectType.GetField("EffectEntityId").cast<int>();
    auto validation=effectType.GetField("RequiresTargetValidation").cast<bool>();
    if(!validation.IsValid())return;
    for(std::size_t i=0;i<pending.size();++i) {
        // Use the actual registered native action. Generic registration may
        // bypass RegisterForEffect(Type,handler), so a subclass alias alone
        // does not guarantee that its zone processor will execute.
        auto item=pending[i];Obj* card=Engine::EntityAccess::Find(model,item.card);Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects",routes[item.mode-1].base));
        if(!effect){for(auto& root:roots)Core::ManagedPool::Return(instances,root->Get());return;}
        roots.push_back(std::make_shared<Core::ManagedRuntime::Root>(effect));
        ownedEffects.Set(effect,item.mode);
        source[effect].Set(item.card);target[effect].Set(item.card);player[effect].Set(Engine::BoardPlacement::Owner(card));effectId[effect].Set(-1);
        validation[effect].Set(false); // Engine-owned action: the zone guard validates it.
        if(item.mode==1)BNM::Class("PvZCards.Engine.Effects","MoveToDiscardPileEffect").GetField("DiscardPileId").cast<int>()[effect].Set(-1);
        if(item.mode==2){auto deck=BNM::Class("PvZCards.Engine.Effects","DiscardFromDeckEffect");deck.GetField("DeckId").cast<int>()[effect].Set(-1);deck.GetField("DiscardPileId").cast<int>()[effect].Set(-1);}
        effects.push_back(effect);
        if(samples.load()<80 && reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
            LOG_INFO("CustomComponents/Withering/ORDINARY_EFFECT: card=%d mode=%d effect=%p stock=%s turnEnd=1",
                item.card,item.mode,effect,routes[item.mode-1].base);
    }
    if(!Engine::EffectContinuations::Schedule(model,instances,effects,Engine::EffectContinuations::Placement::CurrentGroup)){
        for(auto& root:roots)Core::ManagedPool::Return(instances,root->Get());
        return;
    }
    {std::lock_guard<std::mutex> guard(pendingMutex);
     for(const Pending& item:pending)pendingByModel[token].insert(item.card);}
}

inline bool InitializeSystem(){
    if(!Engine::EffectGuards::after.Add(Completed) ||
       !Engine::EffectLifecycleEvents::copied.Add([](Obj* from,Obj* to){const int mode=RuntimeMode(from);if(mode)ownedEffects.Set(to,mode);}) ||
       !Engine::EffectLifecycleEvents::returned.Add([](Obj* effect){if(RuntimeMode(effect))ownedEffects.Set(effect,0);}))return false;
    Core::ModelRegistry::retired.Add([](auto token){std::lock_guard<std::mutex> guard(pendingMutex);pendingByModel.erase(token);});
    return Engine::EffectContinuations::Initialize() && Engine::EffectRegistration::afterRegister.Add(Register) && Engine::EffectGuards::rules.Add(Guard);
}
}
