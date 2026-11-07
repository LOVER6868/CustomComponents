#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Engine::FrontendServices {
using Obj=Core::ManagedRuntime::Obj;
// UI-thread service access. Observe the live holder's injected binder rather
// than resolving presentation signals from a prediction model's container.
inline std::shared_ptr<Core::ManagedRuntime::Root> binderRoot,modelRoot,holderRoot,projectionRoot;
inline Obj* Game(){return modelRoot?modelRoot->Get():nullptr;}
inline Obj* Holder(){return holderRoot?holderRoot->Get():nullptr;}
inline Obj* Projection(){return projectionRoot?projectionRoot->Get():nullptr;}
inline bool IsPresentationModel(Obj* model){return model && projectionRoot && projectionRoot->Get()==model;}
inline void ObserveFacade(Obj* facade,Obj* game){
    if(!facade || !game || game!=Game())return;
    static auto metadata=BNM::Class("PvZCards.Core.Utilities","GameFacade").GetField("<EntityModel>k__BackingField").cast<Obj*>();auto field=metadata;
    Obj* model=field.IsValid()?field[facade].Get():nullptr;
    if(model && (!projectionRoot || projectionRoot->Get()!=model))projectionRoot=std::make_shared<Core::ManagedRuntime::Root>(model,true);
}
inline void Capture(Obj* holder) {
    static auto binderMetadata=BNM::Class("PvZCards.Game","GameInputStateHolder").GetField("<CommandBinder>k__BackingField").cast<Obj*>();
    static auto modelMetadata=BNM::Class("PvZCards.Game","GameInputStateHolder").GetField("<GameModel>k__BackingField").cast<Obj*>();
    auto binder=binderMetadata;auto model=modelMetadata;
    Obj* current=holder && binder.IsValid()?binder[holder].Get():nullptr;
    Obj* game=holder && model.IsValid()?model[holder].Get():nullptr;
    if(!current){binderRoot.reset();modelRoot.reset();holderRoot.reset();projectionRoot.reset();return;}
    if(!holderRoot || holderRoot->Get()!=holder)holderRoot=std::make_shared<Core::ManagedRuntime::Root>(holder,true);
    if(!binderRoot || binderRoot->Get()!=current)binderRoot=std::make_shared<Core::ManagedRuntime::Root>(current,true);
    if(game && (!modelRoot || modelRoot->Get()!=game)){modelRoot=std::make_shared<Core::ManagedRuntime::Root>(game,true);projectionRoot.reset();}
}
inline Obj* Provide(BNM::Class type,Obj* expectedGame=nullptr) {
    Obj* binder=binderRoot?binderRoot->Get():nullptr;
    if(!binder || !type.IsValid() || (expectedGame && (!modelRoot || modelRoot->Get()!=expectedGame)))return nullptr;
    static auto injectionMetadata=BNM::Class("strange.extensions.command.impl","CommandBinder").GetMethod("get_injectionBinder",0).cast<Obj*>();
    static auto instanceMetadata=BNM::Class("strange.extensions.injector.impl","InjectionBinder").GetMethod("GetInstance",2).cast<Obj*>();
    auto injection=injectionMetadata;auto instance=instanceMetadata;if(!injection.IsValid() || !instance.IsValid())return nullptr;
    Obj* result=nullptr;const auto exception=BNM::TryInvoke([&]{Obj* injector=injection[binder]();if(injector)result=instance[injector](type.GetMonoType(),static_cast<Obj*>(nullptr));});
    return exception.IsValid()?nullptr:result;
}
}
