#pragma once
#include <cmath>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "variables/TimedValues.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/WeakObjectSettings.hpp"
#include "core/ManagedReference.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/SimulationChanges.hpp"
#include "history/RecordWriter.hpp"
namespace CustomComponents::Variables {
using Obj=BNM::IL2CPP::Il2CppObject;using Str=BNM::Structures::Mono::String;
namespace Bridge=Systems::CustomFieldBridgeSystem;
inline std::string Text(Str* s,std::string fallback={}){return s?s->str():fallback;}
inline void Reference(Obj* owner,Str** slot,Str* value){if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(owner,reinterpret_cast<void**>(slot),value);else *slot=value;}
struct Component:Obj {
    Str* Data=nullptr;
    Str* TimedData=nullptr;
    BNM_CustomClass(Component,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","Variables","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(Data,BNM::Defaults::Get<Str*>(),"Data");
    BNM_CustomField(TimedData,BNM::Defaults::Get<Str*>(),"TimedData");
    void DeepCopyFrom(Obj* source,Obj*){Reference(this,&Data,source?reinterpret_cast<Component*>(source)->Data:nullptr);Reference(this,&TimedData,source?reinterpret_cast<Component*>(source)->TimedData:nullptr);}
    void HandleReturnToPool(Obj*){Reference(this,&Data,nullptr);Reference(this,&TimedData,nullptr);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct Cached {std::shared_ptr<Core::ManagedRuntime::Root> payload;std::shared_ptr<const Table> table;};
inline Core::WeakObjectSettings<Cached> tables;
inline Component* State(Obj* card,bool create=false){
    Obj* existing=Engine::EntityAccess::Component(card,"Variables");
    if(existing || !create)return reinterpret_cast<Component*>(existing);
    static auto type=BNM::Class("PvZCards.Engine.Components","Variables");
    return reinterpret_cast<Component*>(type.IsValid()?Engine::Match::Component(card,type.GetMonoType(),true):nullptr);
}
inline void (*onVariableOwnerWritten)(Obj*,Obj*)=nullptr;
inline std::shared_ptr<const Table> Stored(Obj* card){
    auto* state=State(card);if(!state)return std::make_shared<const Table>();auto cached=tables.Get(state);
    if(cached.table && ((state->Data && cached.payload && cached.payload->Get()==reinterpret_cast<Obj*>(state->Data)) || (!state->Data && !cached.payload)))return cached.table;
    auto decoded=Decode(Text(state->Data));if(!decoded)return {};
    auto table=std::make_shared<const Table>(std::move(*decoded));tables.Set(state,{state->Data?std::make_shared<Core::ManagedRuntime::Root>(reinterpret_cast<Obj*>(state->Data),true):nullptr,table});return table;
}
inline void RecordWrite(Obj* model,Obj* card,bool timed,std::string before,const std::string& after){
    History::Record r;r.kind=History::Kind::VariableChanged;r.visual=History::VisualPolicy::NoVisual;
    r.entityId=Engine::Match::Id(card);r.payload=History::VariablePayload{r.entityId,timed,std::move(before),after};
    History::Writer::Commit(std::move(r),model);
}
inline bool Write(Obj* model,Obj* card,const Table& table){
    auto encoded=Encode(table);if(encoded.empty())return false;auto* state=State(card,true);if(!state)return false;
    if(onVariableOwnerWritten)onVariableOwnerWritten(model,card);
    if(Text(state->Data)==encoded)return true;
    const auto before=Text(state->Data);
    Reference(state,&state->Data,BNM::CreateMonoString(encoded));tables.Set(state,{},false);Engine::SimulationChanges::Changed(model);
    RecordWrite(model,card,false,before,encoded);return true;
}
struct CachedTimed {std::shared_ptr<Core::ManagedRuntime::Root> payload;std::shared_ptr<const TimedTable> rows;};
inline Core::WeakObjectSettings<CachedTimed> timedTables;
inline std::optional<TimedTable> Timed(Obj* card){
    auto* state=State(card);if(!state)return TimedTable{};
    auto cached=timedTables.Get(state);
    if(cached.rows && ((state->TimedData && cached.payload && cached.payload->Get()==reinterpret_cast<Obj*>(state->TimedData)) || (!state->TimedData && !cached.payload)))return *cached.rows;
    auto decoded=DecodeTimed(Text(state->TimedData));if(!decoded)return {};
    auto rows=std::make_shared<const TimedTable>(*decoded);
    timedTables.Set(state,{state->TimedData?std::make_shared<Core::ManagedRuntime::Root>(reinterpret_cast<Obj*>(state->TimedData),true):nullptr,rows});
    return decoded;
}
inline bool WriteTimed(Obj* model,Obj* card,const TimedTable& table){
    auto encoded=EncodeTimed(table);if(encoded.empty())return false;auto* state=State(card,true);if(!state)return false;
    if(onVariableOwnerWritten)onVariableOwnerWritten(model,card);
    if(Text(state->TimedData)==encoded)return true;
    const auto before=Text(state->TimedData);
    Reference(state,&state->TimedData,BNM::CreateMonoString(encoded));Engine::SimulationChanges::Changed(model);
    RecordWrite(model,card,true,before,encoded);return true;
}
inline bool ExpireTimed(Obj* model,int duration){
    auto api=Engine::EntityAccess::Api();auto* entities=model && api.entities.IsValid()?api.entities[model].Get():nullptr;
    if(!Engine::EntityAccess::Valid(entities))return false;bool changed=false;
    for(int i=0;i<entities->size;++i){Obj* card=entities->items->m_Items[i];if(!card || !Engine::EntityAccess::Component(card,"Card"))continue;
        auto* state=State(card);if(!state || !state->TimedData)continue;auto values=DecodeTimed(Text(state->TimedData));if(!values)continue;
        auto n=std::erase_if(*values,[&](const auto& row){return row.duration==duration;});if(n && WriteTimed(model,card,*values))changed=true;
    }return changed;
}
inline bool Integer(Obj* boxed,int& value){
    if(!boxed || !boxed->klass)return false;std::string_view name=boxed->klass->name;
    if(Bridge::objectUnbox){auto* raw=Bridge::objectUnbox(boxed);if(!raw)return false;if(name=="Int32"){value=*static_cast<int*>(raw);return true;}if(name=="Int64"){auto n=*static_cast<std::int64_t*>(raw);if(n<INT32_MIN || n>INT32_MAX)return false;value=int(n);return true;}if(name=="Double"){double n=*static_cast<double*>(raw);if(!std::isfinite(n) || std::trunc(n)!=n || n<INT32_MIN || n>INT32_MAX)return false;value=int(n);return true;}}
    if(name=="String"){auto text=reinterpret_cast<Str*>(boxed)->str();auto read=std::from_chars(text.data(),text.data()+text.size(),value);return read.ec==std::errc{} && read.ptr==text.data()+text.size();}return false;
}
inline std::optional<std::vector<Obj*>> Items(Obj* list){
    if(!list || !list->klass)return {};std::vector<Obj*> result;
    if(list->klass->rank==1){auto* a=reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(list);if(a->capacity>MaxArray)return {};for(std::size_t i=0;i<a->capacity;++i)result.push_back(a->m_Items[i]);return result;}
    auto count=BNM::Class(list).GetMethod("get_Count",0).cast<int>();auto item=BNM::Class(list).GetMethod("get_Item",{BNM::Class("System","Int32").GetCompileTimeClass()}).cast<Obj*>();if(!count.IsValid() || !item.IsValid())return {};int n=count[list]();if(n<0 || n>int(MaxArray))return {};for(int i=0;i<n;++i)result.push_back(item[list](i));return result;
}
inline void ReadDefaultsCore(Obj* object,Obj* properties){
    if(!object || !object->klass || std::string_view(object->klass->name)!="Variables")return;
    Obj* rows=Bridge::DictionaryValue(properties,"Values");if(!rows)return;auto entries=Items(rows);Table table;bool valid=Bridge::convertPropertiesToDictionary.IsValid() && bool(entries) && entries->size()<=MaxVariables;
    if(valid)for(Obj* row:*entries){auto convert=Bridge::convertPropertiesToDictionary;Obj* map=convert[static_cast<Obj*>(nullptr)](row);auto* name=Bridge::ObjectToString(Bridge::DictionaryValue(map,"Name"));auto* type=Bridge::ObjectToString(Bridge::DictionaryValue(map,"Type"));auto kind=ParseKind(type?type->str():"Int");if(!name || !kind){valid=false;break;}Value value{*kind,{}};Obj* input=Bridge::DictionaryValue(map,"Value");if(value.Array()){auto items=Items(input);if(!items){valid=false;break;}for(Obj* b:*items){int n;if(!Integer(b,n)){valid=false;break;}value.items.push_back(n);}}else{int n;if(!Integer(input,n)){valid=false;break;}value.items.push_back(n);}if(!valid || !Name(name->str()) || !value.Valid() || !table.emplace(name->str(),std::move(value)).second){valid=false;break;}}
    if(valid){auto* state=reinterpret_cast<Component*>(object);Reference(state,&state->Data,BNM::CreateMonoString(Encode(table)));tables.Set(object,{},false);}else LOG_ERR("CustomComponents/Variables: invalid authored Values table");
    auto remove=BNM::Class(properties).GetMethod("Remove",1).cast<bool>();if(remove.IsValid())remove[properties](BNM::CreateMonoString("Values"));
}
inline void ReadDefaults(Obj* object,Obj* properties){
    if(!object || !object->klass || std::string_view(object->klass->name)!="Variables")return;
    auto error=BNM::TryInvoke([&]{ReadDefaultsCore(object,properties);});if(error.IsValid())LOG_ERR("CustomComponents/Variables: malformed authored Values table");
}
inline void ReadSerializedCore(Obj* object,Obj* serialized){
    if(!object || !object->klass || std::string_view(object->klass->name)!="Variables")return;auto convert=Bridge::convertPropertiesToDictionary;if(!convert.IsValid())return;Obj* data=convert[static_cast<Obj*>(nullptr)](serialized);if(!data)return;if(Obj* nested=Bridge::DictionaryValue(data,"$data"))data=convert[static_cast<Obj*>(nullptr)](nested);ReadDefaults(object,data);
}
inline void ReadSerialized(Obj* object,Obj* serialized){
    if(!object || !object->klass || std::string_view(object->klass->name)!="Variables")return;
    auto error=BNM::TryInvoke([&]{ReadSerializedCore(object,serialized);});if(error.IsValid())LOG_ERR("CustomComponents/Variables: malformed serialized Values table");
}
inline bool InitializeStorage(){using K=Bridge::FieldKind;return CustomComponentRegistry::Register({"Variables","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) && Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","Variables"),BNM::Class("PvZCards.Engine.Components","MarkerComponent")) && Bridge::Register("PvZCards.Engine.Components","Variables","PvZCards.Engine.Components","MarkerComponent",{{"Data",K::String},{"TimedData",K::String}},false) && Bridge::beforeProperties.Add(ReadDefaults) && Bridge::afterDeserialize.Add(ReadSerialized);}
}
