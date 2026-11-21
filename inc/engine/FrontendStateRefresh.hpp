#pragma once
#include <mutex>
#include <set>
#include <map>
#include "core/AnimationQueuePostUpdate.hpp"
#include "core/ModelRegistry.hpp"
#include "core/SceneViewContext.hpp"
#include "engine/FrontendServices.hpp"
#include "engine/SimulationChanges.hpp"
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
namespace CustomComponents::Engine::FrontendStateRefresh {
using Obj=BNM::IL2CPP::Il2CppObject;using Token=Core::ModelRegistry::Token;
inline std::mutex lock;inline std::set<Token> pending;
inline std::map<Token,std::uint64_t> dispatched;
inline Core::Observers<Obj*> afterMoveRecord;
inline BNM::Class moveRecordType;
inline Obj* (*originalCreateCommand)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline Obj* CreateCommand(Obj* factory,Obj* record,Obj* data,const void* method){
    Obj* result=originalCreateCommand(factory,record,data,method);
    if(record && BNM::IsA(record,moveRecordType))afterMoveRecord.Notify(record);
    return result;
}
inline void Queue(Obj* model){if(!model)return;auto token=Core::ModelRegistry::Identify(model,true);std::lock_guard<std::mutex> guard(lock);pending.insert(token);}
inline void Tick(){
    Obj* model=nullptr;Token token=0;
    {std::lock_guard<std::mutex> guard(lock);for(auto it=pending.begin();it!=pending.end();){
        auto* candidate=Core::ModelRegistry::Get(*it);if(!candidate){it=pending.erase(it);continue;}
        if(reinterpret_cast<std::uintptr_t>(candidate)==Core::SceneViewContext::ModelIdentity()){model=candidate;token=*it;pending.erase(it);break;}++it;}}
    if(!model)return;
    auto revision=SimulationChanges::Revision(model);if(dispatched.contains(token) && dispatched[token]==revision)return;
    Obj* signal=FrontendServices::Provide(BNM::Class("PvZCards.Game","GameStateUpdatedSignal"),FrontendServices::Game());
    auto dispatch=signal?BNM::Class(signal).GetMethod("Dispatch",0).cast<void>():BNM::Method<void>{};
    if(!signal || !dispatch.IsValid()){Queue(model);return;}
    auto error=BNM::TryInvoke([&]{dispatch[signal]();});
    if(error.IsValid())Queue(model);else dispatched[token]=revision;
}
inline bool Initialize(){
    static bool initialized=false;if(initialized)return true;
    moveRecordType=BNM::Class("PvZCards.Engine.Records","CardMoveRecord");
    if(!moveRecordType.IsValid() || !Core::Hooks::Install(BNM::Class("PvZCards.Game","AnimationEventActionFactory").GetMethod("CreateCommand",2),CreateCommand,originalCreateCommand))return false;
    Core::ModelRegistry::retired.Add([](auto token){std::lock_guard<std::mutex> guard(lock);pending.erase(token);dispatched.erase(token);});
    initialized=Core::AnimationQueuePostUpdate::Register([](void* queue,const void*){
        {std::lock_guard<std::mutex> guard(lock);if(pending.empty())return;}
        Obj* owner=FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationQueue"),FrontendServices::Game());
        if(owner!=queue)return;
        static auto empty=BNM::Class("PvZCards.Game","EffectAnimationQueue").GetMethod("IsEmpty",0).cast<bool>();
        if(empty.IsValid() && empty[owner]())Tick();
    });return initialized;
}
}
