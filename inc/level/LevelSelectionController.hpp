#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include "level/LevelRuntime.hpp"
#include "level/LevelGesturePolicy.hpp"
#include "level/LevelPulseAnimation.hpp"
#include "level/LevelHandHighlight.hpp"
#include "level/LevelCostTint.hpp"
#include "level/LevelFrameState.hpp"
#include "level/LevelStatColours.hpp"
#include "level/LevelPlacementPresentation.hpp"
#include "core/SharedHookEvents.hpp"
#include "core/SceneViewContext.hpp"
#include "core/FrontendTickDispatcher.hpp"
#include "core/PresentationRefreshPolicy.hpp"
#include "presentation/CardStatState.hpp"
#include "engine/SimulationChanges.hpp"
#include <BNM/UnityStructures.hpp>
namespace CustomComponents::Level::Selection {
using Vec3=BNM::Structures::Unity::Vector3;
struct Selection {Core::ModelRegistry::Token model;int card,entry;};
inline std::mutex lock;inline std::vector<Selection> choices;
struct PresentedState {
    Core::ModelRegistry::Token model=0,statsModel=0;
    int card=-1,entry=-1,availableSun=-1,nativePlayable=-1;
    std::uint64_t revision=0;
    std::uintptr_t view=0,frame=0,entity=0,statsEntity=0;
};
inline std::vector<PresentedState> presented;
struct CompletionNotice{Core::ModelRegistry::Token scene;int card;};
inline std::vector<CompletionNotice> completions;
inline void QueueCompletion(Core::ModelRegistry::Token scene,int card){
    std::lock_guard<std::mutex> guard(lock);
    const auto same=[&](const CompletionNotice& notice){return notice.scene==scene && notice.card==card;};
    if(std::none_of(completions.begin(),completions.end(),same))completions.push_back({scene,card});
}
inline thread_local bool frontend=false;
inline bool IsFrontend(){return frontend;}
inline std::shared_ptr<Core::ManagedRuntime::Root> drag;
inline Gesture::Detector gesture;
inline int draggedCard=-1;
inline Core::ModelRegistry::Token dragModelToken=0;
inline Core::PresentationRefresh::FrameGate frames;
inline bool(*inputBlocked)()=nullptr;
inline std::shared_ptr<Core::ManagedRuntime::Root> pulseTransform;
inline double pulseStarted=-1;
inline Vec3 pulseLast{},pulseBase{};
inline float pulseFactor=1;
inline double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline Obj* Model(){Obj* mapper=Core::SceneViewContext::RootedMapper();if(!mapper)return nullptr;auto field=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();return field.IsValid()?field[mapper].Get():nullptr;}
inline bool Local(Obj* model,int card) {
    if(!model || !frontend || Model()!=model)return false;
    Obj* entity=Engine::EntityAccess::Find(model,card);if(!entity || !Engine::EntityAccess::Component(entity,"HandElement") || !ActivePlayerTurn(model,entity))return false;
    Obj* game=Engine::Match::Provide(Engine::Match::Instances(entity),BNM::Class("PvZCards.Core.Utilities","GameModel").GetMonoType());
    auto field=BNM::Class("PvZCards.Core.Utilities","GameModel").GetField("LocalPlayerId").cast<int>();return game && field.IsValid() && field[game].Get()==Engine::BoardPlacement::Owner(entity);
}
inline int EntryFor(Obj* model,int card){return SelectedEntry(HandCard(model,card));}
inline bool ChooseEntry(Obj* model,int card,int entry) {
    if(!Local(model,card) || (inputBlocked && inputBlocked()))return false;Obj* entity=Engine::EntityAccess::Find(model,card);auto definition=DefinitionFor(entity);if(!definition || entry < -1 || entry>=int(definition->entries.size()))return false;
    if(!Engine::PlayServices::Submit(entity,Engine::BoardPlacement::Owner(entity),entry)){LOG_WARN("CustomComponents/Level: hand mode submission unavailable");return false;}
    auto token=Core::ModelRegistry::Identify(model,true);
    const auto scene=Core::ModelRegistry::Identify(Engine::FrontendServices::Game(),true);
    {
        std::lock_guard<std::mutex> guard(lock);
        auto existing=std::find_if(choices.begin(),choices.end(),[&](const Selection& choice){
            return choice.model==token && choice.card==card;
        });
        if(existing==choices.end())choices.push_back({token,card,entry});
        else existing->entry=entry;
    }
    Engine::HandPlayability::dirty=true;
    QueueCompletion(scene,card);
    return true;
}
inline bool NextLevel(Obj* model,int card){auto definition=DefinitionFor(Engine::EntityAccess::Find(model,card));return definition && ChooseEntry(model,card,Next(EntryFor(model,card),int(definition->entries.size())));}
inline bool ResetChoice(Obj* model,int card){return ChooseEntry(model,card,-1);}
inline Obj* View(int card){Obj* mapper=Core::SceneViewContext::RootedMapper();auto get=BNM::Class("PvZCards.Game","EntityViewMapper").GetMethod("GetViewObject",{"entityId"}).cast<Obj*>();return mapper && get.IsValid()?get[mapper](card):nullptr;}
inline Obj* Frame(Obj* view){if(!view)return nullptr;auto get=BNM::Class(view).GetMethod("GetFrame",0).cast<Obj*>();if(get.IsValid())return get[view]();auto field=BNM::Class(view).GetField("Frame").cast<Obj*>();return field.IsValid()?field[view].Get():nullptr;}
inline bool Alive(Obj* object){static auto metadata=BNM::Class("UnityEngine","Object").GetField("m_CachedPtr").cast<void*>();auto field=metadata;return object && field.IsValid() && field[object].Get()!=nullptr;}
inline bool Same(Vec3 a,Vec3 b){return std::abs(a.x-b.x)<.0001f && std::abs(a.y-b.y)<.0001f && std::abs(a.z-b.z)<.0001f;}
inline Vec3 Multiply(Vec3 value,float scale){return {value.x*scale,value.y*scale,value.z*scale};}
inline void StopPulse() {
    Obj* transform=pulseTransform?pulseTransform->Get():nullptr;
    if(Alive(transform)){auto type=BNM::Class("UnityEngine","Transform");auto get=type.GetMethod("get_localScale",0).cast<Vec3>();auto set=type.GetMethod("set_localScale",1).cast<void>();if(get.IsValid() && set.IsValid() && Same(get[transform](),pulseLast))set[transform](pulseBase);}
    pulseTransform.reset();pulseStarted=-1;pulseFactor=1;
}
inline void StartPulse(int card){StopPulse();Obj* view=View(card);if(!view)return;auto get=BNM::Class(view).GetMethod("get_transform",0).cast<Obj*>();Obj* transform=get.IsValid()?get[view]():nullptr;if(!Alive(transform))return;pulseTransform=std::make_shared<Core::ManagedRuntime::Root>(transform,true);auto scale=BNM::Class("UnityEngine","Transform").GetMethod("get_localScale",0).cast<Vec3>();if(!scale.IsValid()){StopPulse();return;}pulseBase=pulseLast=scale[transform]();pulseStarted=Now();}
inline void TickPulse(double now){Obj* transform=pulseTransform?pulseTransform->Get():nullptr;if(!Alive(transform)){StopPulse();return;}if(pulseStarted<0)return;if(now-pulseStarted>=Pulse::Duration){StopPulse();return;}auto type=BNM::Class("UnityEngine","Transform");auto get=type.GetMethod("get_localScale",0).cast<Vec3>();auto set=type.GetMethod("set_localScale",1).cast<void>();if(!get.IsValid() || !set.IsValid()){StopPulse();return;}auto current=get[transform]();pulseBase=Same(current,pulseLast)?Multiply(current,1/pulseFactor):current;pulseFactor=Pulse::Scale(now-pulseStarted);pulseLast=Multiply(pulseBase,pulseFactor);set[transform](pulseLast);}
inline void Present(Obj* model,int card,int entry) {
    Obj* entity=Engine::EntityAccess::Find(model,card);Obj* view=View(card);Obj* frame=Frame(view);if(!frame || !entity)return;
    Obj* selected=HandCard(model,card);Obj* statsModel=selected!=entity?Engine::FrontendServices::Projection():model;
    FrameState::Bind(frame,statsModel,card);
    StatColours::Bind(frame,view,statsModel,card);
    auto values=Presentation::CardStats::Read(selected);
    auto quote=entry>=0?QuoteFor(statsModel,selected,entry):Quote{};
    HandHighlight::Present(model,view,card,entry,quote);
    // Native entity modifiers already contain the selected mode; never add it twice.
    CostTint::Present(frame,entry>=0 && quote.valid);
    Presentation::CardStats::Apply(frame,values);
    static auto dimmerMetadata=BNM::Class("PvZCards.Game","CardViewComponent").GetField("Dimmer").cast<Obj*>();auto dimmerField=dimmerMetadata;Obj* dimmer=dimmerField[view].Get();if(dimmer)StatColours::UpdateCache(dimmer);
}
inline PresentedState StateFor(Obj* model,int card,int entry) {
    Obj* entity=Engine::EntityAccess::Find(model,card);
    Obj* view=View(card);
    Obj* frame=Frame(view);
    Obj* selected=HandCard(model,card);
    Obj* statsModel=selected!=entity?Engine::FrontendServices::Projection():model;
    const auto native=Engine::HandPlayability::Playable(card);
    const int owner=Engine::BoardPlacement::Owner(selected?selected:entity);
    return {
        Core::ModelRegistry::Identify(model),Core::ModelRegistry::Identify(statsModel),
        card,entry,owner>=0?Available(model,owner):-1,native?int(*native):-1,
        Engine::SimulationChanges::Revision(statsModel),
        reinterpret_cast<std::uintptr_t>(view),reinterpret_cast<std::uintptr_t>(frame),
        reinterpret_cast<std::uintptr_t>(entity),reinterpret_cast<std::uintptr_t>(selected)
    };
}
inline bool Same(const PresentedState& a,const PresentedState& b) {
    return a.model==b.model && a.statsModel==b.statsModel && a.card==b.card &&
        a.entry==b.entry && a.availableSun==b.availableSun &&
        a.nativePlayable==b.nativePlayable && a.revision==b.revision &&
        a.view==b.view && a.frame==b.frame && a.entity==b.entity &&
        a.statsEntity==b.statsEntity;
}
inline void RefreshAfterChoices(Obj* model,std::span<const int> cards) {
    if(Engine::HandPlayability::Refresh(cards))return;
    static auto signalType=BNM::Class("PvZCards.Game","GameStateUpdatedSignal");
    Obj* entity=Engine::EntityAccess::Find(model,cards.front());
    Obj* instances=Engine::Match::Instances(entity);
    Obj* signal=Engine::FrontendServices::Provide(signalType,Engine::FrontendServices::Game());
    if(!signal)signal=Engine::Match::Provide(instances,signalType.GetMonoType());
    if(!signal)return;
    auto dispatch=BNM::Class(signal).GetMethod("Dispatch",0).cast<void>();
    if(dispatch.IsValid())dispatch[signal]();
}
inline void Tick() {
    frontend=true;
    static auto frameMethod=BNM::Class("UnityEngine","Time").GetMethod("get_frameCount",0).cast<int>();auto frame=frameMethod;
    if(frame.IsValid() && !frames.Enter(frame[static_cast<Obj*>(nullptr)]()))return;
    Obj* model=Model();double now=Now();CostTint::BeginFrame();
    if(!model){{std::lock_guard<std::mutex> guard(lock);choices.clear();}presented.clear();gesture.Reset();drag.reset();StopPulse();CostTint::EndFrame();return;}
    std::vector<CompletionNotice> acknowledged;{std::lock_guard<std::mutex> guard(lock);acknowledged.swap(completions);}
    const auto scene=Core::ModelRegistry::Identify(Engine::FrontendServices::Game());
    std::vector<int> refreshCards;
    for(auto item:acknowledged)
        if(item.scene==scene && Local(model,item.card))refreshCards.push_back(item.card);
    if(!refreshCards.empty())RefreshAfterChoices(model,refreshCards);
    std::vector<Selection> snapshot;
    {std::lock_guard<std::mutex> guard(lock);choices.erase(std::remove_if(choices.begin(),choices.end(),[](auto item){Obj* m=Core::ModelRegistry::Get(item.model);Obj* card=Engine::EntityAccess::Find(m,item.card);return !card || !Engine::EntityAccess::Component(card,"HandElement");}),choices.end());snapshot=choices;}
    auto token=Core::ModelRegistry::Identify(model);
    // X follows Sun even when no gameplay effect completed this frame.
    // Fixed level selections must NOT change on resource fluctuations.
    bool dynamicSelected=false;
    int xOwner=-1;
    for(auto item:snapshot)if(item.model==token && Local(model,item.card)) {
        Obj* card=HandCard(model,item.card);auto definition=DefinitionFor(card);
        const int entry=EntryFor(model,item.card);
        if(definition && entry>=0 && entry<int(definition->entries.size()) &&
           Dynamic(definition->entries[entry])) {
            dynamicSelected=true;xOwner=Engine::BoardPlacement::Owner(card);break;
        }
    }
    // X must follow Sun, not recompute every frame while resources remain
    // unchanged. The latter scanned the entire hand twice per frame and can
    // compete with stock AI simulation. Runtime::After still reconciles other
    // authoritative game-state changes after their owning effect completes.
    static Core::ModelRegistry::Token lastXToken=0;
    static int lastXOwner=-1,lastXSun=INT32_MIN;
    static Obj* lastXProjection=nullptr;
    if(dynamicSelected && xOwner>=0){
        const int available=Available(model,xOwner);
        Obj* projected=Engine::FrontendServices::Projection();
        if(token!=lastXToken || xOwner!=lastXOwner || available!=lastXSun ||
           projected!=lastXProjection){
            RefreshHandModes(model);
            if(projected && projected!=model)RefreshHandModes(projected);
            lastXToken=token;lastXOwner=xOwner;lastXSun=available;
            lastXProjection=projected;
            static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<16)
                LOG_INFO("CustomComponents/Level/X_REFRESH: model=%p sun=%d change-driven=1",model,available);
        }
    }else{lastXToken=0;lastXOwner=-1;lastXSun=INT32_MIN;lastXProjection=nullptr;}
    if(drag && dragModelToken!=token){drag.reset();gesture.Reset();StopPulse();}
    std::vector<PresentedState> nextPresented;
    nextPresented.reserve(snapshot.size());
    for(auto item:snapshot)if(item.model==token && Local(model,item.card)){
        const int entry=EntryFor(model,item.card);
        const auto next=StateFor(model,item.card,entry);
        const auto prior=std::find_if(presented.begin(),presented.end(),[&](const auto& state){
            return state.model==next.model && state.card==next.card;
        });
        if(prior==presented.end() || !Same(*prior,next))Present(model,item.card,entry);
        else CostTint::Touch(reinterpret_cast<Obj*>(next.frame));
        nextPresented.push_back(next);
    }
    presented.swap(nextPresented);
    CostTint::EndFrame();
    Obj* dragModel=drag?drag->Get():nullptr;PlacementPresentation::Tick(model,dragModel);if(!dragModel){TickPulse(now);return;}
    auto id=BNM::Class("PvZCards.Game","DragModel").GetField("CardEntityId").cast<int>();auto dragging=BNM::Class("PvZCards.Game","DragModel").GetMethod("IsDragging",0).cast<bool>();int card=id[dragModel].Get();
    if(card!=draggedCard){gesture.Reset();draggedCard=card;}
    static auto mouse=BNM::Class("UnityEngine","Input").GetMethod("get_mousePosition",0).cast<Vec3>();static auto width=BNM::Class("UnityEngine","Screen").GetMethod("get_width",0).cast<int>();static auto height=BNM::Class("UnityEngine","Screen").GetMethod("get_height",0).cast<int>();static auto touches=BNM::Class("UnityEngine","Input").GetMethod("get_touchCount",0).cast<int>();
    auto pointer=mouse;auto w=width;auto h=height;auto count=touches;
    if(!Local(model,card) || (inputBlocked && inputBlocked()) || (count.IsValid() && count[static_cast<Obj*>(nullptr)]()>1)){gesture.Reset();StopPulse();return;}
    if(!dragging.IsValid() || !dragging[dragModel]()){gesture.Reset();TickPulse(now);return;}
    if(pointer.IsValid() && w.IsValid() && h.IsValid()){float size=float(std::min(w[static_cast<Obj*>(nullptr)](),h[static_cast<Obj*>(nullptr)]()));auto pos=pointer[static_cast<Obj*>(nullptr)]();if(size>0 && gesture.Sample(pos.x/size,pos.y/size,now) && NextLevel(model,card))StartPulse(card);}
    TickPulse(now);
}
inline void(*originalStart)(Obj*,const void*)=nullptr;
static void Start(Obj* self,const void* method){frontend=true;Core::SharedHookEvents::startCardDrag.Notify(self,false);originalStart(self,method);auto field=BNM::Class("PvZCards.Game","StartCardDragCommand").GetField("<DragModel>k__BackingField").cast<Obj*>();Obj* value=field[self].Get();drag=std::make_shared<Core::ManagedRuntime::Root>(value,true);gesture.Reset();draggedCard=-1;dragModelToken=Core::ModelRegistry::Identify(Model(),true);Core::SharedHookEvents::startCardDrag.Notify(self,true);}
inline bool InitializeSelection(){renderModel=Model;localEntry=EntryFor;isLocalCard=Local;isFrontend=IsFrontend;PlacementPresentation::modalInput=[](){return inputBlocked && inputBlocked();};return Engine::PlayServices::completed.Add(QueueCompletion) && Engine::HandPlayability::Initialize() && HandHighlight::Initialize() && CostTint::Initialize() && FrameState::Initialize() && StatColours::Initialize() && PlacementPresentation::Initialize() && Core::FrontendTickDispatcher::additionalPresentationTicks.Add(Tick) && Core::Hooks::Install(BNM::Class("PvZCards.Game","StartCardDragCommand").GetMethod("Execute",0),Start,originalStart);}
}
