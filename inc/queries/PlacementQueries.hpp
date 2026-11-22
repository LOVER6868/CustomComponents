#pragma once
#include "queries/DiscardedFromQuery.hpp"
#include "engine/BoardPlacement.hpp"
namespace CustomComponents::Queries {
inline Obj* CopyPlacementQuery(Obj* query,Obj* instances) {
    auto copy=BNM::Class("PvZCards.Engine","EntityQueryUtils").GetMethod("DeepCopy",2).cast<Obj*>();return query && copy.IsValid()?copy(query,instances):nullptr;
}
inline void ReturnPlacementQuery(Obj*& query,Obj* instances) {
    if(query && instances){auto method=BNM::Class("PvZCards.Engine","EntityQueryUtils").GetMethod("ReturnToPool",2).cast<Obj*>();if(method.IsValid())method(query,instances);}
    query=nullptr;
}
inline void StorePlacementQuery(Obj* self,Obj** field,Obj* value){if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(self,reinterpret_cast<void**>(field),value);else *field=value;}
struct OpenLaneForCardQuery:Obj {
    Obj* CardQuery=nullptr;bool CardQueryUsesEntities=false;
    void Constructor(){CardQuery=nullptr;CardQueryUsesEntities=false;}
    bool Matches(Obj* lane,Obj* context) {
        if(!lane || !context || !CardQuery)return false;
        Obj* model=Engine::BoardPlacement::Model(context);if(!Engine::EntityAccess::Component(lane,"Lane"))return false;
        const auto preview=Engine::PlacementQueryContext::active;
        if(preview.provisional && preview.card) {
            if(Engine::BoardPlacement::Matches(CardQuery,preview.card,context) && Engine::BoardPlacement::Fits(model,preview.card,lane,context))return true;
            return false;
        }
        if(preview.candidatesQuery) {
            for(Obj* card:Engine::BoardPlacement::Cards(preview.candidatesQuery,context,preview.live)) {
                auto value=preview;value.card=card;value.provisional=true;Engine::PlacementQueryContext::Scope selected(value);
                if(Engine::BoardPlacement::Matches(CardQuery,card,context) && Engine::BoardPlacement::Fits(model,card,lane,context))return true;
            }
            return false;
        }
        for(Obj* card:Engine::BoardPlacement::Cards(CardQuery,context,CardQueryUsesEntities))if(Engine::BoardPlacement::Fits(model,card,lane,context))return true;
        return false;
    }
    void DeepCopyFrom(Obj* source,Obj* instances){ReturnPlacementQuery(CardQuery,instances);auto* from=reinterpret_cast<OpenLaneForCardQuery*>(source);CardQueryUsesEntities=from && from->CardQueryUsesEntities;StorePlacementQuery(this,&CardQuery,from?CopyPlacementQuery(from->CardQuery,instances):nullptr);}
    void HandleReturnToPool(Obj* instances){ReturnPlacementQuery(CardQuery,instances);CardQueryUsesEntities=false;}
    BNM_CustomClass(OpenLaneForCardQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","OpenLaneForCardQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(CardQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQuery","EngineLib.dll").Build(),"CardQuery");
    BNM_CustomField(CardQueryUsesEntities,BNM::Defaults::Get<bool>(),"CardQueryUsesEntities");
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct CanBePlacedQuery:Obj {
    Obj* LaneQuery=nullptr;
    void Constructor(){LaneQuery=nullptr;}
    bool Matches(Obj* card,Obj* context) {
        if(!card || !context)return false;
        Obj* model=Engine::BoardPlacement::Model(context);auto api=Engine::EntityAccess::Api();auto* entities=model && api.entities.IsValid()?api.entities[model].Get():nullptr;
        if(!Engine::EntityAccess::Valid(entities))return false;
        for(int i=0;i<entities->size;++i){Obj* lane=entities->items->m_Items[i];if(Engine::PlacementQueryContext::active.lane>=0 && Engine::Match::Id(lane)!=Engine::PlacementQueryContext::active.lane)continue;if(Engine::EntityAccess::Component(lane,"Lane") && (!LaneQuery || Engine::BoardPlacement::Matches(LaneQuery,lane,context)) && Engine::BoardPlacement::Fits(model,card,lane,context))return true;}
        return false;
    }
    void DeepCopyFrom(Obj* source,Obj* instances){ReturnPlacementQuery(LaneQuery,instances);auto* from=reinterpret_cast<CanBePlacedQuery*>(source);StorePlacementQuery(this,&LaneQuery,from?CopyPlacementQuery(from->LaneQuery,instances):nullptr);}
    void HandleReturnToPool(Obj* instances){ReturnPlacementQuery(LaneQuery,instances);}
    BNM_CustomClass(CanBePlacedQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","CanBePlacedQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(LaneQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQuery","EngineLib.dll").Build(),"LaneQuery");
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline bool InitializePlacementQueries() {
    auto base=BNM::Class("PvZCards.Engine.Queries","InHandQuery");
    for(const char* name:{"OpenLaneForCardQuery","CanBePlacedQuery"}) {
        auto query=BNM::Class("PvZCards.Engine.Queries",name);
        if(!PatchInDeckQueryDispatch(query,base,name) || !PatchDiscardedCopyDispatch(query,base))return false;
        auto method=query.GetMethod("HandleReturnToPool",1);auto stock=base.GetMethod("HandleReturnToPool",1);auto iface=BNM::Class("PvZCards.Engine","Poolable");auto im=iface.GetMethod("HandleReturnToPool",1);auto* klass=query.GetClass();
        if(!klass || !method.IsValid() || !stock.IsValid() || !im.IsValid())return false;
        int slot=-1;for(int i=0;i<klass->interface_offsets_count;++i)if(klass->interfaceOffsets[i].interfaceType==iface.GetClass())slot=klass->interfaceOffsets[i].offset+im.GetInfo()->slot;
        const auto concrete=stock.GetInfo()->slot;if(slot<0 || slot>=klass->vtable_count || concrete>=klass->vtable_count)return false;
        method.GetInfo()->slot=concrete;for(int index:{int(concrete),slot})klass->vtable[index]={method.GetInfo()->methodPointer,method.GetInfo()};
        const std::string full=std::string("PvZCards.Engine.Queries.")+name;
        // Query registry stores stable string literals.
        const char* type=std::string_view(name)=="OpenLaneForCardQuery"?"PvZCards.Engine.Queries.OpenLaneForCardQuery":"PvZCards.Engine.Queries.CanBePlacedQuery";
        if(!RegisterQuery({name,type,"PvZCards.Engine.Queries",name,"PvZCards.Engine.Queries","InHandQuery"}))return false;
    }
    using K=Systems::CustomFieldBridgeSystem::FieldKind;
    return Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries","OpenLaneForCardQuery","PvZCards.Engine.Queries","InHandQuery",{{"CardQueryUsesEntities",K::Bool}},false);
}
}
