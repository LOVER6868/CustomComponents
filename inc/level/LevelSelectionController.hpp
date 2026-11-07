#pragma once
#include <chrono>
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
#include "core/SceneViewContext.hpp"
#include "core/FrontendTickDispatcher.hpp"
#include "core/PresentationRefreshPolicy.hpp"
#include "presentation/CardStatState.hpp"
#include <BNM/UnityStructures.hpp>
namespace CustomComponents::Level::Selection {
using Vec3=BNM::Structures::Unity::Vector3;
struct Selection {Core::ModelRegistry::Token model;int card,entry;};
inline std::mutex lock;inline std::vector<Selection> choices;
struct CompletionNotice{Core::ModelRegistry::Token scene;int card;};
inline std::vector<CompletionNotice> completions;
inline void QueueCompletion(Core::ModelRegistry::Token scene,int card){std::lock_guard<std::mutex> guard(lock);completions.push_back({scene,card});}
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
    Obj* entity=Engine::EntityAccess::Find(model,card);if(!entity || !Engine::EntityAccess::Component(entity,"HandElement"))return false;
    Obj* game=Engine::Match::Provide(Engine::Match::Instances(entity),BNM::Class("PvZCards.Core.Utilities","GameModel").GetMonoType());
    auto field=BNM::Class("PvZCards.Core.Utilities","GameModel").GetField("LocalPlayerId").cast<int>();return game && field.IsValid() && field[game].Get()==Engine::BoardPlacement::Owner(entity);
}
inline int EntryFor(Obj* model,int card){return SelectedEntry(HandCard(model,card));}
inline bool ChooseEntry(Obj* model,int card,int entry) {
    if(!Local(model,card) || (inputBlocked && inputBlocked()))return false;Obj* entity=Engine::EntityAccess::Find(model,card);auto definition=DefinitionFor(entity);if(!definition || entry < -1 || entry>=int(definition->entries.size()))return false;
    if(!Engine::PlayServices::Submit(entity,Engine::BoardPlacement::Owner(entity),entry)){LOG_WARN("CustomComponents/Level: hand mode submission unavailable");return false;}
    auto token=Core::ModelRegistry::Identify(model,true);
    const auto scene=Core::ModelRegistry::Identify(Engine::FrontendServices::Game(),true);
    // A synchronous acknowledgement is covered by the refresh below. Later
    // acknowledgements are drained on the frontend tick, never on a worker.
    {std::lock_guard<std::mutex> guard(lock);completions.erase(std::remove_if(completions.begin(),completions.end(),[&](auto item){return item.scene==scene && item.card==card;}),completions.end());bool found=false;for(auto& item:choices)if(item.model==token && item.card==card){item.entry=entry;found=true;break;}if(!found)choices.push_back({token,card,entry});}
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<64)LOG_INFO("CustomComponents/Level/SELECT: card=%d entry=%d",card,entry);
    const bool refreshed=Engine::HandPlayability::Refresh(card);
    static std::atomic<unsigned> refreshSamples{0};if(refreshSamples.fetch_add(1)<32)LOG_INFO("CustomComponents/Level/CACHE_REFRESH: card=%d entry=%d refreshed=%d playable=%d",card,entry,int(refreshed),Engine::HandPlayability::Playable(card).value_or(false));
    if(refreshed)return true;
    static auto signalType=BNM::Class("PvZCards.Game","GameStateUpdatedSignal");
    static auto gameType=BNM::Class("PvZCards.Core.Utilities","GameModel");
    Obj* game=Engine::Match::Provide(Engine::Match::Instances(entity),gameType.GetMonoType());
    Obj* signal=Engine::FrontendServices::Provide(signalType,game);
    if(!signal)signal=Engine::Match::Provide(Engine::Match::Instances(entity),signalType.GetMonoType());
    if(signal){auto dispatch=BNM::Class(signal).GetMethod("Dispatch",0).cast<void>();if(dispatch.IsValid())dispatch[signal]();}
    return true;
}
inline bool NextLevel(Obj* model,int card){auto definition=DefinitionFor(Engine::EntityAccess::Find(model,card));return definition && ChooseEntry(model,card,Next(EntryFor(model,card),int(definition->entries.size())));}
inline bool ResetChoice(Obj* model,int card){return ChooseEntry(model,card,-1);}
inline Obj* View(int card){Obj* mapper=Core::SceneViewContext::RootedMapper();auto get=BNM::Class("PvZCards.Game","EntityViewMapper").GetMethod("GetViewObject",1).cast<Obj*>();return mapper && get.IsValid()?get[mapper](card):nullptr;}
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
inline void Tick() {
    frontend=true;
    static auto frameMethod=BNM::Class("UnityEngine","Time").GetMethod("get_frameCount",0).cast<int>();auto frame=frameMethod;
    if(frame.IsValid() && !frames.Enter(frame[static_cast<Obj*>(nullptr)]()))return;
    Obj* model=Model();double now=Now();CostTint::BeginFrame();
    if(!model){{std::lock_guard<std::mutex> guard(lock);choices.clear();}gesture.Reset();drag.reset();StopPulse();CostTint::EndFrame();return;}
    std::vector<CompletionNotice> acknowledged;{std::lock_guard<std::mutex> guard(lock);acknowledged.swap(completions);}
    const auto scene=Core::ModelRegistry::Identify(Engine::FrontendServices::Game());
    for(auto item:acknowledged)if(item.scene==scene && Local(model,item.card))Engine::HandPlayability::Refresh(item.card);
    std::vector<Selection> snapshot;
    {std::lock_guard<std::mutex> guard(lock);choices.erase(std::remove_if(choices.begin(),choices.end(),[](auto item){Obj* m=Core::ModelRegistry::Get(item.model);Obj* card=Engine::EntityAccess::Find(m,item.card);return !card || !Engine::EntityAccess::Component(card,"HandElement");}),choices.end());snapshot=choices;}
    auto token=Core::ModelRegistry::Identify(model);
    if(drag && dragModelToken!=token){drag.reset();gesture.Reset();StopPulse();}
    for(auto item:snapshot)if(item.model==token && Local(model,item.card))Present(model,item.card,EntryFor(model,item.card));
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
static void Start(Obj* self,const void* method){frontend=true;originalStart(self,method);auto field=BNM::Class("PvZCards.Game","StartCardDragCommand").GetField("<DragModel>k__BackingField").cast<Obj*>();Obj* value=field[self].Get();drag=std::make_shared<Core::ManagedRuntime::Root>(value,true);gesture.Reset();draggedCard=-1;dragModelToken=Core::ModelRegistry::Identify(Model(),true);}
inline bool InitializeSelection(){renderModel=Model;localEntry=EntryFor;isLocalCard=Local;isFrontend=IsFrontend;PlacementPresentation::modalInput=[](){return inputBlocked && inputBlocked();};return Engine::PlayServices::completed.Add(QueueCompletion) && Engine::HandPlayability::Initialize() && HandHighlight::Initialize() && CostTint::Initialize() && FrameState::Initialize() && StatColours::Initialize() && PlacementPresentation::Initialize() && Core::FrontendTickDispatcher::additionalPresentationTicks.Add(Tick) && Core::Hooks::Install(BNM::Class("PvZCards.Game","StartCardDragCommand").GetMethod("Execute",0),Start,originalStart);}
}
