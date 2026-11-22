#pragma once
#include <unordered_map>
#include <atomic>
#include <cmath>
#include <BNM/UnityStructures.hpp>
#include <BNM/Exceptions.hpp>
#include "core/SceneViewContext.hpp"
#include "core/MetadataBinding.hpp"
#include "engine/FrontendServices.hpp"
#include "presentation/CardReadiness.hpp"
#include "diagnostics/HistoryTrace.hpp"
#include "presentation/history/NativeAnimation.hpp"

namespace CustomComponents::Presentation::DiscardCardAnimation {
using Obj=BNM::IL2CPP::Il2CppObject;
using Vec3=BNM::Structures::Unity::Vector3;
inline std::uintptr_t scene=0;
enum class Phase {Lift, Shrink};
struct Animation {
    std::shared_ptr<Core::ManagedRuntime::Root> root;
    std::uintptr_t view=0,hand=0,model=0;
    Phase phase=Phase::Shrink;
    Vec3 origin{},destination{},startScale{},targetScale{};
};
inline std::unordered_map<int,Animation> animations;
// Called at the owning queue's retirement boundary. A false result retains
// the hand view until the native tween reports completion.
inline bool BeforeRetire(Obj* queue,Obj* mapper,int id,Obj* hand=nullptr,bool owned=false,Obj* ownedView=nullptr){
    const auto current=Core::SceneViewContext::ModelIdentity();
    if(scene!=current){animations.clear();scene=current;}
    static auto tween=BNM::Class("PvZCards.Game","TweenCardEffectAnimation");
    static auto completed=tween.GetMethod("IsCompleted",0).cast<bool>();
    bool popupFinished=false;
    static auto mapped=BNM::Class("PvZCards.Game","EntityViewMapper").GetMethod("GetViewObject",{std::string_view("entityId")}).cast<Obj*>();
    Obj* currentView=mapper && mapped.IsValid()?Core::Metadata::Bind(mapped,mapper)(id):nullptr;
    if(!currentView)currentView=ownedView;
    if(auto found=animations.find(id);found!=animations.end()){
        const auto& state=found->second;
        if(state.model!=current || (currentView && state.view!=reinterpret_cast<std::uintptr_t>(currentView))){
            LOG_WARN("CustomComponents/Discard/VIEW_CHANGED: card=%d original=%p now=%p; refuse stale tween retirement",
                id,reinterpret_cast<void*>(state.view),currentView);
            animations.erase(found);return false;
        }
        Obj* animation=state.root?state.root->Get():nullptr;
        if(!animation || !completed.IsValid()){
            LOG_WARN("CustomComponents/Discard/ANIMATION_MISSING: card=%d; retaining visual for native recovery",id);
            animations.erase(found);return false;
        }
        if(owned)NativeAnimation::Call(animation,"Update");
        if(!Core::Metadata::Bind(completed,animation)())return false;
        if(owned)NativeAnimation::Call(animation,"FinishAnimation");
        popupFinished=state.phase==Phase::Lift;
        Diagnostics::HistoryTrace::Animation(popupFinished?"LIFT_COMPLETE":"SHRINK_COMPLETE",queue,animation,"TweenCardEffectAnimation",id);
        LOG_INFO("CustomComponents/Discard/%s: card=%d view=%p tween=%p hand=%p",
            popupFinished?"LIFT_COMPLETE":"SHRINK_COMPLETE",id,currentView,animation,
            reinterpret_cast<void*>(state.hand));
        animations.erase(found);
        if(!popupFinished)return true;
    }
    static auto get=BNM::Class("PvZCards.Game","EntityViewMapper").GetMethod("GetViewObject",{std::string_view("entityId")}).cast<Obj*>();
    static auto component=BNM::Class("UnityEngine","Component");
    static auto transform=component.GetMethod("get_transform",0).cast<Obj*>();
    static auto position=BNM::Class("UnityEngine","Transform").GetMethod("get_position",0).cast<Vec3>();
    static auto localScale=BNM::Class("UnityEngine","Transform").GetMethod("get_localScale",0).cast<Vec3>();
    static auto point=BNM::Class("UnityEngine","Transform").GetMethod("TransformPoint",1).cast<Vec3>();
    static auto handType=BNM::Class("PvZCards.Game","HandViewComponent");
    static auto kind=handType.GetField("ViewId").cast<int>();
    static auto offset=handType.GetField("SelectedCardOffset").cast<Vec3>();
    static auto upscale=handType.GetField("SelectedCardUpscale").cast<Vec3>();
    // Stock hand geometry is a reference point for the capture, not a reason
    // to reveal an opponent's hidden card or guess world/local coordinates.
    static auto nativePosition=handType.GetMethod("GetPositionForCardIncludingSelectedCardOffset",1).cast<Vec3>();
    static auto nativeScale=handType.GetMethod("GetScaleForCard",1).cast<Vec3>();
    static auto dim=BNM::Class("PvZCards.Game","CardViewComponent").GetMethod("EnableDimmer",0).cast<void>();
    static auto create=BNM::Class("PvZCards.Game","EffectAnimationFactory").GetMethod("CreateEffectAnimation",0).GetGeneric({tween.GetCompileTimeClass()}).cast<Obj*>();
    static auto card=tween.GetField("MoveCardView").cast<Obj*>();
    static auto target=tween.GetField("TargetPosition").cast<Vec3>();
    static auto scale=tween.GetField("TargetScale").cast<Vec3>();
    static auto duration=tween.GetField("TweenAnimationTime").cast<float>();
    static auto yield=tween.GetField("AlwaysYield").cast<bool>();
    static auto add=BNM::Class("PvZCards.Game","EffectAnimationQueue").GetMethod("AddAnimation",1).cast<void>();
    if(!queue || !mapper || !get.IsValid() || !transform.IsValid() || !position.IsValid() || !dim.IsValid() ||
       !create.IsValid() || !card.IsValid() || !target.IsValid() || !scale.IsValid() || !duration.IsValid() || !yield.IsValid() || !add.IsValid())return true;
    Obj* view=nullptr;
    auto lookup=BNM::TryInvoke([&]{view=Core::Metadata::Bind(get,mapper)(id);});
    if(!view)view=ownedView;
    if((lookup.IsValid() && !ownedView) || !view)return true;
    Obj* factory=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationFactory"),Engine::FrontendServices::Game());
    if(!factory)return true;
    Obj* animation=Core::Metadata::Bind(create,factory)();
    auto root=std::make_shared<Core::ManagedRuntime::Root>(animation);
    Obj* viewTransform=Core::Metadata::Bind(transform,view)();
    if(!root->Get() || !viewTransform || !Core::ManagedRuntime::SetReference(animation,card,view))return true;
    CardReadiness::Dim(view);
    Vec3 destination=Core::Metadata::Bind(position,viewTransform)();
    Vec3 targetScale(0,0,0);
    const Vec3 startPosition=destination;
    const Vec3 startScale=localScale.IsValid()?Core::Metadata::Bind(localScale,viewTransform)():Vec3(1,1,1);
    const int viewId=hand && kind.IsValid()?Core::Metadata::Bind(kind,hand).Get():-1;
    if(hand && viewId==7 && nativePosition.IsValid() && nativeScale.IsValid()){
        const Vec3 stockPosition=Core::Metadata::Bind(nativePosition,hand)(view);
        const Vec3 stockScale=Core::Metadata::Bind(nativeScale,hand)(view);
        LOG_INFO("CustomComponents/Discard/NATIVE_OPPONENT_GEOMETRY: card=%d hand=%p view=%p stockSelectedPos=(%.3f,%.3f,%.3f) stockCardScale=(%.3f,%.3f,%.3f) currentWorldPos=(%.3f,%.3f,%.3f)",
            id,hand,view,stockPosition.x,stockPosition.y,stockPosition.z,
            stockScale.x,stockScale.y,stockScale.z,startPosition.x,startPosition.y,startPosition.z);
    }
    bool popup=!popupFinished && hand && viewId==7 && offset.IsValid() && upscale.IsValid() && localScale.IsValid() && point.IsValid();
    if(popup){
        Obj* handTransform=Core::Metadata::Bind(transform,hand)();
        if(handTransform){
            const auto end=Core::Metadata::Bind(point,handTransform)(Core::Metadata::Bind(offset,hand).Get());
            const auto origin=Core::Metadata::Bind(point,handTransform)(Vec3(0,0,0));
            destination=Vec3(destination.x+end.x-origin.x,destination.y+end.y-origin.y,destination.z+end.z-origin.z);
        }
        const auto factor=Core::Metadata::Bind(upscale,hand).Get();
        const Vec3 moved=Vec3(destination.x-startPosition.x,destination.y-startPosition.y,destination.z-startPosition.z);
        const bool movedEnough=std::isfinite(moved.x) && std::isfinite(moved.y) && std::isfinite(moved.z) &&
            moved.x*moved.x+moved.y*moved.y+moved.z*moved.z>0.00001f;
        const bool scaledEnough=std::isfinite(factor.x) && std::isfinite(factor.y) && std::isfinite(factor.z) &&
            factor.x>1.001f && factor.y>1.001f && factor.z>0.f;
        popup=movedEnough && scaledEnough;
        if(popup)targetScale=Vec3(startScale.x*factor.x,startScale.y*factor.y,startScale.z*factor.z);
        else {destination=startPosition;
            LOG_INFO("CustomComponents/Discard/LIFT_NOT_CONFIGURED: card=%d viewId=%d offset=(%.3f,%.3f,%.3f) upscale=(%.3f,%.3f,%.3f) moved=%d scaled=%d",
                id,viewId,moved.x,moved.y,moved.z,factor.x,factor.y,factor.z,int(movedEnough),int(scaledEnough));}
    }
    Core::Metadata::Bind(target,animation).Set(destination);
    Core::Metadata::Bind(scale,animation).Set(targetScale);
    Core::Metadata::Bind(duration,animation).Set(popup?0.18f:0.24f);
    Core::Metadata::Bind(yield,animation).Set(false);
    animations[id]={std::move(root),reinterpret_cast<std::uintptr_t>(view),
        reinterpret_cast<std::uintptr_t>(hand),current,popup?Phase::Lift:Phase::Shrink,
        startPosition,destination,startScale,targetScale};
    Diagnostics::HistoryTrace::Animation(popup?"DISCARD_LIFT_ENQUEUE":"DISCARD_SHRINK_ENQUEUE",queue,animation,"TweenCardEffectAnimation",id);
    if(owned){
        if(!NativeAnimation::Call(animation,"Start")){
            animations.erase(id);
            LOG_WARN("CustomComponents/Discard/RECORD_TWEEN_START_FAILED: card=%d",id);
            return true;
        }
    }else Core::Metadata::Bind(add,queue)(animation);
    LOG_INFO("CustomComponents/Discard/%s: card=%d view=%p hand=%p viewId=%d nativeCompletion=1 from=(%.3f,%.3f,%.3f) to=(%.3f,%.3f,%.3f) scale=(%.3f,%.3f,%.3f)->(%.3f,%.3f,%.3f)",
        popup?"LIFT_BEGIN":"SHRINK_BEGIN",id,view,hand,viewId,
        startPosition.x,startPosition.y,startPosition.z,destination.x,destination.y,destination.z,
        startScale.x,startScale.y,startScale.z,targetScale.x,targetScale.y,targetScale.z);
    return false;
}
}
