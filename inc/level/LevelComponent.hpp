#pragma once
#include <charconv>
#include <cmath>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "level/LevelPolicy.hpp"
#include "level/LevelNumberPolicy.hpp"
#include "core/WeakObjectSettings.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
namespace CustomComponents::Level {
using Obj=BNM::IL2CPP::Il2CppObject;
inline Core::WeakObjectSettings<std::shared_ptr<const Definition>> definitions;
struct Component:Obj {
    BNM_CustomClass(Component,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","Level","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    void DeepCopyFrom(Obj* source,Obj*){definitions.Set(this,definitions.Get(source));}
    void HandleReturnToPool(Obj*){definitions.Set(this,{},false);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
namespace Bridge=Systems::CustomFieldBridgeSystem;
inline bool Integer(Obj* object,int& value) {
    if(!object || !object->klass)return false;
    const std::string_view type=object->klass->name;
    // JSON numbers are boxed value types. Invoking a value-type ToString as
    // an object method passes the box header as the value on IL2CPP.
    if(Bridge::objectUnbox && (type=="Int32" || type=="Int64" || type=="Double" || type=="Single"))if(void* raw=Bridge::objectUnbox(object)) {
        if(type=="Int32")return ReadInteger(*static_cast<std::int32_t*>(raw),value);
        if(type=="Int64")return ReadInteger(*static_cast<std::int64_t*>(raw),value);
        if(type=="Double")return ReadInteger(*static_cast<double*>(raw),value);
        if(type=="Single")return ReadInteger(*static_cast<float*>(raw),value);
    }
    if(type!="String")return false;
    const auto s=reinterpret_cast<BNM::Structures::Mono::String*>(object)->str();auto result=std::from_chars(s.data(),s.data()+s.size(),value);return result.ec==std::errc{} && result.ptr==s.data()+s.size();
}
inline bool ParseStat(Obj* object,Stat& value) {if(!object){value={};return true;}if(object->klass && std::string_view(object->klass->name)=="String"){const auto text=reinterpret_cast<BNM::Structures::Mono::String*>(object)->str();if(text=="X" || text=="x"){value={0,true};return true;}}return Integer(object,value.value);}
inline void ReadMap(Obj* object,Obj* properties) {
    if(!object || !object->klass || std::string_view(object->klass->name)!="Level")return;
    Obj* list=Bridge::DictionaryValue(properties,"Levels");if(!list)return;
    auto count=BNM::Class(list).GetMethod("get_Count",0).cast<int>();auto item=BNM::Class(list).GetMethod("get_Item",1).cast<Obj*>();auto convert=Bridge::convertPropertiesToDictionary;
    auto* array=list->klass->rank==1?reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(list):nullptr;
    auto definition=std::make_shared<Definition>();bool valid=convert.IsValid() && (array || (count.IsValid() && item.IsValid()));int size=valid?(array?int(array->capacity):count[list]()):-1;
    valid=valid && size>0 && size<=1024;
    int failedRow=-1;const char* failedField="list";
    for(int i=0;valid && i<size;++i){Obj* row=convert[static_cast<Obj*>(nullptr)](array?array->m_Items[i]:item[list](i));Entry entry;failedRow=i;failedField="SunCost";valid=row && Integer(Bridge::DictionaryValue(row,"SunCost"),entry.sun) && entry.sun>=-1;if(valid){failedField="Attack";valid=ParseStat(Bridge::DictionaryValue(row,"Attack"),entry.attack);}if(valid){failedField="Health";valid=ParseStat(Bridge::DictionaryValue(row,"Health"),entry.health);}if(valid)definition->entries.push_back(entry);}
    definitions.Set(object,valid?definition:nullptr,valid);if(!valid)LOG_ERR("CustomComponents/Level: invalid Levels table list=%s count=%d row=%d field=%s",list->klass->name,size,failedRow,failedField);
    else {static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<16)LOG_INFO("CustomComponents/Level/TABLE: entries=%d list=%s",size,list->klass->name);}
}
inline void ReadProperties(Obj* object,Obj* properties){if(!object || !object->klass || std::string_view(object->klass->name)!="Level")return;ReadMap(object,properties);auto remove=BNM::Class(properties).GetMethod("Remove",1).cast<bool>();if(remove.IsValid())remove[properties](BNM::CreateMonoString("Levels"));}
inline void ReadSerialized(Obj* object,Obj* serialized){if(!object || !object->klass || std::string_view(object->klass->name)!="Level")return;auto convert=Bridge::convertPropertiesToDictionary;if(!convert.IsValid())return;Obj* data=convert[static_cast<Obj*>(nullptr)](serialized);if(!data)return;if(Obj* nested=Bridge::DictionaryValue(data,"$data"))data=convert[static_cast<Obj*>(nullptr)](nested);ReadMap(object,data);}
inline bool InitializeComponent(){return Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","Level"),BNM::Class("PvZCards.Engine.Components","MarkerComponent")) && CustomComponentRegistry::Register({"Level","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) && Bridge::beforeProperties.Add(ReadProperties) && Bridge::afterDeserialize.Add(ReadSerialized);}
}
