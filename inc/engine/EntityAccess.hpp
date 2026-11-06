#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <cstring>
namespace CustomComponents::Engine::EntityAccess {
using Obj=BNM::IL2CPP::Il2CppObject;
using List=BNM::Structures::Mono::List<Obj*>;
struct Metadata {BNM::Field<List*> entities,components;BNM::Field<int> id;};
inline const Metadata& Api() {
    static const Metadata api{
        BNM::Class("PvZCards.Engine","EntityModel").GetField("Entities").cast<List*>(),
        BNM::Class("PvZCards.Engine","Entity").GetField("Components").cast<List*>(),
        BNM::Class("PvZCards.Engine","Entity").GetField("<Id>k__BackingField").cast<int>()};
    return api;
}
inline bool Valid(const List* list) {return list && list->items && list->size>=0 && list->size<=int(list->items->capacity);}
inline Obj* Find(Obj* model,int id) {
    auto api=Api();
    if(!model || id<0 || !api.entities.IsValid() || !api.id.IsValid()) return nullptr;
    auto* list=api.entities[model].Get();if(!Valid(list)) return nullptr;
    for(int i=0;i<list->size;++i) {
        auto* entity=list->items->m_Items[i];
        if(entity && api.id[entity].Get()==id) return entity;
    }
    return nullptr;
}
inline Obj* Component(Obj* entity,const char* name) {
    auto api=Api();
    if(!entity || !name || !api.components.IsValid()) return nullptr;
    auto* list=api.components[entity].Get();if(!Valid(list)) return nullptr;
    for(int i=0;i<list->size;++i) {
        auto* object=list->items->m_Items[i];if(!object) continue;
        auto* type=BNM::Class(object).GetClass();
        if(type && type->name && std::strcmp(type->name,name)==0) return object;
    }
    return nullptr;
}
}
