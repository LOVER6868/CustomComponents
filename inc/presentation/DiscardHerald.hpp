#include <bloom/bloom.h>
#pragma once
#include "presentation/InspectionStatPresentation.hpp"
namespace CustomComponents::Presentation::Herald {
using Obj=BNM::IL2CPP::Il2CppObject;using String=BNM::Structures::Mono::String;
using Decorator=void*(*)(void*,int,void*,void*);
inline Decorator decorator=nullptr;
struct Context {Obj* model=nullptr;int entity=-1;bool discarded=false;};
inline Context Resolve(Obj* object,bool panel) {
    auto snapshot=InspectionStats::Find(panel?InspectionStats::panels:InspectionStats::items,object);
    if(!snapshot)return {};
    Obj* model=Core::ModelRegistry::Get(snapshot->model);Obj* entity=Engine::EntityAccess::Find(model,snapshot->entity);
    bool exact=entity && InspectionStats::Guid(entity)==snapshot->guid;
    return {exact?model:nullptr,exact?snapshot->entity:-1,
        snapshot->model==0?snapshot->discarded:exact&&InspectionStats::Discarded(entity)};
}
inline String* Format(Context context,String* current,Obj* factory=nullptr) { // sprig-lint: allow managed-global factory is a borrowed game reference used synchronously by the decorator
    const std::string previous=current?current->str():std::string{};
    const std::string origin=HeraldPolicy::Origin(context.discarded,previous);
    String* base=origin==previous?current:BNM::CreateMonoString(origin);
    if(decorator && context.model && context.entity>=0)
        if(auto* value=static_cast<String*>(decorator(context.model,context.entity,base,factory)))return value;
    return base;
}
struct PanelText {String* NameText;String* RarityText;String* DescriptionText;String* FlavorText;String* TypesText;String* CreationSourceText;bool HasDescription;};
static_assert(offsetof(PanelText,CreationSourceText)==0x28 && offsetof(PanelText,HasDescription)==0x30);
inline void(*originalText)(void*,PanelText,const void*)=nullptr;
inline void(*originalOverlay)(void*,Obj*,const void*)=nullptr;
inline String*(*originalCasual)(void*,int,const void*)=nullptr;
static void Text(void* self,PanelText text,const void* method) {
    text.CreationSourceText=Format(Resolve(static_cast<Obj*>(self),true),text.CreationSourceText);
    originalText(self,text,method);
}
static void Overlay(void* self,Obj* item,const void* method) {
    originalOverlay(self,item,method);auto context=Resolve(item,false);
    auto field=BNM::Class("PvZCards.CardInspection","CardInspectionOverlayViewMediator").GetField("<View>k__BackingField").cast<Obj*>();
    Obj* view=self && field.IsValid()?field[self].Get():nullptr;
    auto labelField=BNM::Class("PvZCards.CardInspection","CardInspectionOverlayView").GetField("CreationSourceLabel").cast<Obj*>();
    Obj* label=view && labelField.IsValid()?labelField[view].Get():nullptr;if(!label)return;
    auto get=BNM::Class("TMPro","TextMeshProUGUI").GetMethod("get_text",0).cast<String*>();auto set=BNM::Class("TMPro","TextMeshProUGUI").GetMethod("set_text",1).cast<void>();
    if(get.IsValid() && set.IsValid()){
        auto* previous=get[label]();
        auto* formatted=Format(context,previous);
        if(formatted!=previous)set[label](formatted);
    }
}
static String* Casual(void* self,int id,const void* method) {
    String* original=originalCasual(self,id,method);
    auto field=BNM::Class("PvZCards.Game","CardTooltipTextFactory").GetField("<ViewMap>k__BackingField").cast<Obj*>();Obj* mapper=self && field.IsValid()?field[self].Get():nullptr;
    auto modelField=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();Obj* model=mapper && modelField.IsValid()?modelField[mapper].Get():nullptr;
    Obj* entity=Engine::EntityAccess::Find(model,id);Context context{model,id,InspectionStats::Discarded(entity)};
    if(!context.discarded && !decorator)return original;
    String* extra=Format(context,nullptr,static_cast<Obj*>(self));if(!extra || extra->str().empty())return original;
    std::string text=original?original->str():std::string();auto color=text.find("<color=#ADCDF5>");if(color==std::string::npos)color=text.find("<color=#adcdf5>");
    if(!context.discarded) {
        if(color!=std::string::npos)return BNM::CreateMonoString(text+" — "+extra->str());
        return BNM::CreateMonoString(text+(text.empty()?"":"\n")+"<size=90%><color=#ADCDF5>"+extra->str()+"</color></size>");
    }
    if(color!=std::string::npos)text.erase(color);
    if(!text.empty())text+='\n';
    return BNM::CreateMonoString(text+"<size=90%><color=#ADCDF5>"+extra->str()+"</color></size>");
}
inline bool Initialize() {
    auto panel=BNM::Class("PvZCards.CardInspection","CardInspectionPanelView");auto overlay=BNM::Class("PvZCards.CardInspection","CardInspectionOverlayViewMediator");auto factory=BNM::Class("PvZCards.Game","CardTooltipTextFactory");
    bool a=Core::Hooks::Install(panel.GetMethod("SetText",1),Text,originalText);
    bool b=Core::Hooks::Install(overlay.GetMethod("CreateUiForItem",1),Overlay,originalOverlay);
    bool c=Core::Hooks::Install(factory.GetMethod("GetCasualText",1),Casual,originalCasual);
    return a && b && c;
}
}
// Versioned C ABI for optional modules in the same Sprig library. Symbol
// existence is independent of initialization order; hooks stay in this module.
extern "C" __attribute__((visibility("default"),used)) bool Sprig_RegisterHeraldDecoratorV1(CustomComponents::Presentation::Herald::Decorator callback) {
    if(!::Bloom::ConfigBool("CustomComponents","General","Enabled",true))return false;
    auto& current=CustomComponents::Presentation::Herald::decorator;
    if(!callback || (current && current!=callback))return false;current=callback;return true;
}




