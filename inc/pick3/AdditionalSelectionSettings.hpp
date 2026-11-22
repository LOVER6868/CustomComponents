#pragma once
#include <mutex>
#include <vector>
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedRuntime.hpp"
#include "pick3/AdditionalSelectionPolicy.hpp"
namespace CustomComponents::Pick3::AdditionalSelection {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Setting {int mode=Inherit;int primary=-1;bool first=false;};
struct SettingEntry {std::shared_ptr<Core::ManagedRuntime::Root> object;Setting value;};
inline std::mutex settingsMutex;
inline std::vector<SettingEntry> settings;
inline BNM::Class primaryClass;
inline Setting Get(Obj* filter) {
    if(!filter)return {};
    std::lock_guard<std::mutex> lock(settingsMutex);
    for(const auto& entry:settings)if(entry.object->Get()==filter)return entry.value;
    return {};
}
inline void Set(Obj* filter,Setting value) {
    if(!filter || !primaryClass.IsValid() || filter->klass!=primaryClass.GetClass())return;
    std::lock_guard<std::mutex> lock(settingsMutex);
    settings.erase(std::remove_if(settings.begin(),settings.end(),[&](const auto& entry){auto* p=entry.object->Get();return !p || p==filter;}),settings.end());
    if(value.mode==Inherit)return;
    auto root=std::make_shared<Core::ManagedRuntime::Root>(filter,true);
    if(root->Get())settings.push_back({root,value});
}
inline void ReadMap(Obj* object,Obj* properties) {
    if(!object || !properties || !primaryClass.IsValid() || object->klass!=primaryClass.GetClass())return;
    namespace Bridge=Systems::CustomFieldBridgeSystem;
    Obj* value=Bridge::DictionaryValue(properties,"AdditionalTargetSelectionType");
    if(!value)return;
    auto field=BNM::Class("PvZCards.Engine.Components","TargetEntityFilter").GetField("SelectionType").cast<int>();
    int primary=field.IsValid()?field[object].Get():-1;
    if(Obj* declared=Bridge::DictionaryValue(properties,"SelectionType"))if(auto* name=Bridge::ObjectToString(declared)) {
        const auto text=name->str();primary=(text=="PickFromThreeCardDatabase" || text=="5")?5:(text=="PickFromThreeRandomTargets"?3:Parse(text));
    }
    auto* text=value?Bridge::ObjectToString(value):nullptr;
    const int mode=text?Parse(text->str()):Inherit;
    if(primary<3 || primary>5){Set(object,{});return;}
    if(mode==Invalid)LOG_ERR("CustomComponents/AdditionalSelection: invalid AdditionalTargetSelectionType");
    Obj* firstValue=Bridge::DictionaryValue(properties,"AdditionalTargetFirst");
    auto* firstText=firstValue?Bridge::ObjectToString(firstValue):nullptr;
    const auto firstName=firstText?firstText->str():std::string{};
    Set(object,{mode,primary,firstName=="true" || firstName=="True" || firstName=="1"});
}
inline void Read(Obj* object,Obj* serialized) {
    if(!object || !serialized || !primaryClass.IsValid() || object->klass!=primaryClass.GetClass())return;
    namespace Bridge=Systems::CustomFieldBridgeSystem;
    auto convert=Bridge::convertPropertiesToDictionary;
    if(!convert.IsValid())return;
    Obj* properties=convert[static_cast<Obj*>(nullptr)](serialized);if(!properties)return;
    if(Obj* data=Bridge::DictionaryValue(properties,"$data"))
        if(Obj* nested=convert[static_cast<Obj*>(nullptr)](data))properties=nested;
    ReadMap(object,properties);
}
inline void ReadProperties(Obj* object,Obj* properties) {
    if(!object || !properties || !primaryClass.IsValid() || object->klass!=primaryClass.GetClass())return;
    Set(object,{});ReadMap(object,properties);
    // Consume the serialized extension before the stock reflective setter.
    // No stock class layout or instance size is changed.
    auto remove=BNM::Class(properties).GetMethod("Remove",1).cast<bool>();
    if(remove.IsValid())for(const char* key:{"AdditionalTargetSelectionType","AdditionalTargetFirst"})remove[properties](BNM::CreateMonoString(key));
}
inline void(*originalCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*copyLogicalFilter)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*originalReturn)(Obj*,Obj*,const void*)=nullptr;
static void Copy(Obj* self,Obj* source,Obj* instances,const void* method) {
    const auto setting=Get(source);
    if(copyLogicalFilter)copyLogicalFilter(self,source,instances,method);
    else originalCopy(self,source,instances,method);
    Set(self,setting);
}
static void Return(Obj* self,Obj* instances,const void* method) {
    Set(self,{});originalReturn(self,instances,method);
}
inline bool InitializeSettings() {
    primaryClass=BNM::Class("PvZCards.Engine.Components","PrimaryTargetFilter");
    auto base=BNM::Class("PvZCards.Engine.Components","TargetEntityFilter");
    Systems::CustomFieldBridgeSystem::beforeProperties.Add(ReadProperties);
    Systems::CustomFieldBridgeSystem::afterDeserialize.Add(Read);
    return primaryClass.IsValid() && Core::Hooks::Install(base.GetMethod("DeepCopyFrom",2),Copy,originalCopy) &&
        Core::Hooks::Install(base.GetMethod("HandleReturnToPool",1),Return,originalReturn);
}
}
