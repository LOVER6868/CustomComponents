#include "core/AtomicSnapshot.hpp"
#include <mutex>
#pragma once
#include <string>
#include <vector>
#include "engine/Events.hpp"
#include "engine/EntityAccess.hpp"
#include "core/SceneViewContext.hpp"
#include "core/ManagedRuntime.hpp"
#include <BNM/Utils.hpp>
namespace CustomComponents::Presentation::FrameIcons {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Vec3 {float x,y,z;};struct Color {float r,g,b,a;};
enum class Visibility {Hidden,Dim,Bright};
struct Context {Obj* model;Obj* entity;Obj* view;};
struct Icon {
    std::string key;
    Obj*(*sprite)(const Context&)=nullptr;
    Visibility(*state)(const Context&)=nullptr;
    float x=-2.3f,y=3.5f,z=-0.01f,scale=1;
    int orderOffset=1;
};
inline Core::AtomicSnapshot<std::vector<Icon>> icons{std::make_shared<const std::vector<Icon>>()};
inline std::mutex registrationLock;
inline bool Register(Icon icon) {
    if(icon.key.empty() || !icon.sprite || !icon.state)return false;
    std::lock_guard<std::mutex> guard(registrationLock);auto prior=icons.load();
    for(const auto& current:*prior)if(current.key==icon.key)return current.sprite==icon.sprite && current.state==icon.state;
    auto next=std::make_shared<std::vector<Icon>>(*prior);next->push_back(std::move(icon));icons.store(next);return true;
}
// This service creates no feature icons by itself. Providers own sprite assets;
// child objects are owned and destroyed by the card's frame hierarchy.
inline void Refresh(Obj* view,Obj* model) {
    auto registered=icons.load();if(registered->empty() || !view || !model)return;
    auto viewType=BNM::Class("PvZCards.Game","CardViewComponent");if(!BNM::IsA(view,viewType))return;
    auto id=viewType.GetField("CardId").cast<int>();auto frameField=viewType.GetField("Frame").cast<Obj*>();
    if(!id.IsValid() || !frameField.IsValid())return;Obj* entity=Engine::EntityAccess::Find(model,id[view].Get());Obj* frame=frameField[view].Get();if(!frame)return;
    auto component=BNM::Class("UnityEngine","Component"),gameObject=BNM::Class("UnityEngine","GameObject"),transform=BNM::Class("UnityEngine","Transform");
    auto getTransform=component.GetMethod("get_transform",0).cast<Obj*>();auto getObject=component.GetMethod("get_gameObject",0).cast<Obj*>();
    auto find=transform.GetMethod("Find",1).cast<Obj*>();auto setParent=transform.GetMethod("SetParent",2).cast<void>();
    auto position=transform.GetMethod("set_localPosition",1).cast<void>();auto scale=transform.GetMethod("set_localScale",1).cast<void>();
    auto active=gameObject.GetMethod("SetActive",1).cast<void>();auto add=gameObject.GetMethod("AddComponent",1).cast<Obj*>();auto get=component.GetMethod("GetComponent",1).cast<Obj*>();
    auto spriteType=BNM::Class("UnityEngine","SpriteRenderer"),rendererType=BNM::Class("UnityEngine","Renderer");
    auto setSprite=spriteType.GetMethod("set_sprite",1).cast<void>();auto color=spriteType.GetMethod("set_color",1).cast<void>();
    auto frontField=BNM::Class(frame).GetField("FrameFrontSprite").cast<Obj*>();Obj* front=frontField.IsValid()?frontField[frame].Get():nullptr;
    if(!getTransform.IsValid() || !getObject.IsValid() || !find.IsValid() || !setParent.IsValid() || !position.IsValid() || !scale.IsValid() || !active.IsValid() || !add.IsValid() || !get.IsValid() || !setSprite.IsValid() || !color.IsValid())return;
    Obj* parent=getTransform[frame]();if(!parent)return;
    Context context{model,entity,view};
    for(const auto& icon:*registered) {
        auto name=BNM::CreateMonoString("CustomComponents.Icon."+icon.key);Obj* child=find[parent](name);
        auto visibility=entity?icon.state(context):Visibility::Hidden;
        if(visibility==Visibility::Hidden) {if(child)active[getObject[child]()](false);continue;}
        Obj* sprite=icon.sprite(context);if(!sprite){if(child)active[getObject[child]()](false);continue;}
        Obj* renderer=nullptr;
        if(!child) {
            Obj* object=gameObject.CreateNewObjectParameters(name);Core::ManagedRuntime::Root root(object);if(!root.Get())continue;
            child=gameObject.GetMethod("get_transform",0).cast<Obj*>()[object]();renderer=add[object](spriteType.GetMonoType());
            if(!child || !renderer)continue;setParent[child](parent,false);
        } else renderer=get[child](spriteType.GetMonoType());
        if(!renderer)continue;
        setSprite[renderer](sprite);position[child](Vec3{icon.x,icon.y,icon.z});scale[child](Vec3{icon.scale,icon.scale,icon.scale});
        if(front) {
            auto material=rendererType.GetMethod("get_sharedMaterial",0).cast<Obj*>();auto setMaterial=rendererType.GetMethod("set_sharedMaterial",1).cast<void>();
            auto layer=rendererType.GetMethod("get_sortingLayerID",0).cast<int>();auto setLayer=rendererType.GetMethod("set_sortingLayerID",1).cast<void>();
            auto order=rendererType.GetMethod("get_sortingOrder",0).cast<int>();auto setOrder=rendererType.GetMethod("set_sortingOrder",1).cast<void>();
            if(material.IsValid() && setMaterial.IsValid())setMaterial[renderer](material[front]());
            if(layer.IsValid() && setLayer.IsValid())setLayer[renderer](layer[front]());
            if(order.IsValid() && setOrder.IsValid())setOrder[renderer](order[front]()+icon.orderOffset);
        }
        float brightness=visibility==Visibility::Bright?1.f:.4f;color[renderer](Color{brightness,brightness,brightness,1});active[getObject[child]()](true);
    }
}
inline void Added(const Engine::Events::View& context){Refresh(context.view,context.model);}
inline void Shown(Obj* view) {
    Obj* mapper=Core::SceneViewContext::RootedMapper();auto field=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();
    if(mapper && field.IsValid())Refresh(view,field[mapper].Get());
}
inline bool Initialize(){return Engine::Events::afterViewAdded.Add(Added) && Engine::Events::afterShowInHand.Add(Shown);}
}

