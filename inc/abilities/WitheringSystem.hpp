#pragma once
#include "abilities/Withering.hpp"
#include "engine/EffectRegistration.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/BoardPlacement.hpp"
#include "effects/DiscardFromHandEffectDescriptor.hpp"
#include "effects/DiscardFromDeckEffectDescriptor.hpp"
namespace CustomComponents::Abilities::Withering {
struct HandEffect:Obj { BNM_CustomClass(HandEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","WitheringHandEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","MoveToDiscardPileEffect","EngineLib.dll").Build(),{}); };
struct DeckEffect:Obj { BNM_CustomClass(DeckEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","WitheringDeckEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","DiscardFromDeckEffect","EngineLib.dll").Build(),{}); };
struct BoardEffect:Obj { BNM_CustomClass(BoardEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","WitheringBoardEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","DestroyCardEffect","EngineLib.dll").Build(),{}); };
struct Route {const char* runtime;const char* base;int mode;};
inline constexpr Route routes[]={{"WitheringHandEffect","MoveToDiscardPileEffect",1},{"WitheringDeckEffect","DiscardFromDeckEffect",2},{"WitheringBoardEffect","DestroyCardEffect",3}};
inline int RuntimeMode(Obj* effect) {
    if(!effect || !effect->klass || !effect->klass->name)return 0;
    for(const auto& route:routes)if(std::string_view(effect->klass->name)==route.runtime)return route.mode;
    return 0;
}
inline bool InZone(Obj* model,int card,int mode) {
    Obj* entity=Engine::EntityAccess::Find(model,card);if(!entity)return false;
    return GrantModes::MatchesZone(mode,Engine::EntityAccess::Component(entity,"HandElement")!=nullptr,Engine::EntityAccess::Component(entity,"DeckElement")!=nullptr,Engine::EntityAccess::Component(entity,"LaneElement")!=nullptr);
}
inline bool Guard(const Engine::EffectGuards::Context& context,int) {
    int mode=RuntimeMode(context.effect);if(!mode)return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    auto target=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    return model && target.IsValid() && InZone(model,target[context.effect].Get(),mode);
}
inline void Register(Obj* router,Obj* type,Obj* handler,const void* method) {
    for(const auto& route:routes) {
        auto base=BNM::Class("PvZCards.Engine.Effects",route.base);
        if(type==reinterpret_cast<Obj*>(base.GetMonoType()))Engine::EffectRegistration::Alias(router,reinterpret_cast<Obj*>(BNM::Class("PvZCards.Engine.Effects",route.runtime).GetMonoType()),handler,method);
    }
}
inline void Schedule(Obj* system,Obj* turnEffect) {
    auto type=BNM::Class("PvZCards.Core.Systems","EndOfTurnAbilityRemovalSystem");
    auto finderField=type.GetField("<EntityFinder>k__BackingField").cast<Obj*>();auto instancesField=type.GetField("<Instances>k__BackingField").cast<Obj*>();
    auto modelField=BNM::Class("PvZCards.Engine","EntityFinder").GetField("Model").cast<Obj*>();
    Obj* finder=finderField[system].Get();Obj* instances=instancesField[system].Get();Obj* model=finder?modelField[finder].Get():nullptr;
    if(!model || !instances)return;
    // One stable snapshot; newly granted Withering waits for the next boundary.
    struct Pending {int card;int mode;};std::vector<Pending> pending;
    auto api=Engine::EntityAccess::Api();auto* entities=api.entities[model].Get();if(!Engine::EntityAccess::Valid(entities))return;
    for(int i=0;i<entities->size;++i) {
        Obj* card=entities->items->m_Items[i];int mode=Mode(static_cast<Obj*>(Get(card,false)));int id=Engine::Match::Id(card);
        if(mode && InZone(model,id,mode))pending.push_back({id,mode});
    }
    if(pending.empty())return;
    Obj* router=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EffectRouter").GetMonoType());
    Obj* queue=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EffectCollection").GetMonoType());
    Obj* injectedModel=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EntityModel").GetMonoType());
    if(!router || !queue || injectedModel!=model){LOG_ERR("CustomComponents/Withering: model-bound queue unavailable");return;}
    Engine::RouterContext::RegisterRouter(router,model);
    auto effectType=BNM::Class("PvZCards.Engine","Effect");
    BNM::Method<void> add;
    // Select the array overload explicitly; IEnumerable overload is also present.
    for(auto method:BNM::Class(queue).GetMethods())if(method.GetInfo() && std::string_view(method.GetInfo()->name)=="AddAfterCurrentGroup" && method.GetInfo()->parameters_count==1) {
        auto* info=method.GetInfo();if(info && BNM::Class(info->parameters[0]).GetClass() && BNM::Class(info->parameters[0]).GetClass()->rank==1){add=method.cast<void>();break;}
    }
    if(!add.IsValid())return;
    auto* array=effectType.NewArray<Obj*>(pending.size());Core::ManagedRuntime::Root arrayRoot(reinterpret_cast<Obj*>(array));if(!arrayRoot.Get())return;
    std::vector<std::shared_ptr<Core::ManagedRuntime::Root>> roots;roots.reserve(pending.size());
    auto source=effectType.GetField("SourceId").cast<int>();auto target=effectType.GetField("TargetId").cast<int>();auto player=effectType.GetField("PlayerId").cast<int>();auto effectId=effectType.GetField("EffectEntityId").cast<int>();
    for(std::size_t i=0;i<pending.size();++i) {
        auto item=pending[i];Obj* card=Engine::EntityAccess::Find(model,item.card);Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects",routes[item.mode-1].runtime));
        if(!effect){for(auto& root:roots)Core::ManagedPool::Return(instances,root->Get());return;}
        roots.push_back(std::make_shared<Core::ManagedRuntime::Root>(effect));
        source[effect].Set(item.card);target[effect].Set(item.card);player[effect].Set(Engine::BoardPlacement::Owner(card));effectId[effect].Set(-1);
        if(item.mode==1)BNM::Class("PvZCards.Engine.Effects","MoveToDiscardPileEffect").GetField("DiscardPileId").cast<int>()[effect].Set(-1);
        if(item.mode==2){auto deck=BNM::Class("PvZCards.Engine.Effects","DiscardFromDeckEffect");deck.GetField("DeckId").cast<int>()[effect].Set(-1);deck.GetField("DiscardPileId").cast<int>()[effect].Set(-1);}
        if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(array,reinterpret_cast<void**>(&array->m_Items[i]),effect);else array->m_Items[i]=effect;
    }
    add[queue](array);
}
inline bool InitializeSystem() {
    return Engine::EffectRegistration::afterRegister.Add(Register) && Engine::EffectGuards::rules.Add(Guard);
}
}

