#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "queries/InDeckQuery.hpp"
#include "queries/InDiscardPileQuery.hpp"
#include "queries/DiscardedFromQuery.hpp"
#include "queries/ListPositionPolicy.hpp"
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Queries {
struct ListPositionQuery:Obj {
    BNM::Structures::Mono::String* Pool=nullptr;
    int Position=0;
    bool FromEnd=false;
    BNM::Structures::Mono::String* ComparisonOperator=nullptr;
    void Constructor() {Pool=nullptr;Position=0;FromEnd=false;ComparisonOperator=nullptr;}
    bool Matches(Obj* candidate,Obj* context) {
        if(!candidate || !context)return false;
        auto idField=inDeckEntityId;const int id=idField[candidate].Get();if(id<0)return false;
        auto modelField=inDeckContextModel;Obj* model=modelField[context].Get();if(!model)return false;
        auto entitiesField=inDeckModelEntities;auto* entities=entitiesField[model].Get();
        if(!entities || !entities->items || entities->size<0 || entities->size>int(entities->items->capacity))return false;
        const auto pool=ListPosition::ParsePool(Pool?Pool->str():"Deck");
        const std::string op=ComparisonOperator?ComparisonOperator->str():"Equal";
        auto componentsField=inDeckEntityComponents;
        for(int e=0;e<entities->size;++e) {
            Obj* owner=InDeckListAt(entities,e);if(!owner)continue;
            auto* components=componentsField[owner].Get();
            if(!components || !components->items || components->size<0 || components->size>int(components->items->capacity))continue;
            if(pool==ListPosition::Pool::Deck && !InDeckIsRegularPool(components))continue;
            for(int c=0;c<components->size;++c) {
                Obj* component=InDeckListAt(components,c);if(!component)continue;
                if(pool==ListPosition::Pool::Deck && component->klass==inDeckOrderedPoolClass) {
                    auto field=inDeckOrderedPoolEntries;auto* entries=field[component].Get();
                    if(!entries || !entries->items || entries->size<0 || entries->size>int(entries->items->capacity))continue;
                    auto entryId=inDeckPoolEntryEntityId;
                    for(int i=0;i<entries->size;++i) {
                        Obj* entry=InDeckListAt(entries,i);
                        if(entry && entryId[entry].Get()==id)return ListPosition::Compare(i,entries->size,Position,FromEnd,op);
                    }
                } else if(pool==ListPosition::Pool::DiscardPile && component->klass==inDiscardPileClass) {
                    auto field=inDiscardPileCardIds;auto* ids=field[component].Get();
                    if(!ids || !ids->items || ids->size<0 || ids->size>int(ids->items->capacity))continue;
                    for(int i=0;i<ids->size;++i)if(ids->items->m_Items[i]==id)
                        return ListPosition::Compare(i,ids->size,Position,FromEnd,op);
                }
            }
        }
        return false;
    }
    void DeepCopyFrom(Obj* source,Obj*) {
        auto* value=reinterpret_cast<ListPositionQuery*>(source);
        Position=value?value->Position:0;FromEnd=value && value->FromEnd;
        auto write=[&](auto** slot,auto* text) {
            if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(slot),text);
            else *slot=text;
        };
        write(&Pool,value?value->Pool:nullptr);write(&ComparisonOperator,value?value->ComparisonOperator:nullptr);
    }
    void HandleReturnToPool(Obj* instances){DeepCopyFrom(nullptr,instances);}
    BNM_CustomClass(ListPositionQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","ListPositionQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Pool,BNM::Defaults::Get<BNM::Structures::Mono::String*>(),"Pool");
    BNM_CustomField(Position,BNM::Defaults::Get<int>(),"Position");
    BNM_CustomField(FromEnd,BNM::Defaults::Get<bool>(),"FromEnd");
    BNM_CustomField(ComparisonOperator,BNM::Defaults::Get<BNM::Structures::Mono::String*>(),"ComparisonOperator");
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline bool InitializeListPositionQuery() {
    auto query=BNM::Class("PvZCards.Engine.Queries","ListPositionQuery");auto base=BNM::Class("PvZCards.Engine.Queries","InHandQuery");
    if(!PatchInDeckQueryDispatch(query,base,"ListPositionQuery") || !PatchDiscardedCopyDispatch(query,base))return false;
    // BNM's override metadata also needs the inherited Poolable interface slot.
    auto method=query.GetMethod("HandleReturnToPool",1);auto stock=base.GetMethod("HandleReturnToPool",1);
    auto iface=BNM::Class("PvZCards.Engine","Poolable");auto im=iface.GetMethod("HandleReturnToPool",1);
    auto* klass=query.GetClass();
    if(!klass || !method.IsValid() || !stock.IsValid() || !im.IsValid())return false;
    int slot=-1;for(int i=0;i<klass->interface_offsets_count;++i)if(klass->interfaceOffsets[i].interfaceType==iface.GetClass())slot=klass->interfaceOffsets[i].offset+im.GetInfo()->slot;
    const auto concrete=stock.GetInfo()->slot;
    if(slot<0 || slot>=klass->vtable_count || concrete>=klass->vtable_count)return false;
    method.GetInfo()->slot=concrete;
    for(int index:{int(concrete),slot})klass->vtable[index]={method.GetInfo()->methodPointer,method.GetInfo()};
    using K=Systems::CustomFieldBridgeSystem::FieldKind;
    return Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries","ListPositionQuery","PvZCards.Engine.Queries","InHandQuery",{{"Pool",K::String},{"Position",K::Int32},{"FromEnd",K::Bool},{"ComparisonOperator",K::String}},false) &&
        RegisterQuery({"ListPositionQuery","PvZCards.Engine.Queries.ListPositionQuery","PvZCards.Engine.Queries","ListPositionQuery","PvZCards.Engine.Queries","InHandQuery"});
}
}
