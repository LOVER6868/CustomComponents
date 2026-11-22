// Included by StatQueueContext inside its owning namespace.
// Mirrors native ProcessRecords' processed/prediction/pause/cursor contract,
// verified in libil2cpp ProcessRecords, RVA 0x1f70a30. No null record sentinel.
inline void(*originalRecords)(Obj*,Obj*,const void*)=nullptr;
static void Records(Obj* self,Obj* raw,const void* method){
    if(!owner.model || !owner.history || !owner.historyModel || !raw){originalRecords(self,raw,method);return;}
    static auto type=BNM::Class("PvZCards.Game","HistoryProcessor");
    static auto progressField=type.GetField("<Model>k__BackingField").cast<Obj*>();
    static auto historyField=type.GetField("<History>k__BackingField").cast<Obj*>();
    Obj* progress=progressField.IsValid()?progressField[self].Get():nullptr;
    Obj* history=historyField.IsValid()?historyField[self].Get():nullptr;
    static auto recordsField=BNM::Class("PvZCards.Engine","History").GetField("Records").cast<Obj*>();
    if(!history || !recordsField.IsValid() || recordsField[history].Get()!=raw || progress!=owner.historyModel->Get() || history!=owner.history->Get() ||
       reinterpret_cast<std::uintptr_t>(owner.model->Get())!=Core::SceneViewContext::ModelIdentity()){
        originalRecords(self,raw,method);return;
    }
    static auto modelType=BNM::Class("PvZCards.Game","HistoryProcessorModel");
    static auto cursor=modelType.GetField("CurrentPositionInHistory").cast<int>();
    static auto running=modelType.GetField("CanRun").cast<bool>();
    static auto processed=modelType.GetMethod("IsRecordProcessed",1).cast<bool>();
    static auto predicted=type.GetMethod("TryProcessPredictedRecord",1).cast<bool>();
    static auto process=type.GetMethod("ProcessRecord",1).cast<void>();
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(raw);
    if(!Engine::EntityAccess::Valid(list) || !cursor.IsValid() || !running.IsValid() ||
       !processed.IsValid() || !predicted.IsValid() || !process.IsValid()){
        LOG_ERR("CustomHistory: merge dependencies unavailable; preserving native path");originalRecords(self,raw,method);return;
    }
    while(cursor[progress].Get()>=0 && cursor[progress].Get()<list->size){
        PumpCustomRecords(true);
        if(::CustomComponents::History::WaitForPresentation(reinterpret_cast<std::uintptr_t>(owner.model->Get()),
             cursor[progress].Get(),reinterpret_cast<std::uintptr_t>(history)))return;
        Obj* record=Next(self,raw,method);
        if(!record){LOG_ERR("CustomHistory: native GetNextRecord returned null");return;}
        if(!predicted[self](record) && !processed[progress](record)){
            process[self](record);
            if(!running[progress].Get())return;
        }
        cursor[progress].Set(cursor[progress].Get()+1);
    }
    PumpCustomRecords();
}
inline void ResumePresentationHistory(){
    if(processingHistory || !owner.history || !owner.model || reinterpret_cast<std::uintptr_t>(owner.model->Get())!=Core::SceneViewContext::ModelIdentity())return;
    Obj* processor=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","HistoryProcessor"),Engine::FrontendServices::Game());
    static auto canProcess=BNM::Class("PvZCards.Game","HistoryProcessorModel").GetMethod("CanProcess",0).cast<bool>();
    if(!processor || !owner.historyModel || !owner.historyModel->Get() || !canProcess.IsValid() || !canProcess[owner.historyModel->Get()]())return;
    if(::CustomComponents::History::TakePresentationResume(reinterpret_cast<std::uintptr_t>(owner.model->Get()),reinterpret_cast<std::uintptr_t>(owner.history->Get())))
        History(processor,nullptr);
}
