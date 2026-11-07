#pragma once
#include "core/CandidateRules.hpp"
#include "core/Observers.hpp"
#include "engine/RouterContext.hpp"
namespace CustomComponents::Engine::EffectGuards {
using Obj=Core::ManagedRuntime::Obj;
struct Context {Obj* router;Obj* effect;};
inline thread_local Context current{nullptr,nullptr};
inline Core::CandidateRules<Context> rules;
inline Core::Observers<const Context&> before,after;
using Preparation=bool(*)(const Context&);
inline std::mutex preparationLock;
struct PreparationStep {Preparation run;int order;};
inline Core::AtomicSnapshot<std::vector<PreparationStep>> preparations{std::make_shared<const std::vector<PreparationStep>>()};
inline bool RegisterPreparation(Preparation step,int order=0){if(!step)return false;std::lock_guard<std::mutex> guard(preparationLock);auto prior=preparations.load();for(auto item:*prior)if(item.run==step)return item.order==order;auto next=std::make_shared<std::vector<PreparationStep>>(*prior);next->push_back({step,order});std::stable_sort(next->begin(),next->end(),[](auto a,auto b){return a.order<b.order;});preparations.store(next);return true;}
inline bool Prepare(const Context& context){for(auto step:*preparations.load())if(!step.run(context))return false;return true;}
struct ProcessingScope {Context context,previous=current;explicit ProcessingScope(Context value):context(value){current=context;before.Notify(context);}~ProcessingScope(){after.Notify(context);current=previous;}};
inline bool Allows(Obj* router,Obj* effect){return Core::CandidateRules<Context>::Allows(rules.Read(),{router,effect},0);}
}
