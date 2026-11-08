#pragma once
#include <unordered_map>
#include "core/ModelRegistry.hpp"
#include "core/HookRegistry.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/Events.hpp"
namespace CustomComponents::Engine::SimulationChanges {
using Obj=BNM::IL2CPP::Il2CppObject;
inline thread_local unsigned depth=0;
inline std::mutex lock;
inline std::unordered_map<Core::ModelRegistry::Token,std::uint64_t> revisions;
inline std::uint64_t Revision(Obj* model){auto token=Core::ModelRegistry::Identify(model,true);std::lock_guard<std::mutex> guard(lock);return revisions[token];}
inline void Changed(Obj* model){if(!model)return;auto token=Core::ModelRegistry::Identify(model,true);std::lock_guard<std::mutex> guard(lock);++revisions[token];}
inline Core::Observers<Obj*> settled;
inline Obj* ProcessorModel(Obj* processor){
    static auto metadata=BNM::Class("PvZCards.Engine","EffectCollectionProcessor").GetField("<Instances>k__BackingField").cast<Obj*>();auto field=metadata;
    return processor && field.IsValid()?Match::Provide(field[processor].Get(),BNM::Class("PvZCards.Engine","EntityModel").GetMonoType()):nullptr;
}
inline void BeforeComplete(Obj* processor,Obj*){Changed(ProcessorModel(processor));}
inline void Complete(Obj* processor,Obj*){Obj* model=ProcessorModel(processor);Changed(model);settled.Notify(model);}
inline bool Initialize(){Engine::EffectGuards::before.Add([](const auto& c){++depth;Changed(Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(c.router)));});Engine::EffectGuards::after.Add([](const auto& c){Changed(Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(c.router)));if(depth)--depth;});Core::ModelRegistry::retired.Add([](auto token){std::lock_guard<std::mutex> guard(lock);revisions.erase(token);});return Events::effectCompletionReady && Events::beforeEffectComplete.Add(BeforeComplete) && Events::afterEffectComplete.Add(Complete);}
}
