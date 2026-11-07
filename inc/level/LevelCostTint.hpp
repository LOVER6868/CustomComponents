#pragma once
#include <algorithm>
#include <vector>
#include <atomic>
#include "level/LevelVisualStyle.hpp"
#include "presentation/CardStatState.hpp"
#include "core/ManagedRuntime.hpp"
#include "core/HookRegistry.hpp"
namespace CustomComponents::Level::CostTint {
using Obj=BNM::IL2CPP::Il2CppObject;
using VisualStyle::Color;
struct Color32 {std::uint8_t r,g,b,a;};
inline Color32 Bytes(Color c){return {std::uint8_t(c.r*255),std::uint8_t(c.g*255),std::uint8_t(c.b*255),std::uint8_t(c.a*255)};}
inline bool Same(Color32 a,Color32 b){return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;}
struct Owned {std::shared_ptr<Core::ManagedRuntime::Root> frame;bool seen=false,tags=false,gradient=false;Color32 face{255,255,255,255};};
inline std::vector<Owned> owned;
struct TextApi {
    BNM::Field<Obj*> label;
    BNM::Field<Color32> cachedColor;
    BNM::Method<Color32> face;
    BNM::Method<void> setFace;
    BNM::Method<Color> color;
    BNM::Method<bool> tags,gradient,active;
    BNM::Method<void> setColor,setTags,setGradient,mesh;
    bool Valid()const{return cachedColor.IsValid() && face.IsValid() && setFace.IsValid() && label.IsValid() && color.IsValid() && tags.IsValid() && gradient.IsValid() && setColor.IsValid() && setTags.IsValid() && setGradient.IsValid() && mesh.IsValid();}
};
inline const TextApi& Metadata(){static const auto api=[] {
    auto text=BNM::Class("TMPro","TextMeshPro");
    return TextApi{BNM::Class("PvZCards.Game","CardFrameViewComponent").GetField("SunCostUI").cast<Obj*>(),text.GetField("m_fontColor32").cast<Color32>(),text.GetMethod("get_faceColor",0).cast<Color32>(),text.GetMethod("set_faceColor",1).cast<void>(),text.GetMethod("get_color",0).cast<Color>(),text.GetMethod("get_overrideColorTags",0).cast<bool>(),text.GetMethod("get_enableVertexGradient",0).cast<bool>(),BNM::Class("UnityEngine","Behaviour").GetMethod("get_isActiveAndEnabled",0).cast<bool>(),text.GetMethod("set_color",1).cast<void>(),text.GetMethod("set_overrideColorTags",1).cast<void>(),text.GetMethod("set_enableVertexGradient",1).cast<void>(),text.GetMethod("ForceMeshUpdate",0).cast<void>()};
}();return api;}
inline void ReportFailure(const char* reason){static std::atomic<unsigned> count{0};if(count.fetch_add(1)<8)LOG_WARN("CustomComponents/Level/COST_TINT: unavailable reason=%s",reason);}
inline Color(*originalPick)(Obj*,int,int,const void*)=nullptr;
static Color Pick(Obj* frame,int initial,int current,const void* method) {
    for(const auto& item:owned)if(item.frame->Get()==frame)return VisualStyle::CostTint;
    return originalPick(frame,initial,current,method);
}
inline bool Alive(Obj* object){static auto metadata=BNM::Class("UnityEngine","Object").GetField("m_CachedPtr").cast<void*>();auto field=metadata;return object && field.IsValid() && field[object].Get();}
inline bool Same(Color a,Color b){return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;}
inline void Apply(Obj* frame,bool selected) {
    if(!Alive(frame))return;
    auto api=Metadata();if(!api.Valid() || !originalPick){if(selected)ReportFailure("metadata");return;}
    Obj* text=api.label[frame].Get();if(!Alive(text)){if(selected)ReportFailure("label");return;}
    auto stats=Presentation::CardStats::Metadata();
    const auto color=selected?VisualStyle::CostTint:originalPick(frame,stats.initialCost[frame].Get(),stats.cost[frame].Get(),nullptr);
    auto owner=std::find_if(owned.begin(),owned.end(),[&](auto& item){return item.frame->Get()==frame;});
    bool dirty=false;
    if(owner!=owned.end()){
        const Color32 face=selected?Color32{255,255,255,255}:owner->face;
        if(!Same(api.face[text](),face)){api.setFace[text](face);dirty=true;}
        const bool tags=selected?true:owner->tags,gradient=selected?false:owner->gradient;
        if(api.tags[text]()!=tags){api.setTags[text](tags);dirty=true;}
        if(api.gradient[text]()!=gradient){api.setGradient[text](gradient);dirty=true;}
    }
    if(!Same(api.cachedColor[text].Get(),Bytes(color))){api.cachedColor[text].Set(Bytes(color));dirty=true;}
    if(!Same(api.color[text](),color)){api.setColor[text](color);dirty=true;}
    if(dirty)api.mesh[text]();
}
inline void BeginFrame(){for(auto& item:owned)item.seen=false;}
inline void Present(Obj* frame,bool selected) {
    auto found=std::find_if(owned.begin(),owned.end(),[&](auto& item){return item.frame->Get()==frame;});
    if(selected){
        const bool fresh=found==owned.end();
        if(fresh){auto api=Metadata();if(!Alive(frame) || !api.Valid()){ReportFailure("frame-or-metadata");return;}Obj* text=api.label[frame].Get();if(!Alive(text)){ReportFailure("label");return;}owned.push_back({std::make_shared<Core::ManagedRuntime::Root>(frame,true),true,api.tags[text](),api.gradient[text](),api.face[text]()});}
        else found->seen=true;
        Apply(frame,true);
        if(fresh){static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<16){auto api=Metadata();Obj* text=api.label[frame].Get();auto color=api.color[text]();LOG_INFO("CustomComponents/Level/COST_TINT: label=%s active=%d rgb=%.2f,%.2f,%.2f gradient=%d overrideTags=%d face=%u,%u,%u",text->klass->name,api.active.IsValid()?int(api.active[text]()):-1,color.r,color.g,color.b,int(api.gradient[text]()),int(api.tags[text]()),unsigned(api.face[text]().r),unsigned(api.face[text]().g),unsigned(api.face[text]().b));}}
    }
    else if(found!=owned.end()){Apply(frame,false);owned.erase(found);}
}
inline void EndFrame(){owned.erase(std::remove_if(owned.begin(),owned.end(),[](auto& item){if(item.seen)return false;Apply(item.frame->Get(),false);return true;}),owned.end());}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Game","CardFrameViewComponent").GetMethod("PickSunCostColor",2),Pick,originalPick);}
}
