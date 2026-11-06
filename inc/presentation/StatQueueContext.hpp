#pragma once
#include "core/HookRegistry.hpp"
#include "core/ManagedRuntime.hpp"
#include "core/SceneViewContext.hpp"
#include "presentation/CardStatState.hpp"
namespace CustomComponents::Presentation::StatQueue {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Owner {std::shared_ptr<Core::ManagedRuntime::Root> queue,mapper,model,history,historyModel;};
inline Owner owner;
inline thread_local Obj* processingHistory=nullptr;
inline void Capture(Obj* processor,const char* typeName,const char* modelName,const char* mapperName) {
    auto type=BNM::Class("PvZCards.Game",typeName);
    auto queue=type.GetField("<AnimationQueue>k__BackingField").cast<Obj*>();
    auto model=type.GetField(modelName).cast<Obj*>();auto mapper=type.GetField(mapperName).cast<Obj*>();
    if(!processor || !queue.IsValid() || !model.IsValid() || !mapper.IsValid()) return;
    Obj* q=queue[processor].Get();Obj* m=model[processor].Get();Obj* v=mapper[processor].Get();
    if(!q || !m || !v || !processingHistory) return;
    auto processorType=BNM::Class("PvZCards.Game","HistoryProcessor");
    auto historyField=processorType.GetField("<History>k__BackingField").cast<Obj*>();
    auto progressField=processorType.GetField("<Model>k__BackingField").cast<Obj*>();
    if(!historyField.IsValid() || !progressField.IsValid()) return;
    Obj* history=historyField[processingHistory].Get();Obj* progress=progressField[processingHistory].Get();
    if(!history || !progress) return;
    Core::SceneViewContext::Capture(v,m);
    if(owner.queue && owner.queue->Get()==q && owner.mapper->Get()==v && owner.model->Get()==m) return;
    owner={std::make_shared<Core::ManagedRuntime::Root>(q,true),std::make_shared<Core::ManagedRuntime::Root>(v,true),std::make_shared<Core::ManagedRuntime::Root>(m,true),
        std::make_shared<Core::ManagedRuntime::Root>(history,true),std::make_shared<Core::ManagedRuntime::Root>(progress,true)};
}
inline bool Owns(Obj* queue) {
    return owner.queue && owner.mapper && owner.model && owner.queue->Get()==queue &&
        owner.mapper->Get()==Core::SceneViewContext::RootedMapper() &&
        reinterpret_cast<std::uintptr_t>(owner.model->Get())==Core::SceneViewContext::ModelIdentity();
}
inline bool CaughtUp(Obj* queue) {
    if(!Owns(queue) || !owner.history || !owner.historyModel) return false;
    Obj* history=owner.history->Get();Obj* progress=owner.historyModel->Get();
    if(!history || !progress || processingHistory) return false;
    static auto recordsField=BNM::Class("PvZCards.Engine","History").GetField("Records").cast<BNM::Structures::Mono::List<Obj*>*>();
    static auto positionField=BNM::Class("PvZCards.Game","HistoryProcessorModel").GetField("CurrentPositionInHistory").cast<int>();
    auto records=recordsField;auto position=positionField;
    if(!records.IsValid() || !position.IsValid()) return false;
    auto* list=records[history].Get();int next=position[progress].Get();
    return Engine::EntityAccess::Valid(list) && next>=0 && next==list->size;
}
inline void(*originalHistory)(Obj*,const void*)=nullptr;
static void History(Obj* self,const void* method) {
    Obj* previous=processingHistory;processingHistory=self;
    struct Restore {Obj* value;~Restore(){processingHistory=value;}} restore{previous};
    originalHistory(self,method);
}
inline void(*originalMulligan)(Obj*,Obj*,const void*)=nullptr;
inline void(*originalLane)(Obj*,Obj*,const void*)=nullptr;
static void Mulligan(Obj* self,Obj* record,const void* method) {
    Capture(self,"EndMulliganRecordProcessor","<EntityModel>k__BackingField","<EntityViewMapper>k__BackingField");
    originalMulligan(self,record,method);
}
static void Lane(Obj* self,Obj* record,const void* method) {
    Capture(self,"CreateLaneRecordProcessor","<Model>k__BackingField","<ViewMap>k__BackingField");
    originalLane(self,record,method);
}
inline bool Initialize() {
    bool history=Core::Hooks::Install(BNM::Class("PvZCards.Game","HistoryProcessor").GetMethod("ProcessHistory",0),History,originalHistory);
    bool mulligan=Core::Hooks::Install(BNM::Class("PvZCards.Game","EndMulliganRecordProcessor").GetMethod("ProcessRecord",1),Mulligan,originalMulligan);
    bool lane=Core::Hooks::Install(BNM::Class("PvZCards.Game","CreateLaneRecordProcessor").GetMethod("ProcessRecord",1),Lane,originalLane);
    return history && mulligan && lane;
}
}



