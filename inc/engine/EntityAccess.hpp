#pragma once
#include "core/MetadataBinding.hpp"
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <cstring>
#include <unordered_map>
namespace CustomComponents::Engine::EntityAccess {
using Obj=BNM::IL2CPP::Il2CppObject;
using List=BNM::Structures::Mono::List<Obj*>;
struct Metadata {BNM::Field<List*> entities,components;BNM::Field<int> id;};
inline Metadata metadata{};
inline bool ready=false;
inline bool Initialize() {
    if(ready)return true;
    metadata={
        BNM::Class("PvZCards.Engine","EntityModel").GetField("Entities").cast<List*>(),
        BNM::Class("PvZCards.Engine","Entity").GetField("Components").cast<List*>(),
        BNM::Class("PvZCards.Engine","Entity").GetField("<Id>k__BackingField").cast<int>()};
    ready=metadata.entities.IsValid() && metadata.components.IsValid() && metadata.id.IsValid();
    return ready;
}
inline const Metadata& Api() {
    return metadata;
}
template<class T>
inline bool Valid(const BNM::Structures::Mono::List<T>* list) {
    return list && list->items && list->size>=0 && list->size<=int(list->items->capacity);
}
inline Obj* Find(Obj* model,int id) {
    auto api=Api();
    if(!model || id<0 || !api.entities.IsValid() || !api.id.IsValid()) return nullptr;
    auto* list=api.entities[model].Get();if(!Valid(list)) return nullptr;
    // Store indices rather than managed pointers. Rebuild when the stock list
    // changes, and verify the live slot before returning it. This avoids a
    // managed field read for every entity in every nested query.
    struct Index {const void* model=nullptr;const void* list=nullptr;const void* items=nullptr;int version=-1,count=-1;std::unordered_map<int,int> slots;};
    static thread_local Index index;
    auto rebuild=[&]{
        index={model,list,list->items,list->version,list->size,{}};
        index.slots.reserve(list->size);
        for(int i=0;i<list->size;++i)if(Obj* entity=list->items->m_Items[i])index.slots.emplace(Core::Metadata::Bind(api.id,entity).Get(),i);
    };
    if(index.model!=model || index.list!=list || index.items!=list->items || index.version!=list->version || index.count!=list->size)rebuild();
    auto found=index.slots.find(id);if(found==index.slots.end())return nullptr;
    Obj* entity=list->items->m_Items[found->second];
    if(entity && Core::Metadata::Bind(api.id,entity).Get()==id)return entity;
    rebuild();found=index.slots.find(id);
    return found==index.slots.end()?nullptr:list->items->m_Items[found->second];
}
inline Obj* Component(Obj* entity,const char* name) {
    auto api=Api();
    if(!entity || !name || !api.components.IsValid()) return nullptr;
    auto* list=api.components[entity].Get();if(!Valid(list)) return nullptr;
    for(int i=0;i<list->size;++i) {
        auto* object=list->items->m_Items[i];if(!object) continue;
        auto* type=object->klass;
        if(type && type->name && std::strcmp(type->name,name)==0) return object;
    }
    return nullptr;
}
}
