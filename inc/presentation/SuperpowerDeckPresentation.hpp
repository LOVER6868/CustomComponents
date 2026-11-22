#pragma once
#include <bloom/bloom.h>
#include "core/AnimationQueuePostUpdate.hpp"
#include "core/HookRegistry.hpp"
#include <atomic>
#include <unordered_set>
#include <cctype>
#include <map>
#include "core/WeakObjectSettings.hpp"
#include "core/SceneViewContext.hpp"
#include "engine/FrontendServices.hpp"
#include "zones/CreateSuperpowerSubset.hpp"
namespace CustomComponents::Presentation::SuperpowerDeck {
using Obj=Engine::Match::Obj;
using Root=Core::ManagedRuntime::Root;
struct Color {float r,g,b,a;};
struct Vec3 {float x,y,z;};
struct Tint {std::shared_ptr<Root> renderer;Color original;};
struct State {int count=-1;std::shared_ptr<Root> label,gold;std::vector<Tint> tints;bool inspected=false;};
inline Core::WeakObjectSettings<State> states;
inline std::map<std::pair<std::uintptr_t,int>,int> restoration;
inline void PoolCreated(const History::Event& event){
    const auto& record=event.record;
    auto* pool=std::get_if<History::PoolCreatedPayload>(&record.payload);
    if(record.kind==History::Kind::CardCreated && pool && pool->superpowers && pool->before==0 && pool->after>0 &&
       record.model==Core::SceneViewContext::ModelIdentity())restoration[{record.model,pool->owner}]=pool->after;
}
inline BNM::Field<Obj*> shield,meter,animator,textUI,mapperModel;
inline BNM::Field<int> remaining;
inline BNM::Method<Obj*> getView,getObject,getTransform,getObjectTransform,instantiate;
inline BNM::Method<void> active,setInteger,setText,setPosition,setName;
inline BNM::Method<BNM::Structures::Mono::String*> getName;
inline BNM::Method<Color> getColor;
inline BNM::Method<void> setColor;
inline BNM::Method<BNM::Structures::Mono::Array<Obj*>*> renderers;
inline BNM::Class spriteType,textType,viewMapperType;
inline int localView=-1,opponentView=-1,shieldParameter=0;
inline BNM::Field<Obj*> shieldParameterField;
inline BNM::Method<int> parameterId;
inline bool parameterReady=false;
// Only the native MulliganPhaseEndRecordProcessor creates the opening
// superpower draw groups. An ordinary shield-break draw uses the independent
// RegularDrawSuperpowerAnimation and MUST NOT settle an opening pool of four.
// Track the actual returned draw-group animation identities, their starts,
// and their native IsCompleted results. No arbitrary frame/time threshold.
inline std::uintptr_t drawScene=0;
inline bool mulliganEnded=false,openingPostProcessed=false;
inline std::unordered_set<Obj*> openingGroups,startedGroups,completedGroups;
using RecordFn=void(*)(void*,Obj*,const void*);
using StartedFn=void(*)(void*,const void*);
using CompletedFn=bool(*)(void*,const void*);
using OpeningGroupFn=Obj*(*)(void*,Obj*,const void*);
inline RecordFn originalMulliganEnd=nullptr,originalOpeningPost=nullptr;
inline StartedFn originalOpeningStarted=nullptr;
inline CompletedFn originalOpeningCompleted=nullptr;
inline OpeningGroupFn originalAllyOpening=nullptr,originalOpponentOpening=nullptr;
inline void ResetOpening(){
    mulliganEnded=false;openingPostProcessed=false;
    openingGroups.clear();startedGroups.clear();completedGroups.clear();
}
inline void SyncDrawScene(){
    const auto scene=Core::SceneViewContext::ModelIdentity();
    // The first mulligan processor can run before the frontend has a scene
    // mapper; don't erase its native lifecycle evidence when mapper appears.
    if(scene && drawScene && drawScene!=scene){ResetOpening();drawScene=scene;}
    else if(scene && !drawScene)drawScene=scene;
}
inline void MulliganEnd(void* self,Obj* record,const void* method){
    SyncDrawScene();
    // One per match; these are the next opening's animation identities.
    ResetOpening();
    originalMulliganEnd(self,record,method);
    mulliganEnded=true;
    LOG_INFO("CustomComponents/Superpowers/MULLIGAN_END: record=%p scene=%p openingGroups=%zu",record,
        reinterpret_cast<void*>(drawScene),openingGroups.size());
}
inline Obj* OpeningGroup(void* self,Obj* records,const void* method,OpeningGroupFn original,const char* side){
    Obj* group=original(self,records,method);
    SyncDrawScene();
    if(group)openingGroups.insert(group);
    LOG_INFO("CustomComponents/Superpowers/OPENING_GROUP_CREATED: side=%s group=%p records=%p groups=%zu",side,group,records,openingGroups.size());
    return group;
}
inline Obj* AllyOpening(void* self,Obj* records,const void* method){
    return OpeningGroup(self,records,method,originalAllyOpening,"ally");
}
inline Obj* OpponentOpening(void* self,Obj* records,const void* method){
    return OpeningGroup(self,records,method,originalOpponentOpening,"opponent");
}
inline void OpeningPost(void* self,Obj* data,const void* method){
    originalOpeningPost(self,data,method);
    SyncDrawScene();openingPostProcessed=true;
    LOG_INFO("CustomComponents/Superpowers/OPENING_POST_PROCESS: source=%p groups=%zu complete=%zu",self,openingGroups.size(),completedGroups.size());
}
inline void OpeningStarted(void* self,const void* method){
    SyncDrawScene();
    if(openingGroups.count(static_cast<Obj*>(self)))startedGroups.insert(static_cast<Obj*>(self));
    originalOpeningStarted(self,method);
    if(openingGroups.count(static_cast<Obj*>(self)))
        LOG_INFO("CustomComponents/Superpowers/OPENING_DRAW_START: group=%p started=%zu total=%zu",self,startedGroups.size(),openingGroups.size());
}
inline bool OpeningCompleted(void* self,const void* method){
    const bool done=originalOpeningCompleted(self,method);
    SyncDrawScene();
    if(done && openingGroups.count(static_cast<Obj*>(self)) && startedGroups.count(static_cast<Obj*>(self)) &&
       completedGroups.insert(static_cast<Obj*>(self)).second)
        LOG_INFO("CustomComponents/Superpowers/OPENING_DRAW_COMPLETE: group=%p complete=%zu total=%zu",self,completedGroups.size(),openingGroups.size());
    return done;
}
inline bool OpeningSettled(){
    return mulliganEnded && openingPostProcessed && !openingGroups.empty() &&
        completedGroups.size()==openingGroups.size();
}
inline std::string Name(Obj* object){
    if(!object || !getName.IsValid())return {};
    auto* value=Core::Metadata::Bind(getName,object)();return value?value->str():std::string{};
}
inline bool IsSmallShieldName(std::string value){
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
    // Only explicit power-indicator sprites can be recolored or hidden.
    // Ambiguous health artwork, backgrounds and the block meter fail closed.
    if(value.find("health")!=std::string::npos || value.find("meter")!=std::string::npos ||
       value.find("background")!=std::string::npos || value.find("shadow")!=std::string::npos ||
       value.find("count")!=std::string::npos)return false;
    return value.find("smallshield")!=std::string::npos ||
        value.find("small_shield")!=std::string::npos ||
        value.find("shieldpip")!=std::string::npos ||
        value.find("shield_pip")!=std::string::npos ||
        value.find("superpowerindicator")!=std::string::npos;
}
inline bool UnityAlive(Obj* object){
    static auto alive=BNM::Class("UnityEngine","Object").GetMethod("op_Implicit",1).cast<bool>();
    return object && alive.IsValid() && alive(object);
}
inline void Theme(const State& state,bool golden){
    const bool owned=state.gold && UnityAlive(state.gold->Get());
    for(const auto& tint:state.tints)if(Obj* renderer=tint.renderer->Get())if(UnityAlive(renderer)){
        auto color=tint.original;if(golden && owned)color.a=0.f;
        Core::Metadata::Bind(setColor,renderer)(color);
    }
    if(golden && owned)Core::Metadata::Bind(setColor,state.gold->Get())(Color{1.f,.76f,.12f,1.f});
}
inline void Refresh(Obj* life,int count,int owner){
    if(!life)return;
    auto state=states.Get(life);
    SyncDrawScene();
    const bool settled=OpeningSettled();
    const int nativeRemaining=Core::Metadata::Bind(remaining,life).Get();
    const auto key=std::pair{Core::SceneViewContext::ModelIdentity(),owner};
    const auto request=restoration.find(key);
    const int confirmedGain=request==restoration.end()?0:request->second;
    const bool gained=confirmedGain>0 || (state.count>=0 && count>state.count);
    const bool restoreFromEmpty=confirmedGain>0 || (state.count==0 && count>0);
    const bool golden=count>=4 && settled && (nativeRemaining>0 || restoreFromEmpty);
    if(nativeRemaining<=0 && !restoreFromEmpty){
        Theme(state,false);
        for(auto root:{state.gold,state.label})if(root && UnityAlive(root->Get())){
            Obj* object=Core::Metadata::Bind(getObject,root->Get())();if(object)Core::Metadata::Bind(active,object)(false);
        }
        state.count=count;states.Set(life,std::move(state));return;
    }
    if(count>=4 && !settled){
        static std::atomic<unsigned> pendingReports{0};
        if(pendingReports.fetch_add(1)<40)
            LOG_INFO("CustomComponents/Superpowers/INITIAL_POOL_GUARD: owner=%d count=%d mulliganEnd=%d postProcess=%d groups=%zu complete=%zu",owner,count,int(mulliganEnded),int(openingPostProcessed),openingGroups.size(),completedGroups.size());
        // Do not overwrite stock visual state while the opening draw is active.
        return;
    }
    if(state.count==count && !gained &&
       (!golden || (state.gold && UnityAlive(state.gold->Get())))){
        // Animator color tracks may continue to run on the native shield art.
        if(golden)Theme(state,true);
        return;
    }
    Obj* object=Core::Metadata::Bind(shield,life).Get();
    Obj* blockMeter=Core::Metadata::Bind(meter,life).Get();
    Obj* animation=Core::Metadata::Bind(animator,life).Get();
    if(!UnityAlive(object) || !UnityAlive(animation))return;
    // In 4+ mode we must prove the named small-shield binding BEFORE touching
    // the animator or any native sprite. Never clone the first random sprite.
    if(golden && !state.inspected){
        state.inspected=true;
        auto* found=Core::Metadata::Bind(renderers,object)(spriteType.GetMonoType(),true);
        static auto meterSprite=BNM::Class("PvZCards.Game","BlockMeterViewComponent").GetField("Renderer").cast<Obj*>();
        Obj* meterRenderer=blockMeter && meterSprite.IsValid()?Core::Metadata::Bind(meterSprite,blockMeter).Get():nullptr;
        unsigned emitted=0;
        for(std::size_t i=0;found && i<found->capacity;++i){
            Obj* renderer=found->m_Items[i];if(!UnityAlive(renderer) || renderer==meterRenderer)continue;
            Obj* go=Core::Metadata::Bind(getObject,renderer)();
            const std::string rendererName=Name(renderer), objectName=Name(go);
            const bool selected=IsSmallShieldName(rendererName) || IsSmallShieldName(objectName);
            if(emitted++<24)LOG_INFO("CustomComponents/Superpowers/PREFAB_BIND: owner=%d life=%p renderer=%p sprite=%s object=%s smallShield=%d",
                owner,life,renderer,rendererName.c_str(),objectName.c_str(),int(selected));
            if(selected)state.tints.push_back({std::make_shared<Root>(renderer),Core::Metadata::Bind(getColor,renderer)()});
        }
        if(state.tints.empty())LOG_WARN("CustomComponents/Superpowers/NO_SMALL_SHIELD_BINDING: owner=%d life=%p; preserving stock health/animator art",owner,life);
    }
    if(golden && state.tints.empty()){
        states.Set(life,std::move(state));return;
    }
    if(restoreFromEmpty)Core::Metadata::Bind(active,object)(true);
    if(restoreFromEmpty && UnityAlive(blockMeter)){
        Obj* meterObject=Core::Metadata::Bind(getObject,blockMeter)();
        if(meterObject)Core::Metadata::Bind(active,meterObject)(count>0);
    }
    // Native blocking owns depletion. Only an actual observed pool gain may
    // increase the remaining shields; golden art does not alter that counter.
    if(gained){
        const int updated=std::clamp(nativeRemaining+(confirmedGain>0?confirmedGain:count-state.count),0,3);
        Core::Metadata::Bind(remaining,life).Set(updated);
        Core::Metadata::Bind(setInteger,animation)(shieldParameter,updated);
        restoration.erase(key);
    }
    if(golden && (!state.gold || !UnityAlive(state.gold->Get())) && !state.tints.empty()){
        Obj* original=state.tints.front().renderer->Get();
        Obj* parent=Core::Metadata::Bind(getObjectTransform,object)();
        Obj* gold=UnityAlive(original) && parent?instantiate(original,parent,false):nullptr;
        if(gold && BNM::IsA(gold,spriteType)){
            state.gold=std::make_shared<Root>(gold);
            Core::Metadata::Bind(setName,gold)(BNM::CreateMonoString("SprigSuperpowerShield"));
            Obj* transform=Core::Metadata::Bind(getTransform,gold)();
            if(transform)Core::Metadata::Bind(setPosition,transform)(Vec3{0.f,0.f,-.05f});
        }
    }
    if(state.gold && UnityAlive(state.gold->Get())){
        Obj* goldObject=Core::Metadata::Bind(getObject,state.gold->Get())();
        if(goldObject)Core::Metadata::Bind(active,goldObject)(golden);
    }
    Theme(state,golden);
    if(golden && (!state.label || !UnityAlive(state.label->Get()))){
        Obj* original=Core::Metadata::Bind(textUI,life).Get();
        Obj* parent=Core::Metadata::Bind(getObjectTransform,object)();
        // Clone the native count label to retain the game's font/material.
        Obj* label=original && parent?instantiate(original,parent,false):nullptr;
        if(label && BNM::IsA(label,textType)){
            state.label=std::make_shared<Root>(label);
            Obj* transform=Core::Metadata::Bind(getTransform,label)();
            if(transform)Core::Metadata::Bind(setPosition,transform)(Vec3{0.f,0.f,-.1f});
        }
    }
    if(state.label && UnityAlive(state.label->Get())){
        Obj* label=state.label->Get();Obj* labelObject=Core::Metadata::Bind(getObject,label)();
        if(labelObject)Core::Metadata::Bind(active,labelObject)(golden);
        if(golden)Core::Metadata::Bind(setText,label)(BNM::CreateMonoString(std::to_string(count)));
    }
    state.count=count;states.Set(life,std::move(state));
    LOG_INFO("CustomComponents/Superpowers/SHIELDS: remaining=%d golden=%d",count,int(golden));
}
inline void Tick(void*,const void*){
    // AnimatorParameters is initialized by frontend startup, not module init.
    if(!parameterReady){
        Obj* value=shieldParameterField.Get();if(!value)return;
        shieldParameter=Core::Metadata::Bind(parameterId,value)();parameterReady=true;
    }
    Obj* mapper=Core::SceneViewContext::RootedMapper();
    Obj* model=mapper?Core::Metadata::Bind(mapperModel,mapper).Get():nullptr;
    Obj* game=Engine::FrontendServices::Game();
    static auto localId=BNM::Class("PvZCards.Core.Utilities","GameModel").GetField("LocalPlayerId").cast<int>();
    if(!model || !game || !localId.IsValid())return;
    Obj* views=Engine::FrontendServices::Provide(viewMapperType);if(!views)return;
    const int local=Core::Metadata::Bind(localId,game).Get();
    static Core::ModelRegistry::Token scene=0;
    static std::vector<std::pair<int,int>> pools;
    const auto token=Core::ModelRegistry::Identify(model,true);
    if(scene!=token){scene=token;pools.clear();}
    if(pools.size()<2){
        const auto api=Engine::EntityAccess::Api();const auto* entities=Core::Metadata::Bind(api.entities,model).Get();
        if(!Engine::EntityAccess::Valid(entities))return;
        pools.clear();
        for(int i=0;i<entities->size;++i){Obj* player=entities->items->m_Items[i];
            if(!Engine::EntityAccess::Component(player,"Player"))continue;
            const int owner=Engine::Match::Id(player);
            if(Obj* pool=Engine::CardPoolAccess::Get(model,owner,true))pools.emplace_back(Engine::Match::Id(pool),owner);
        }
    }
    for(const auto [id,owner]:pools){Obj* pool=Engine::EntityAccess::Find(model,id);if(!pool)continue;
        Obj* ordered=Engine::EntityAccess::Component(pool,"OrderedCardPool");
        static auto entries=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<Engine::EntityAccess::List*>();
        const auto* list=ordered && entries.IsValid()?Core::Metadata::Bind(entries,ordered).Get():nullptr;if(!Engine::EntityAccess::Valid(list))continue;
        Obj* view=Core::Metadata::Bind(getView,views)(owner==local?localView:opponentView);
        if(view && BNM::IsA(view,BNM::Class("PvZCards.Game","LifeViewComponent")))Refresh(view,list->size,owner);
    }
}
inline void SafeTick(void* queue,const void* method){
    const auto error=BNM::TryInvoke([&]{Tick(queue,method);});
    if(error.IsValid()){static unsigned failures=0;if(failures++<8)LOG_WARN("CustomComponents/Superpowers: frontend not ready for shield refresh");}
}
inline bool Initialize(){
    const auto life=BNM::Class("PvZCards.Game","LifeViewComponent");
    shield=life.GetField("ShieldObject").cast<Obj*>();meter=life.GetField("BlockMeter").cast<Obj*>();animator=life.GetField("Animator").cast<Obj*>();textUI=life.GetField("CountUI").cast<Obj*>();remaining=life.GetField("ShieldsRemaining").cast<int>();
    viewMapperType=BNM::Class("PvZCards.Game","NonEntityViewMapper");getView=viewMapperType.GetMethod("GetViewObject",{"viewId"}).cast<Obj*>();
    mapperModel=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();
    const auto ids=BNM::Class("PvZCards.Game","NonEntityViewId");auto ally=ids.GetField("AllyLife").cast<int>(),opponent=ids.GetField("OpponentLife").cast<int>();
    const auto component=BNM::Class("UnityEngine","Component");getObject=component.GetMethod("get_gameObject",0).cast<Obj*>();getTransform=component.GetMethod("get_transform",0).cast<Obj*>();
    getObjectTransform=BNM::Class("UnityEngine","GameObject").GetMethod("get_transform",0).cast<Obj*>();
    active=BNM::Class("UnityEngine","GameObject").GetMethod("SetActive",1).cast<void>();
    renderers=BNM::Class("UnityEngine","GameObject").GetMethod("GetComponentsInChildren",{"type","includeInactive"}).cast<BNM::Structures::Mono::Array<Obj*>*>();
    instantiate=BNM::Class("UnityEngine","Object").GetMethod("Instantiate",{"original","parent","instantiateInWorldSpace"}).cast<Obj*>();
    spriteType=BNM::Class("UnityEngine","SpriteRenderer");getColor=spriteType.GetMethod("get_color",0).cast<Color>();setColor=spriteType.GetMethod("set_color",1).cast<void>();
    textType=BNM::Class("TMPro","TextMeshPro");setText=textType.GetMethod("set_text",1).cast<void>();
    setPosition=BNM::Class("UnityEngine","Transform").GetMethod("set_localPosition",1).cast<void>();
    setName=BNM::Class("UnityEngine","Object").GetMethod("set_name",1).cast<void>();
    getName=BNM::Class("UnityEngine","Object").GetMethod("get_name",0).cast<BNM::Structures::Mono::String*>();
    setInteger=BNM::Class("UnityEngine","Animator").GetMethod("SetInteger",{"id","value"}).cast<void>();
    shieldParameterField=BNM::Class("","AnimatorParameters").GetField("ShieldsRemaining").cast<Obj*>();
    parameterId=BNM::Class("","AnimationParameter").GetMethod("get_Id",0).cast<int>();
    if(!shield.IsValid() || !meter.IsValid() || !animator.IsValid() || !textUI.IsValid() || !remaining.IsValid() || !getView.IsValid() || !mapperModel.IsValid() || !ally.IsValid() || !opponent.IsValid() || !getObject.IsValid() || !getTransform.IsValid() || !getObjectTransform.IsValid() || !active.IsValid() || !renderers.IsValid() || !instantiate.IsValid() || !getColor.IsValid() || !setColor.IsValid() || !setText.IsValid() || !setPosition.IsValid() || !setName.IsValid() || !getName.IsValid() || !setInteger.IsValid() || !shieldParameterField.IsValid() || !parameterId.IsValid())return false;
    localView=ally.Get();opponentView=opponent.Get();
    // Native-crash isolation (19:59 SIGSEGV before mulligan):
    // By default we deliberately DO NOT detour the six opening-superpower
    // lifecycle methods. This is reversible via CustomComponents/config.ini:
    // [SuperpowerDiagnostics] EnableOpeningLifecycleHooks=true
    // Keep the existing shield refresh callback active, so this test isolates
    // only the six opening-lifecycle detours rather than all presentation code.
    // Caveat: the 4+-superpower golden-shield animation needs these lifecycle
    // signals; that presentation feature stays gated during this diagnostic.
    const bool installOpeningLifecycleHooks =
        BLOOM_CONFIG_BOOL("SuperpowerDiagnostics", "EnableOpeningLifecycleHooks", false);
    if (!installOpeningLifecycleHooks) {
        LOG_WARN("CustomComponents/Superpowers/LIFECYCLE_HOOKS: opening=0 regularTrace=unhooked (diagnostic bypass)");
        const bool tickReady = Core::AnimationQueuePostUpdate::Register(SafeTick);
        if (!tickReady) LOG_ERR("CustomComponents/Superpowers: shield-refresh callback unavailable");
        return tickReady;
    }
    const auto mulligan=BNM::Class("PvZCards.Game","MulliganPhaseEndRecordProcessor");
    const auto group=BNM::Class("PvZCards.Game","DrawCardsForHandAnimation");
    const bool openingLifecycle=Core::Hooks::Install(mulligan.GetMethod("ProcessRecord",1),MulliganEnd,originalMulliganEnd) &&
        Core::Hooks::Install(mulligan.GetMethod("PostProcessRecord",1),OpeningPost,originalOpeningPost) &&
        Core::Hooks::Install(mulligan.GetMethod("CreateAllyDrawSuperpowersAnimation",1),AllyOpening,originalAllyOpening) &&
        Core::Hooks::Install(mulligan.GetMethod("CreateOpponentDrawCardsAndPowersAnimation",1),OpponentOpening,originalOpponentOpening) &&
        Core::Hooks::Install(group.GetMethod("OnStarted",0),OpeningStarted,originalOpeningStarted) &&
        Core::Hooks::Install(group.GetMethod("IsCompleted",0),OpeningCompleted,originalOpeningCompleted);
    // Stock's regular superpower draw processor must remain unmodified.
    // Those three detours only logged the record/animation lifecycle; they
    // were not part of the opening-draw settlement algorithm. Avoid patching
    // this sensitive code path, especially while replay modules also observe
    // history processing.
    const bool lifecycle=openingLifecycle;
    LOG_INFO("CustomComponents/Superpowers/LIFECYCLE_HOOKS: opening=%d regularTrace=unhooked",int(openingLifecycle));
    if(!lifecycle)LOG_ERR("CustomComponents/Superpowers: native opening-power lifecycle hooks unavailable");
    return lifecycle && Core::AnimationQueuePostUpdate::Register(SafeTick) && History::Subscribe(PoolCreated);
}
}
