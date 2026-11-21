#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Exceptions.hpp>
#include <map>
#include "history/CustomHistory.hpp"
#include "core/NativeConstruction.hpp"
#include "core/ManagedRuntime.hpp"
#include "core/SceneViewContext.hpp"
#include "core/FrontendTickDispatcher.hpp"
#include "engine/FrontendServices.hpp"
namespace CustomComponents::Presentation::HistoryAnimations {
using Obj=BNM::IL2CPP::Il2CppObject;
enum class Progress {Running,Complete,Failed};
using Step=Progress(*)(const History::Record&,bool);
using Cancel=void(*)(const History::Record&);
struct Driver {History::Kind kind;Step step;Cancel cancel;};
struct Job {History::Record record;std::shared_ptr<Core::ManagedRuntime::Root> animation;Driver driver;bool done=false;};
inline std::map<std::uint64_t,Job> jobs; // frontend thread only
inline std::vector<Driver> drivers;
inline void Advance(std::uint64_t id,bool start){
    auto it=jobs.find(id);if(it==jobs.end() || it->second.done)return;
    auto& job=it->second;
    if(job.record.model!=Core::SceneViewContext::ModelIdentity() || History::IsTerminal(id)){
        if(job.driver.cancel)BNM::TryInvoke([&]{job.driver.cancel(job.record);});
        History::Cancel(id,History::Reason::SceneEnded);job.done=true;return;
    }
    if(start)History::MarkStarted(id);
    Progress progress=Progress::Failed;
    auto error=BNM::TryInvoke([&]{progress=job.driver.step(job.record,start);});
    if(error.IsValid() || progress==Progress::Failed){
        if(job.driver.cancel)BNM::TryInvoke([&]{job.driver.cancel(job.record);});
        History::Fail(id,History::Reason::MissingView);job.done=true;
    }else if(progress==Progress::Complete){History::MarkVisualComplete(id);job.done=true;}
}
inline void CancelJob(std::uint64_t id){auto it=jobs.find(id);if(it==jobs.end() || it->second.done)return;
    if(it->second.driver.cancel)BNM::TryInvoke([&]{it->second.driver.cancel(it->second.record);});
    History::Cancel(id,History::Reason::QueueCancelled);it->second.done=true;
}
struct Animation:Obj {
    std::uint8_t inherited[0x18]{}; // verified NullEffectAnimation native size 0x28
    std::uint64_t RecordId=0;
    BNM_CustomClass(Animation,BNM::CompileTimeClassBuilder("PvZCards.Game","CustomHistoryAnimation","Assembly-CSharp.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Game","NullEffectAnimation","Assembly-CSharp.dll").Build(),{});
    BNM_CustomField(RecordId,BNM::Defaults::Get<std::uint64_t>(),"RecordId");
    void Constructor(){Core::NativeConstruction::Base(this);RecordId=0;}
    void OnStarted(){Advance(RecordId,true);}
    void Update(){Advance(RecordId,false);}
    bool IsCompleted(){auto it=jobs.find(RecordId);return it==jobs.end() || it->second.done;}
    bool CanYield(){return IsCompleted();}
    void OnCanceled(){CancelJob(RecordId);}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(OnStarted,false,BNM::Defaults::Get<void>(),"OnStarted");
    BNM_CustomMethod(Update,false,BNM::Defaults::Get<void>(),"Update");
    BNM_CustomMethod(IsCompleted,false,BNM::Defaults::Get<bool>(),"IsCompleted");
    BNM_CustomMethod(CanYield,false,BNM::Defaults::Get<bool>(),"CanYield");
    BNM_CustomMethod(OnCanceled,false,BNM::Defaults::Get<void>(),"OnCanceled");
};
inline bool Enqueue(const History::Record& record){
    if(jobs.contains(record.serial))return true;
    auto driver=std::find_if(drivers.begin(),drivers.end(),[&](auto d){return d.kind==record.kind;});
    if(driver==drivers.end())return false;
    Obj* queue=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationQueue"),Engine::FrontendServices::Game());
    static auto add=BNM::Class("PvZCards.Game","EffectAnimationQueue").GetMethod("AddAnimation",1).cast<void>();
    auto type=BNM::Class("PvZCards.Game","CustomHistoryAnimation");
    if(!queue || !type.IsValid() || !add.IsValid())return false;
    Obj* raw=type.CreateNewObjectParameters();auto root=std::make_shared<Core::ManagedRuntime::Root>(raw);
    if(!root->Get())return false;
    reinterpret_cast<Animation*>(raw)->RecordId=record.serial;
    jobs.emplace(record.serial,Job{record,std::move(root),*driver,false});
    const auto error=BNM::TryInvoke([&]{add[queue](raw);});
    if(error.IsValid()){jobs.erase(record.serial);History::Fail(record.serial,History::Reason::EnqueueFailed);return true;}
    return true;
}
inline bool Register(History::Kind kind,Step step,Cancel cancel){
    if(!step)return false;
    for(auto d:drivers)if(d.kind==kind)return d.step==step && d.cancel==cancel;
    drivers.push_back({kind,step,cancel});return History::Register(kind,Enqueue);
}
inline void Sweep(){
    const auto scene=Core::SceneViewContext::ModelIdentity();
    History::ActivateScene(scene,reinterpret_cast<std::uintptr_t>(Core::SceneViewContext::RootedMapper()));
    for(auto& [id,job]:jobs)if(!job.done && (job.record.model!=scene || History::IsTerminal(id)))CancelJob(id);
    std::erase_if(jobs,[](auto& item){return item.second.done;});
    History::PumpObservers();
}
inline bool Initialize(){
    static bool initialized=false;if(initialized)return true;
    auto type=BNM::Class("PvZCards.Game","CustomHistoryAnimation");
    if(!type.IsValid()){LOG_ERR("CustomHistory: queue animation class unavailable");return false;}
    initialized=Core::FrontendTickDispatcher::additionalPresentationTicks.Add(Sweep);return initialized;
}
}
