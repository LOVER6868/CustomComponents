#pragma once
#include "level/LevelHandHighlight.hpp"
#include "engine/FrontendServices.hpp"
namespace CustomComponents::Level::PlacementPresentation {
inline void(*originalExecute)(Obj*,const void*)=nullptr;
inline bool(*modalInput)()=nullptr;
inline Core::ModelRegistry::Token blockedModel=0;
inline int blockedCard=-1;
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
    static auto idMetadata=BNM::Class("PvZCards.Game","DragModel").GetField("CardEntityId").cast<int>();auto id=idMetadata;
    const int card=id.IsValid()?id[drag].Get():-1;
    if(!HandHighlight::Allows(model,card))Clear(model,drag,card);else Reset();
}
static void Execute(Obj* command,const void* method) {
    static auto dragMetadata=BNM::Class("PvZCards.Game","HighlightPlayableLocationsOrTargetsForCardCommand").GetField("<DragModel>k__BackingField").cast<Obj*>();
    static auto idMetadata=BNM::Class("PvZCards.Game","DragModel").GetField("CardEntityId").cast<int>();
    auto field=dragMetadata;auto id=idMetadata;Obj* drag=field[command].Get();
    Obj* model=renderModel?renderModel():nullptr;
    const int card=drag && id.IsValid()?id[drag].Get():-1;
    if(!(modalInput && modalInput()) && !HandHighlight::Allows(model,card)){Clear(model,drag,card);return;}
    Reset();originalExecute(command,method);
}
inline bool Initialize(){Engine::HandPlayability::afterRefresh.Add([](int){Reset();});return Core::Hooks::Install(BNM::Class("PvZCards.Game","HighlightPlayableLocationsOrTargetsForCardCommand").GetMethod("Execute",0),Execute,originalExecute);}
}
