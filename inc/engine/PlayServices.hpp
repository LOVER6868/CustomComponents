#pragma once
#include "core/HookRegistry.hpp"
#include "engine/FrontendServices.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/HandModeCommandPolicy.hpp"
#include "engine/PlayServiceCompletion.hpp"
#include "engine/SimulationChanges.hpp"
namespace CustomComponents::Engine::PlayServices {
using Obj=BNM::IL2CPP::Il2CppObject;
// None has no stock service. A marked control command uses its existing wire
// representation; ordinary card plays retain their native type and lifecycle.
inline constexpr int HandModeOrder=HandMode::Order;
inline bool(*control)(Obj*,Obj*)=nullptr;
inline void(*observer)(Obj*,Obj*,const void*)=nullptr;
inline void(*originalLocal)(Obj*,Obj*,const void*)=nullptr;
inline Obj*(*originalService)(Obj*,int,const void*)=nullptr;
inline int Type(Obj* play){static auto metadata=BNM::Class("PvZCards.Engine.Dtos","Play").GetField("Type").cast<int>();auto field=metadata;return play?field[play].Get():-1;}
static Obj* Service(Obj* factory,int type,const void* method){return originalService(factory,type==0?3:type,method);}
static void Local(Obj* self,Obj* play,const void* method){
    if(Type(play)==0){if(control){control(self,play);auto instances=BNM::Class("PvZCards.Core.Services","LocalPlayCardService").GetField("<Instances>k__BackingField").cast<Obj*>();Obj* model=Match::Provide(instances[self].Get(),BNM::Class("PvZCards.Engine","EntityModel").GetMonoType());SimulationChanges::Changed(model);SimulationChanges::settled.Notify(model);}else {auto fail=BNM::Class("PvZCards.Engine","BaseService").GetMethod("DispatchFailureDelegate",1).cast<void>();fail[self](29);}return;}
    if(observer)observer(self,play,method);else originalLocal(self,play,method);
}
inline bool Submit(Obj* card,int player,int entry){
    auto type=BNM::Class("PvZCards.Engine.Dtos","Play");
    auto root=std::make_shared<Core::ManagedRuntime::Root>(type.CreateNewObjectParameters());Obj* play=root->Get();if(!play)return false;
    type.GetField("Type").cast<int>()[play].Set(0);type.GetField("CardId").cast<int>()[play].Set(Match::Id(card));type.GetField("PlayerId").cast<int>()[play].Set(player);type.GetField("OrderInTargetLocation").cast<int>()[play].Set(HandModeOrder);
    auto targets=type.GetField("Targets").cast<Obj*>()[play].Get();if(!targets)return false;auto add=BNM::Class(targets).GetMethod("Add",1).cast<void>();if(!add.IsValid())return false;add[targets](entry+1);
    Obj* service=FrontendServices::Provide(BNM::Class("PvZCards.Core.Services","PlayCardService"),FrontendServices::Game());if(!service)return false;
    auto factoryType=BNM::Class("PvZCards.Core.Utilities","ServiceFactory");
    auto processorType=BNM::Class("PvZCards.Multiplayer.Networking","PlayServiceProcessor");
    Core::ManagedRuntime::Root factory(factoryType.CreateNewObjectParameters()),processor(processorType.CreateNewObjectParameters());if(!factory.Get() || !processor.Get())return false;
    auto setService=factoryType.GetMethod("set_PlayCardService",1).cast<void>();auto setFactory=processorType.GetMethod("set_ServiceFactory",1).cast<void>();auto process=processorType.GetMethod("Process",3).cast<void>();
    if(!setService.IsValid() || !setFactory.IsValid() || !process.IsValid())return false;
    auto completionType=BNM::Class("CustomComponents.Engine","PlayServiceCompletion");
    Core::ManagedRuntime::Root completion(completionType.CreateNewInstance());if(!completion.Get())return false;
    auto* outcome=reinterpret_cast<Completion*>(completion.Get());outcome->status=0;outcome->card=Match::Id(card);outcome->scene=Core::ModelRegistry::Identify(FrontendServices::Game(),true);
    auto successMethod=completionType.GetMethod("Succeeded",0);auto failureMethod=completionType.GetMethod("Failed",1);
    if(!successMethod.IsValid() || !failureMethod.IsValid())return false;
    auto action=BNM::Class("System","Action");auto failureAction=BNM::Class("System","Action`1").GetGeneric({BNM::Class("PvZCards.Engine","ServiceFailure").GetCompileTimeClass()});
    Core::ManagedRuntime::Root success(action.CreateNewObjectParameters(completion.Get(),successMethod.GetInfo()));
    Core::ManagedRuntime::Root failure(failureAction.CreateNewObjectParameters(completion.Get(),failureMethod.GetInfo()));
    if(!success.Get() || !failure.Get())return false;
    auto error=BNM::TryInvoke([&]{setService[factory.Get()](service);setFactory[processor.Get()](factory.Get());process[processor.Get()](play,success.Get(),failure.Get());});
    if(error.IsValid())LOG_WARN("CustomComponents/Level/HAND_MODE_FAILURE: managed exception status=%d",outcome->status);
    return !error.IsValid() && outcome->status!=2;
}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","ServiceFactory").GetMethod("GetService",1),Service,originalService) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Services","LocalPlayCardService").GetMethod("Process",1),Local,originalLocal);}
}
