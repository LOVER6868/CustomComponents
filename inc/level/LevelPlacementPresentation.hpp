#pragma once
#include "level/LevelHandHighlight.hpp"
#include "engine/FrontendServices.hpp"
#include "core/SharedHookEvents.hpp"
namespace CustomComponents::Level::PlacementPresentation {
inline void(*originalExecute)(Obj*,const void*)=nullptr;
inline bool(*modalInput)()=nullptr;
inline Core::ModelRegistry::Token blockedModel=0;
inline int blockedCard=-1;
inline std::shared_ptr<Core::ManagedRuntime::Root> highlightCommand;
inline void Reset(){blockedModel=0;blockedCard=-1;}
inline void Clear(Obj* model,Obj* drag,int card) {
    const auto token=Core::ModelRegistry::Identify(model);
    if(blockedModel==token && blockedCard==card)return;
    static auto targetsMetadata=BNM::Class("PvZCards.Game","DragModel").GetField("Targetids").cast<Obj*>();
    auto targets=targetsMetadata;Obj* list=drag && targets.IsValid()?targets[drag].Get():nullptr;
    if(list){auto clear=BNM::Class(list).GetMethod("Clear",0).cast<void>();if(clear.IsValid())clear[list]();}
    static auto signalType=BNM::Class("PvZCards.Game","DeactivateHighlightSignal");
    static auto dispatchMetadata=signalType.GetMethod("Dispatch",0).cast<void>();auto dispatch=dispatchMetadata;
    Obj* entity=Engine::EntityAccess::Find(model,card);
    static auto gameType=BNM::Class("PvZCards.Core.Utilities","GameModel");
    Obj* game=Engine::Match::Provide(Engine::Match::Instances(entity),gameType.GetMonoType());
    Obj* signal=Engine::FrontendServices::Provide(signalType,game);
    if(!signal)signal=Engine::Match::Provide(Engine::Match::Instances(entity),signalType.GetMonoType());
    if(signal && dispatch.IsValid()){dispatch[signal]();blockedModel=token;blockedCard=card;}
}
inline void Tick(Obj* model,Obj* drag) {
    if(!model || !drag || (modalInput && modalInput())){Reset();return;}
    static auto isDragging=BNM::Class("PvZCards.Game","DragModel").GetMethod("IsDragging",0).cast<bool>();
    if(!isDragging.IsValid() || !Core::Metadata::Bind(isDragging,drag)()) {Reset();return;}
    static auto idMetadata=BNM::Class("PvZCards.Game","DragModel").GetField("CardEntityId").cast<int>();auto id=idMetadata;
    const int card=id.IsValid()?id[drag].Get():-1;
    if(!HandHighlight::Allows(model,card))Clear(model,drag,card);else Reset();
}
static void Execute(Obj* command,const void* method) {
    if(!highlightCommand || highlightCommand->Get()!=command)
        highlightCommand=std::make_shared<Core::ManagedRuntime::Root>(command,true);
    Core::SharedHookEvents::highlightPlayableLocations.Notify(command,false);
    static auto dragMetadata=BNM::Class("PvZCards.Game","HighlightPlayableLocationsOrTargetsForCardCommand").GetField("<DragModel>k__BackingField").cast<Obj*>();
    static auto idMetadata=BNM::Class("PvZCards.Game","DragModel").GetField("CardEntityId").cast<int>();
    auto field=dragMetadata;auto id=idMetadata;Obj* drag=field[command].Get();
    Obj* model=renderModel?renderModel():nullptr;
    const int card=drag && id.IsValid()?id[drag].Get():-1;
    if(!(modalInput && modalInput()) && !HandHighlight::Allows(model,card)){
        Clear(model,drag,card);
        Core::SharedHookEvents::highlightPlayableLocations.Notify(command,true);
        return;
    }
    Reset();originalExecute(command,method);
    Core::SharedHookEvents::highlightPlayableLocations.Notify(command,true);
}
inline void Refresh(int card){
    Reset();
    Obj* command=highlightCommand?highlightCommand->Get():nullptr;
    if(!command || (modalInput && modalInput()))return;
    auto field=BNM::Class("PvZCards.Game","HighlightPlayableLocationsOrTargetsForCardCommand").GetField("<DragModel>k__BackingField").cast<Obj*>();
    Obj* drag=field.IsValid()?field[command].Get():nullptr;
    auto id=BNM::Class("PvZCards.Game","DragModel").GetField("CardEntityId").cast<int>();
    auto dragging=BNM::Class("PvZCards.Game","DragModel").GetMethod("IsDragging",0).cast<bool>();
    if(drag && id.IsValid() && dragging.IsValid() && id[drag].Get()==card && dragging[drag]())Execute(command,nullptr);
}
inline bool Initialize(){Engine::HandPlayability::afterRefresh.Add(Refresh);return Core::Hooks::Install(BNM::Class("PvZCards.Game","HighlightPlayableLocationsOrTargetsForCardCommand").GetMethod("Execute",0),Execute,originalExecute);}
}
