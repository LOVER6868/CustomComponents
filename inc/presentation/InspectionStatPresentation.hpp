#include "presentation/HeraldPolicy.hpp"
#include "core/ModelRegistry.hpp"
#include "engine/MatchAccess.hpp"
#pragma once
#include <vector>
#include <BNM/Utils.hpp>
#include "core/HookRegistry.hpp"
#include "core/ManagedRuntime.hpp"
#include "core/SceneViewContext.hpp"
#include "presentation/CardStatState.hpp"
namespace CustomComponents::Presentation::InspectionStats {
using Obj=BNM::IL2CPP::Il2CppObject;
using Root=Core::ManagedRuntime::Root;
struct Snapshot {int guid=-1;Stats::Values stats;Core::ModelRegistry::Token model=0;int entity=-1;bool discarded=false;bool reconcileStats=false;};
struct Entry {std::shared_ptr<Root> object;Snapshot snapshot;};
// Frontend-owned immutable values. Weak keys expire with their item/panel;
// asynchronous display never retains a borrowed gameplay entity.
inline std::vector<Entry> items,panels;
inline thread_local Obj* selectingPanel=nullptr;
inline void Store(std::vector<Entry>& entries,Obj* object,const Snapshot& snapshot) {
    entries.erase(std::remove_if(entries.begin(),entries.end(),[](const auto& entry){return !entry.object->Get();}),entries.end());
    for(auto& entry:entries) if(entry.object->Get()==object) {entry.snapshot=snapshot;return;}
    auto reference=std::make_shared<Root>(object,true);
    if(reference->Get()) entries.push_back({reference,snapshot});
}
inline std::optional<Snapshot> Find(const std::vector<Entry>& entries,Obj* object) {
    for(const auto& entry:entries) if(entry.object->Get()==object) return entry.snapshot;
    return {};
}
inline int Guid(Obj* entity) {
    Obj* card=Engine::EntityAccess::Component(entity,"Card");
    auto field=BNM::Class("PvZCards.Engine.Components","Card").GetField("Guid").cast<int>();
    return card && field.IsValid()?field[card].Get():-1;
}
inline bool Discarded(Obj* entity) {
    Obj* marker=Engine::EntityAccess::Component(entity,"Discarded");
    if(!marker || !Engine::EntityAccess::Component(entity,"DiscardPileElement"))return false;
    auto field=BNM::Class(marker).GetField("Source").cast<int>();int source=field.IsValid()?field[marker].Get():0;
    return HeraldPolicy::CustomDiscard(source,true);
}
inline Snapshot Capture(Obj* entity,Obj* model) {
    return {Guid(entity),CardStats::Read(entity),Core::ModelRegistry::Identify(model,true),Engine::Match::Id(entity),Discarded(entity)};
}
inline void Remember(Obj* item,Obj* entity,Obj* model) {
    if(item && entity) {auto snapshot=Capture(entity,model);snapshot.reconcileStats=true;Store(items,item,snapshot);}
}
struct Data {int cardId;int creationSourceId;};
static_assert(sizeof(Data)==8);
inline void ApplyPanel(Obj* panel) {
    auto snapshot=Find(panels,panel);if(!snapshot || !snapshot->reconcileStats) return;
    auto type=BNM::Class("PvZCards.CardInspection","CardInspectionPanelView");
    auto inspected=type.GetField("InspectedItem").cast<Data>();
    if(!inspected.IsValid() || inspected[panel].Get().cardId!=snapshot->guid) return;
    auto write=[&](const char* name,int value) {
        auto field=type.GetField(name).cast<Obj*>();Obj* widget=field.IsValid()?field[panel].Get():nullptr;
        if(!widget) return;
        auto method=BNM::Class(widget).GetMethod("ShowAndSetText",1).cast<void>();
        if(method.IsValid()) method[widget](BNM::CreateMonoString(std::to_string(value)));
    };
    if(snapshot->stats.attack) write("AttackView",*snapshot->stats.attack);
    if(auto current=snapshot->stats.CurrentHealth()) write("HealthView",*current);
    if(snapshot->stats.cost) {
        auto field=type.GetField("SunCostView").cast<Obj*>();Obj* widget=field.IsValid()?field[panel].Get():nullptr;
        auto method=BNM::Class("PvZCards.CardInspection","CardInspectionSunCostView").GetMethod("ShowAndSetSunCost",1).cast<void>();
        if(widget && method.IsValid()) method[widget](std::max(0,*snapshot->stats.cost));
    }
    auto attached=type.GetMethod("GetAttachedCard",0).cast<Obj*>();
    Obj* view=attached.IsValid()?attached[panel]():nullptr;
    if(view) {
        auto frame=BNM::Class(view).GetField("Frame").cast<Obj*>();
        if(frame.IsValid()) CardStats::Apply(frame[view].Get(),snapshot->stats);
    }
}
inline void(*originalItem)(Obj*,Obj*,const void*)=nullptr;
inline void(*originalData)(Obj*,Data,const void*)=nullptr;
inline void(*originalValues)(Obj*,Data,const void*)=nullptr;
inline void(*originalCreated)(Obj*,int,Obj*,const void*)=nullptr;
inline void(*originalAttach)(Obj*,Obj*,Obj*,int,const void*)=nullptr;
inline void BindItem(Obj* panel,Obj* item) {
    auto snapshot=Find(items,item);
    // Stock inspection items may also carry an exact physical identity (deck
    // inspection). Never find a physical card by GUID alone.
    if(!snapshot && item) {
        auto type=BNM::Class("PvZCards.CardInspection","InspectionItem");
        auto id=type.GetField("<EntityId>k__BackingField").cast<int>();
        auto guid=type.GetField("<CardId>k__BackingField").cast<int>();
        Obj* mapper=Core::SceneViewContext::RootedMapper();
        auto modelField=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();
        Obj* model=mapper && modelField.IsValid()?modelField[mapper].Get():nullptr;
        Obj* entity=id.IsValid()?Engine::EntityAccess::Find(model,id[item].Get()):nullptr;
        if(entity && guid.IsValid() && Guid(entity)==guid[item].Get()) snapshot=Capture(entity,model);
    }
    panels.erase(std::remove_if(panels.begin(),panels.end(),[&](const auto& entry){return !entry.object->Get() || entry.object->Get()==panel;}),panels.end());
    if(snapshot) Store(panels,panel,*snapshot);
}
static void Item(Obj* panel,Obj* item,const void* method) {
    BindItem(panel,item);
    Obj* previous=selectingPanel;selectingPanel=panel;
    struct Restore {Obj* previous;~Restore(){selectingPanel=previous;}} restore{previous};
    originalItem(panel,item,method);ApplyPanel(panel);
}
// Normal overlay inspection calls SetInspectedCard directly. Capture the exact
// item before stock discards its entity identity to construct CardInspectionData.
static void Attach(Obj* mediator,Obj* panel,Obj* item,int index,const void* method) {
    static auto panelType=BNM::Class("PvZCards.CardInspection","CardInspectionPanelView");
    if(!panel || !panelType.IsValid() || !BNM::IsA(panel,panelType)) {
        originalAttach(mediator,panel,item,index,method);return;
    }
    BindItem(panel,item);
    Obj* previous=selectingPanel;selectingPanel=panel;
    struct Restore {Obj* previous;~Restore(){selectingPanel=previous;}} restore{previous};
    originalAttach(mediator,panel,item,index,method);
    ApplyPanel(panel);
}
static void SetData(Obj* panel,Data data,const void* method) {
    // Direct collection/catalog inspection must not inherit a recycled panel's
    // physical snapshot, even when the new card has the same GUID.
    auto snapshot=Find(panels,panel);
    if(selectingPanel!=panel || (snapshot && snapshot->guid!=data.cardId)) panels.erase(std::remove_if(panels.begin(),panels.end(),[&](const auto& entry){return !entry.object->Get() || entry.object->Get()==panel;}),panels.end());
    originalData(panel,data,method);
}
inline Obj* Panel(Obj* mediator) {
    auto field=BNM::Class("PvZCards.CardInspection","CardInspectionPanelViewMediator").GetField("<View>k__BackingField").cast<Obj*>();
    return mediator && field.IsValid()?field[mediator].Get():nullptr;
}
static void Values(Obj* mediator,Data data,const void* method) {originalValues(mediator,data,method);if(auto* panel=Panel(mediator))ApplyPanel(panel);}
static void Created(Obj* mediator,int guid,Obj* card,const void* method) {
    originalCreated(mediator,guid,card,method);
    if(auto* panel=Panel(mediator)) {
        auto snapshot=Find(panels,panel);
        if(snapshot && snapshot->guid==guid) ApplyPanel(panel);
    }
}
inline bool Initialize() {
    auto panel=BNM::Class("PvZCards.CardInspection","CardInspectionPanelView");
    auto mediator=BNM::Class("PvZCards.CardInspection","CardInspectionPanelViewMediator");
    bool a=Core::Hooks::Install(panel.GetMethod("SetInspectedItem",1),Item,originalItem);
    bool b=Core::Hooks::Install(panel.GetMethod("SetInspectedCard",{BNM::Class("PvZCards.CardInspection","CardInspectionData").GetCompileTimeClass()}),SetData,originalData);
    bool c=Core::Hooks::Install(mediator.GetMethod("SetViewValuesForCard",1),Values,originalValues);
    bool d=Core::Hooks::Install(mediator.GetMethod("HandleCardCreated",2),Created,originalCreated);
    bool e=Core::Hooks::Install(BNM::Class("PvZCards.CardInspection","CardInspectionOverlayViewMediator").GetMethod("SetPanelContentsAndAttach",3),Attach,originalAttach);
    return a && b && c && d && e;
}
}


