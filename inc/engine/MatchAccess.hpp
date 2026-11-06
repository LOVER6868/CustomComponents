#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Utils.hpp>
#include <BNM/Exceptions.hpp>
#include <string>
#include <vector>
#include "engine/EntityAccess.hpp"
#include "core/ManagedPool.hpp"
namespace CustomComponents::Engine::Match {
using Obj=BNM::IL2CPP::Il2CppObject;
inline int Id(Obj* entity) {static auto field=BNM::Class("PvZCards.Engine","Entity").GetField("<Id>k__BackingField").cast<int>();auto f=field;return entity && f.IsValid()?f[entity].Get():-1;}
inline Obj* Component(Obj* entity,BNM::MonoType* type,bool create=false) {
    if(!entity || !type) return nullptr;
    static auto has=BNM::Class("PvZCards.Engine","Entity").GetMethod("HasComponent",1).cast<bool>();
    static auto get=BNM::Class("PvZCards.Engine","Entity").GetMethod("GetComponent",1).cast<Obj*>();
    static auto add=BNM::Class("PvZCards.Engine","Entity").GetMethod("GetOrAddComponent",1).cast<Obj*>();
    auto h=has;auto g=get;auto a=add;
    if(!h.IsValid() || !g.IsValid()) return nullptr;
    return h[entity](type)?g[entity](type):create && a.IsValid()?a[entity](type):nullptr;
}
inline Obj* Instances(Obj* entity) {static auto field=BNM::Class("PvZCards.Engine","Entity").GetField("InstanceManager").cast<Obj*>();auto f=field;return entity && f.IsValid()?f[entity].Get():nullptr;}
inline Obj* Provide(Obj* instances,BNM::MonoType* type) {
    static auto injectorMethod=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("get_Injector",0).cast<Obj*>();
    auto inject=injectorMethod;Obj* injector=instances && inject.IsValid()?inject[instances]():nullptr;
    if(!injector || !type) return nullptr;
    auto binderMethod=BNM::Class(injector).GetMethod("get_binder",0).cast<Obj*>();
    Obj* binder=binderMethod.IsValid()?binderMethod[injector]():nullptr;if(!binder) return nullptr;
    auto binding=BNM::Class(binder).GetMethod("GetBinding",{std::string_view("key")}).cast<Obj*>();
    auto instance=BNM::Class(binder).GetMethod("GetInstance",{std::string_view("key")}).cast<Obj*>();
    if(!binding.IsValid() || !instance.IsValid() || !binding[binder](type)) return nullptr;
    return instance[binder](type);
}
inline std::shared_ptr<Core::ManagedRuntime::Root> Service(Obj* instances,BNM::Class type) {
    Obj* object=Provide(instances,type.GetMonoType());
    if(object)return std::make_shared<Core::ManagedRuntime::Root>(object);
    auto get=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("get_Injector",0).cast<Obj*>();
    Obj* injector=instances && get.IsValid()?get[instances]():nullptr;if(!injector)return {};
    auto inject=BNM::Class(injector).GetMethod("Inject",1).cast<void>();auto ctor=type.GetMethod(".ctor",0).cast<void>();
    if(!inject.IsValid() || !ctor.IsValid())return {};
    auto root=std::make_shared<Core::ManagedRuntime::Root>(type.CreateNewInstance());if(!root->Get())return {};
    auto error=BNM::TryInvoke([&]{ctor[root->Get()]();inject[injector](root->Get());});
    return error.IsValid()?nullptr:root;
}
inline std::vector<std::string> Tags(Obj* entity) {
    std::vector<std::string> result;Obj* component=EntityAccess::Component(entity,"Tags");
    static auto field=BNM::Class("PvZCards.Engine.Components","Tags").GetField("TagList").cast<BNM::Structures::Mono::List<BNM::Structures::Mono::String*>*>();
    auto f=field;auto* list=component && f.IsValid()?f[component].Get():nullptr;
    if(list && list->items && list->size>=0 && list->size<=int(list->items->capacity))
        for(int i=0;i<list->size;++i) if(auto* value=list->items->m_Items[i])result.push_back(value->str());
    return result;
}
inline bool Tag(Obj* entity,const std::string& prefix,std::string& value) {
    Obj* component=EntityAccess::Component(entity,"Tags");if(!component)return false;
    static auto field=BNM::Class("PvZCards.Engine.Components","Tags").GetField("TagList").cast<BNM::Structures::Mono::List<BNM::Structures::Mono::String*>*>();auto f=field;
    auto* list=f.IsValid()?f[component].Get():nullptr;
    if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return false;
    for(int i=0;i<list->size;++i)if(auto* text=list->items->m_Items[i]) {
        const auto tag=text->str();if(tag.rfind(prefix,0)==0){value=tag.substr(prefix.size());return true;}
    }
    return false;
}
inline bool SetTag(Obj* entity,const std::string& prefix,const std::string& value) {
    auto type=BNM::Class("PvZCards.Engine.Components","Tags");Obj* component=Component(entity,type.GetMonoType(),true);
    auto field=type.GetField("TagList").cast<Obj*>();Obj* list=component && field.IsValid()?field[component].Get():nullptr;
    if(!list) return false;
    auto* values=reinterpret_cast<BNM::Structures::Mono::List<BNM::Structures::Mono::String*>*>(list);
    if(!values->items || values->size<0 || values->size>int(values->items->capacity)) return false;
    auto set=BNM::Class(list).GetMethod("set_Item",2).cast<void>();auto add=BNM::Class(list).GetMethod("Add",1).cast<void>();
    if(!set.IsValid() || !add.IsValid()) return false;
    for(int i=0;i<values->size;++i) if(auto* text=values->items->m_Items[i];text && text->str().rfind(prefix,0)==0) {set[list](i,BNM::CreateMonoString(prefix+value));return true;}
    add[list](BNM::CreateMonoString(prefix+value));return true;
}
inline int Roll(Obj* entity,int count) {
    if(count<=0) return -1;if(count==1)return 0;
    auto type=BNM::Class("PvZCards.Core.Utilities","MtRandom");Obj* rng=Provide(Instances(entity),type.GetMonoType());
    auto next=type.GetMethod("Next",{BNM::Class("System","Int32").GetCompileTimeClass()}).cast<int>();
    if(!rng || !next.IsValid()) return -1;int value=next[rng](count);return value>=0 && value<count?value:-1;
}
}


