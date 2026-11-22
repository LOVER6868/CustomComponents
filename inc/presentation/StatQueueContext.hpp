#pragma once
#include "core/HookRegistry.hpp"
#include "core/ManagedRuntime.hpp"
#include "core/SceneViewContext.hpp"
#include "presentation/CardStatState.hpp"
#include "engine/FrontendServices.hpp"
#include "engine/CustomRecords.hpp"
#include "history/PresentationBarrier.hpp"
#include "core/FrontendTickDispatcher.hpp"
#include "diagnostics/HistoryTrace.hpp"
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
    if(owner.queue && owner.queue->Get()==q && owner.mapper->Get()==v && owner.model->Get()==m &&
       owner.history && owner.history->Get()==history && owner.historyModel && owner.historyModel->Get()==progress) return;
    owner={std::make_shared<Core::ManagedRuntime::Root>(q,true),std::make_shared<Core::ManagedRuntime::Root>(v,true),std::make_shared<Core::ManagedRuntime::Root>(m,true),
        std::make_shared<Core::ManagedRuntime::Root>(history,true),std::make_shared<Core::ManagedRuntime::Root>(progress,true)};
}
inline bool Owns(Obj* queue) {
    return owner.queue && owner.mapper && owner.model && owner.queue->Get()==queue &&
        owner.mapper->Get()==Core::SceneViewContext::RootedMapper() &&
        reinterpret_cast<std::uintptr_t>(owner.model->Get())==Core::SceneViewContext::ModelIdentity();
}
inline void ObserveQueue(Obj* queue,bool refreshHistory=false){
    if(!queue || (!refreshHistory && Owns(queue)))return;
    Obj* mapper=Core::SceneViewContext::RootedMapper();
    if(!mapper)return;
    // Nonvirtual record processors may be inlined. Resolve the actual bound
    // frontend services as a second capture boundary, rather than relying on
    // those callbacks to have established ownership on both clients.
    Obj* game=Engine::FrontendServices::Game();
    Obj* bound=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationQueue"),game);
    Obj* processor=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","HistoryProcessor"),game);
    if(bound!=queue || !processor || !mapper)return;
    static auto modelField=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();
    static auto historyField=BNM::Class("PvZCards.Game","HistoryProcessor").GetField("<History>k__BackingField").cast<Obj*>();
    static auto progressField=BNM::Class("PvZCards.Game","HistoryProcessor").GetField("<Model>k__BackingField").cast<Obj*>();
    if(!modelField.IsValid() || !historyField.IsValid() || !progressField.IsValid())return;
    Obj* model=Core::Metadata::Bind(modelField,mapper).Get();
    Obj* history=Core::Metadata::Bind(historyField,processor).Get();
    Obj* progress=Core::Metadata::Bind(progressField,processor).Get();
    if(!model || !history || !progress)return;
    if(Owns(queue) && owner.history && owner.history->Get()==history && owner.historyModel && owner.historyModel->Get()==progress)return;
    owner={std::make_shared<Core::ManagedRuntime::Root>(queue,true),std::make_shared<Core::ManagedRuntime::Root>(mapper,true),std::make_shared<Core::ManagedRuntime::Root>(model,true),std::make_shared<Core::ManagedRuntime::Root>(history,true),std::make_shared<Core::ManagedRuntime::Root>(progress,true)};
    LOG_INFO("CustomComponents/StatQueue/OWNER: queue=%p mapper=%p model=%p history=%p",queue,mapper,model,history);
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
inline void PumpCustomRecords(bool beforeStock=false) {
    // Dispatch native records only when the matching stock History cursor
    // has processed all entries preceding this custom mutation.
    // Dispatch only records belonging to this scene and anchored no later
    // than the stock history cursor. Presentation subscribers own visuals.
    Obj* model=owner.model?owner.model->Get():nullptr;
    Obj* history=owner.history?owner.history->Get():nullptr;
    Obj* progress=owner.historyModel?owner.historyModel->Get():nullptr;
    static auto recordsField=BNM::Class("PvZCards.Engine","History").GetField("Records")
        .cast<BNM::Structures::Mono::List<Obj*>*>();
    static auto positionField=BNM::Class("PvZCards.Game","HistoryProcessorModel")
        .GetField("CurrentPositionInHistory").cast<int>();
    if(model && history && progress && recordsField.IsValid() && positionField.IsValid()) {
        auto* list=recordsField[history].Get();
        static auto canProcess=BNM::Class("PvZCards.Game","HistoryProcessorModel").GetMethod("CanProcess",0).cast<bool>();
        const int cursor=positionField[progress].Get();
        if(list && list->size>=0 && canProcess.IsValid() && canProcess[progress]() &&
            (beforeStock || cursor>=list->size) &&
            reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity()){
            Engine::CustomRecords::BindHistory(reinterpret_cast<std::uintptr_t>(model),reinterpret_cast<std::uintptr_t>(history));
            Engine::CustomRecords::ProcessReady(reinterpret_cast<std::uintptr_t>(model),cursor,list->size,reinterpret_cast<std::uintptr_t>(history));
        }
    }
}
inline void TraceBoundary(Obj* self,const char* phase,Obj* record=nullptr){
    if(!Diagnostics::HistoryTrace::enabled || !self)return;
    static auto type=BNM::Class("PvZCards.Game","HistoryProcessor");
    static auto history=type.GetField("<History>k__BackingField").cast<Obj*>();
    static auto progress=type.GetField("<Model>k__BackingField").cast<Obj*>();
    static auto records=BNM::Class("PvZCards.Engine","History").GetField("Records").cast<BNM::Structures::Mono::List<Obj*>*>();
    static auto cursor=BNM::Class("PvZCards.Game","HistoryProcessorModel").GetField("CurrentPositionInHistory").cast<int>();
    if(!history.IsValid() || !progress.IsValid() || !records.IsValid() || !cursor.IsValid())return;
    Obj* h=history[self].Get();Obj* p=progress[self].Get();if(!h || !p)return;
    auto* list=records[h].Get();
    if(Diagnostics::HistoryTrace::Allow())LOG_INFO("CustomHistory/%s: us=%lld processor=%p history=%p progress=%p cursor=%d count=%d stockRecord=%p type=%s",
        phase,Diagnostics::HistoryTrace::Micros(),self,h,p,cursor[p].Get(),list?list->size:-1,record,
        record && record->klass?record->klass->name:"none");
}
inline Obj* (*originalNext)(Obj*,Obj*,const void*)=nullptr;
static Obj* Next(Obj* self,Obj* records,const void* method){
    Obj* record=originalNext(self,records,method);
    TraceBoundary(self,"BEFORE_STOCK",record);
    // Only the owning live History may merge custom work. Prediction entries
    // still pass through this boundary even when their stock visual is skipped.
    static auto field=BNM::Class("PvZCards.Game","HistoryProcessor").GetField("<History>k__BackingField").cast<Obj*>();
    static auto progress=BNM::Class("PvZCards.Game","HistoryProcessor").GetField("<Model>k__BackingField").cast<Obj*>();
    if(field.IsValid() && progress.IsValid() && owner.history && owner.historyModel &&
       owner.history->Get()==field[self].Get() && owner.historyModel->Get()==progress[self].Get())PumpCustomRecords(true);
    return record;
}
inline void(*originalHistory)(Obj*,const void*)=nullptr;
static void History(Obj* self,const void* method) {
    Obj* previous=processingHistory;processingHistory=self;
    struct Restore {Obj* value;~Restore(){processingHistory=value;}} restore{previous};
    TraceBoundary(self,"BATCH_BEGIN");
    {
        Obj* q=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationQueue"),Engine::FrontendServices::Game());
        ObserveQueue(q,true);
    }
    originalHistory(self,method);
    TraceBoundary(self,"BATCH_END");
    PumpCustomRecords();
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
#include "presentation/history/StockHistoryMerge.inl"
inline bool Initialize() {
    if(!Core::Hooks::Install(BNM::Class("PvZCards.Game","HistoryProcessor").GetMethod("ProcessRecords",1),Records,originalRecords) || !Core::FrontendTickDispatcher::additionalPresentationTicks.Add(ResumePresentationHistory))return false;
    Engine::CustomRecords::trace=Diagnostics::HistoryTrace::Record;
    if(!Core::Hooks::Install(BNM::Class("PvZCards.Game","HistoryProcessor").GetMethod("GetNextRecord",1),Next,originalNext)){
        LOG_ERR("CustomHistory: in-band boundary unavailable; initialization refused");return false;
    }
    Core::ModelRegistry::retired.Add([](auto token){::CustomComponents::History::RetirePresentationModel(token);::CustomComponents::History::RetireModel(token);});
    bool history=Core::Hooks::Install(BNM::Class("PvZCards.Game","HistoryProcessor").GetMethod("ProcessHistory",0),History,originalHistory);
    bool mulligan=Core::Hooks::Install(BNM::Class("PvZCards.Game","EndMulliganRecordProcessor").GetMethod("ProcessRecord",1),Mulligan,originalMulligan);
    bool lane=Core::Hooks::Install(BNM::Class("PvZCards.Game","CreateLaneRecordProcessor").GetMethod("ProcessRecord",1),Lane,originalLane);
    return history && mulligan && lane;
}
}



