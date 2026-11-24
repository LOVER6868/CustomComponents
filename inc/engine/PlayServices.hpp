#pragma once
#include <mutex>
#include <unordered_map>
#include "core/HookRegistry.hpp"
#include "engine/FrontendServices.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/HandModeCommandPolicy.hpp"
#include "engine/PlayServiceCompletion.hpp"
#include "engine/PlayTypeCodes.hpp"
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
// A submission may finish after PlayServiceProcessor::Process returns. Keep
// both the pooled Play and its managed callback target rooted until completion.
struct PendingPlay {
    std::shared_ptr<Core::ManagedRuntime::Root> play;
    std::shared_ptr<Core::ManagedRuntime::Root> completion;
    bool inProcess=true,finished=false;
};
inline std::mutex pendingMutex;
inline std::unordered_map<Completion*,PendingPlay> pendingPlays;
inline void Finished(Completion* completion){
    PendingPlay retired;
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        auto it=pendingPlays.find(completion);
        if(it==pendingPlays.end())return;
        if(it->second.inProcess){it->second.finished=true;return;}
        retired=std::move(it->second);
        pendingPlays.erase(it);
    }
    // 'retired' is destroyed outside the mutex and returns Play to its pool.
}
inline int Type(Obj* play){static auto metadata=BNM::Class("PvZCards.Engine.Dtos","Play").GetField("Type").cast<int>();auto field=metadata;return play?field[play].Get():-1;}
static Obj* Service(Obj* factory,int type,const void* method){return originalService(factory,PlayTypeCodes::Is(type,PlayTypeCodes::Type::Control)?
    PlayTypeCodes::ToNative(PlayTypeCodes::Type::Surprise):type,method);}
static void Local(Obj* self,Obj* play,const void* method){
    Core::Performance::Scope timing(Core::Performance::Area::PlaySubmission,"local-play-card");
    if(PlayTypeCodes::Is(Type(play),PlayTypeCodes::Type::Control)){if(control){control(self,play);auto instances=BNM::Class("PvZCards.Core.Services","LocalPlayCardService").GetField("<Instances>k__BackingField").cast<Obj*>();Obj* model=Match::Provide(instances[self].Get(),BNM::Class("PvZCards.Engine","EntityModel").GetMonoType());SimulationChanges::Changed(model);SimulationChanges::settled.Notify(model);}else {auto fail=BNM::Class("PvZCards.Engine","BaseService").GetMethod("DispatchFailureDelegate",1).cast<void>();fail[self](29);}return;}
    if(observer)observer(self,play,method);else originalLocal(self,play,method);
}
inline bool Submit(Obj* card,int player,int entry){
    auto type=BNM::Class("PvZCards.Engine.Dtos","Play");
    // Borrow a pooled Play; its loan is ended by the completion callback, not
    // by the synchronous function return (which may precede completion).
    auto playRoot=Core::ManagedPool::AcquireScoped(Match::Instances(card),type);
    Obj* play=playRoot?playRoot->Get():nullptr;if(!play)return false;
    type.GetField("Type").cast<int>()[play].Set(PlayTypeCodes::ToNative(PlayTypeCodes::Type::Control));type.GetField("CardId").cast<int>()[play].Set(Match::Id(card));type.GetField("PlayerId").cast<int>()[play].Set(player);type.GetField("OrderInTargetLocation").cast<int>()[play].Set(HandModeOrder);
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
    // Register before Process, since completion may fire synchronously.
    auto callbackRoot=std::make_shared<Core::ManagedRuntime::Root>(completion.Get());
    if(!callbackRoot->Get())return false;
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        if(pendingPlays.size()>=256)return false; // never recycle an in-flight Play
        pendingPlays.emplace(outcome,PendingPlay{playRoot,
            std::move(callbackRoot),true,false});
    }
    auto error=BNM::TryInvoke([&]{setService[factory.Get()](service);setFactory[processor.Get()](factory.Get());process[processor.Get()](play,success.Get(),failure.Get());});
    PendingPlay retired;
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        auto it=pendingPlays.find(outcome);
        if(it!=pendingPlays.end()) {
            it->second.inProcess=false;
            if(it->second.finished || error.IsValid() || outcome->Status()==2) {
                retired=std::move(it->second);
                pendingPlays.erase(it);
            }
        }
    }
    // Releasing 'retired' happens after the processor has returned and after
    // the lock is released. A later callback retires the remaining entries.
    if(error.IsValid())LOG_WARN("CustomComponents/Level/HAND_MODE_FAILURE: managed exception status=%d",outcome->Status());
    return !error.IsValid() && outcome->Status()!=2;
}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","ServiceFactory").GetMethod("GetService",1),Service,originalService) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Services","LocalPlayCardService").GetMethod("Process",1),Local,originalLocal);}
}
