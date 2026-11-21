#pragma once

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "queries/PlacementQueries.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedRuntime.hpp"

namespace CustomComponents::Queries::LaneTopology {
using Obj=BNM::IL2CPP::Il2CppObject;
using IntList=BNM::Structures::Mono::List<int>;

inline Obj* LaneEntity(Obj* model, Obj* entity) {
    if (!model || !entity) return nullptr;
    if (Engine::EntityAccess::Component(entity,"Lane")) return entity;
    Obj* position=Engine::EntityAccess::Component(entity,"LaneElement");
    if (!position) return nullptr;
    static auto ids=BNM::Class("PvZCards.Engine.Components","LaneElement").GetField("LaneIds").cast<IntList*>();
    if (!ids.IsValid()) return nullptr;
    auto* lanes=ids[position].Get();
    if (!lanes || !lanes->items || lanes->size!=1 || lanes->size>int(lanes->items->capacity)) return nullptr;
    Obj* lane=Engine::EntityAccess::Find(model,lanes->items->m_Items[0]);
    return lane && Engine::EntityAccess::Component(lane,"Lane")?lane:nullptr;
}
inline std::optional<int> Order(Obj* model,Obj* entity) {
    Obj* lane=LaneEntity(model,entity);
    Obj* component=lane?Engine::EntityAccess::Component(lane,"Lane"):nullptr;
    static auto order=BNM::Class("PvZCards.Engine.Components","Lane").GetField("CombatOrderIndex").cast<int>();
    if (!component || !order.IsValid())return {};
    return order[component].Get();
}
inline std::optional<int> Distance(Obj* model,Obj* a,Obj* b) {
    auto left=Order(model,a),right=Order(model,b);
    return left && right?std::optional<int>(std::abs(*left-*right)):std::nullopt;
}
inline int FactionPlayer(Obj* model,std::string_view faction) {
    auto api=Engine::EntityAccess::Api();
    auto* entities=model && api.entities.IsValid()?api.entities[model].Get():nullptr;
    if(!Engine::EntityAccess::Valid(entities))return -1;
    int selected=-1;
    for(int i=0;i<entities->size;++i){
        Obj* entity=entities->items->m_Items[i];
        if(!Engine::EntityAccess::Component(entity,"Player") ||
           !Engine::EntityAccess::Component(entity,faction=="Plants"?"Plants":"Zombies"))continue;
        if(selected>=0)return -1; // fail closed on an ambiguous player
        selected=Engine::Match::Id(entity);
    }
    return selected;
}
inline std::optional<int> Fighters(Obj* model,Obj* entity,std::string_view faction) {
    Obj* lane=LaneEntity(model,entity);
    Obj* component=lane?Engine::EntityAccess::Component(lane,"Lane"):nullptr;
    if(!component)return {};
    if(faction=="All"){
        static auto count=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetNumTotalFighters",0).cast<int>();
        return count.IsValid()?std::optional<int>(count[component]()):std::nullopt;
    }
    if(faction=="SameAsTarget"){
        if(Engine::EntityAccess::Component(entity,"Plants"))faction="Plants";
        else if(Engine::EntityAccess::Component(entity,"Zombies"))faction="Zombies";
        else return {};
    }
    if(faction!="Plants" && faction!="Zombies")return {};
    const int player=FactionPlayer(model,faction);
    static auto count=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetNumFightersForPlayer",1).cast<int>();
    return player>=0 && count.IsValid()?std::optional<int>(count[component](player)):std::nullopt;
}
inline Obj* Origin(Obj* context,std::string_view mode) {
    if(!context)return nullptr;
    const int entityType=mode=="Self"?0:mode=="Source"?1:mode=="Target"?2:-1;
    static auto get=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("GetEntity",{std::string_view("type")}).cast<Obj*>();
    return entityType>=0 && get.IsValid()?get[context](entityType):nullptr;
}
inline void SetText(Obj* object, BNM::Structures::Mono::String** field,BNM::Structures::Mono::String* value){
    if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(object,reinterpret_cast<void**>(field),value);
    else *field=value;
}
inline std::string Text(BNM::Structures::Mono::String* value,const char* fallback){return value?value->str():fallback;}
} // namespace CustomComponents::Queries::LaneTopology

namespace CustomComponents::Queries {
using Str=BNM::Structures::Mono::String;
using Obj=BNM::IL2CPP::Il2CppObject;
using K=Systems::CustomFieldBridgeSystem::FieldKind;

// A fighter qualifies if its lane contains the requested number of fighters.
// For plants specifically: Faction="Plants", Minimum=2.
struct LaneFighterCountQuery:Obj {
    int Minimum=2,Maximum=-1;
    Str* Faction=nullptr;
    BNM_CustomClass(LaneFighterCountQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","LaneFighterCountQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Minimum,BNM::Defaults::Get<int>(),"Minimum");
    BNM_CustomField(Maximum,BNM::Defaults::Get<int>(),"Maximum");
    BNM_CustomField(Faction,BNM::Defaults::Get<Str*>(),"Faction");
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();Minimum=2;Maximum=-1;Faction=nullptr;}
    bool Matches(Obj* candidate,Obj* context){
        if(!candidate || !Engine::EntityAccess::Component(candidate,"Card"))return false;
        const auto n=LaneTopology::Fighters(Engine::BoardPlacement::Model(context),candidate,LaneTopology::Text(Faction,"Plants"));
        return n && *n>=Minimum && (Maximum<0 || *n<=Maximum);
    }
    void DeepCopyFrom(Obj* source,Obj*){
        auto* s=reinterpret_cast<LaneFighterCountQuery*>(source);
        Minimum=s?s->Minimum:2;Maximum=s?s->Maximum:-1;
        LaneTopology::SetText(this,&Faction,s?s->Faction:nullptr);
    }
    void HandleReturnToPool(Obj*){Minimum=2;Maximum=-1;LaneTopology::SetText(this,&Faction,nullptr);}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};

// Targets lane entities, never fighters. MaximumDistance=0 means every lane
// on the selected side; =1 means only the immediately neighboring lane.
struct LanesRelativeToEntityQuery:Obj {
    Str* Direction=nullptr;
    Str* OriginEntityType=nullptr;
    int MaximumDistance=0;
    BNM_CustomClass(LanesRelativeToEntityQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","LanesRelativeToEntityQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Direction,BNM::Defaults::Get<Str*>(),"Direction");
    BNM_CustomField(OriginEntityType,BNM::Defaults::Get<Str*>(),"OriginEntityType");
    BNM_CustomField(MaximumDistance,BNM::Defaults::Get<int>(),"MaximumDistance");
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();}
    bool Matches(Obj* candidate,Obj* context){
        if(!candidate || !Engine::EntityAccess::Component(candidate,"Lane"))return false;
        Obj* model=Engine::BoardPlacement::Model(context);
        const auto mode=LaneTopology::Text(OriginEntityType,"Self");
        Obj* origin=LaneTopology::Origin(context,mode);
        auto from=LaneTopology::Order(model,origin),to=LaneTopology::Order(model,candidate);
        // Surprise-phase contexts can have Source set while Self is absent.
        // This fallback remains source-specific: never substitute Target, which
        // identifies the event's trigger target rather than the card owner.
        if(!from && mode=="Self"){
            origin=LaneTopology::Origin(context,"Source");
            from=LaneTopology::Order(model,origin);
        }
        if(!from){
            static std::atomic<unsigned> laneTraceSamples{0};
            if(laneTraceSamples.fetch_add(1)<28)LOG_WARN("CustomComponents/Queries/RELATIVE_LANE_ORIGIN: mode=%s self=%d source=%d candidate=%d",
                mode.c_str(),int(LaneTopology::Origin(context,"Self")!=nullptr),int(LaneTopology::Origin(context,"Source")!=nullptr),Engine::Match::Id(candidate));
        }
        if(!from || !to || *from==*to || MaximumDistance<0)return false;
        const int delta=*to-*from;
        const std::string direction=LaneTopology::Text(Direction,"Both");
        if(direction=="Left" && delta>=0)return false;
        if(direction=="Right" && delta<=0)return false;
        if(direction!="Left" && direction!="Right" && direction!="Both")return false;
        return !MaximumDistance || std::abs(delta)<=MaximumDistance;
    }
    void DeepCopyFrom(Obj* source,Obj*){
        auto* s=reinterpret_cast<LanesRelativeToEntityQuery*>(source);
        MaximumDistance=s?s->MaximumDistance:0;
        LaneTopology::SetText(this,&Direction,s?s->Direction:nullptr);
        LaneTopology::SetText(this,&OriginEntityType,s?s->OriginEntityType:nullptr);
    }
    void HandleReturnToPool(Obj*){MaximumDistance=0;LaneTopology::SetText(this,&Direction,nullptr);LaneTopology::SetText(this,&OriginEntityType,nullptr);}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};

// Resolve the previous lane of the event's target/source entity, not Self.
// The candidate is a Lane entity; missing context/LastLane fails closed.
template<int ContextEntityType> struct LastLaneOfEventEntityQuery:Obj {
    // Concrete derived types below carry the serialized game-facing names.
    bool MatchesLane(Obj* candidate,Obj* context){
        if(!candidate || !context || !Engine::EntityAccess::Component(candidate,"Lane"))return false;
        static auto get=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("GetEntity",{std::string_view("type")}).cast<Obj*>();
        static auto lastId=BNM::Class("PvZCards.Engine.Components","LastLane").GetField("LaneId").cast<int>();
        if(!get.IsValid() || !lastId.IsValid())return false;
        Obj* origin=get[context](ContextEntityType);
        Obj* last=origin?Engine::EntityAccess::Component(origin,"LastLane"):nullptr;
        if(!last)return false;
        return Engine::Match::Id(candidate)==lastId[last].Get();
    }
};

#define DEFINE_EVENT_LAST_LANE_QUERY(Name, EntityType) \
struct Name:LastLaneOfEventEntityQuery<EntityType> { \
    BNM_CustomClass(Name,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries",#Name,"EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{}); \
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();} \
    bool Matches(Obj* candidate,Obj* context){return MatchesLane(candidate,context);} \
    void DeepCopyFrom(Obj*,Obj*){} \
    void HandleReturnToPool(Obj*){} \
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor"); \
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build()); \
    BNM_CustomMethodSkipTypeMatch(Matches); \
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build()); \
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build()); \
};
DEFINE_EVENT_LAST_LANE_QUERY(LastLaneOfTargetQuery,2)
DEFINE_EVENT_LAST_LANE_QUERY(LastLaneOfSourceQuery,1)
// Unlike LastLaneOfTargetQuery (the event's trigger target), this query
// deliberately reads the current effect's stock PrimaryTargets selection.
// Multiple primary targets are ambiguous for one lane and fail closed.
struct LastLaneOfPrimaryTargetQuery:LastLaneOfEventEntityQuery<2> {
    BNM_CustomClass(LastLaneOfPrimaryTargetQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","LastLaneOfPrimaryTargetQuery","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();}
    bool Matches(Obj* candidate,Obj* context){
        if(!candidate || !context || !Engine::EntityAccess::Component(candidate,"Lane"))return false;
        Obj* model=Engine::BoardPlacement::Model(context);
        static auto effectId=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("get_EffectEntityId",0).cast<int>();
        static auto selected=BNM::Class("PvZCards.Engine.Components","Targets").GetField("TargetEntityIds").cast<BNM::Structures::Mono::List<int>*>();
        static auto lastId=BNM::Class("PvZCards.Engine.Components","LastLane").GetField("LaneId").cast<int>();
        if(!model || !effectId.IsValid() || !selected.IsValid() || !lastId.IsValid())return false;
        Obj* effect=Engine::EntityAccess::Find(model,effectId[context]());
        Obj* primary=Engine::EntityAccess::Component(effect,"PrimaryTargets");
        auto* ids=primary?selected[primary].Get():nullptr;
        if(!ids || !ids->items || ids->size!=1 || ids->size>int(ids->items->capacity))return false;
        Obj* card=Engine::EntityAccess::Find(model,ids->items->m_Items[0]);
        Obj* last=Engine::EntityAccess::Component(card,"LastLane");
        return last && Engine::Match::Id(candidate)==lastId[last].Get();
    }
    void DeepCopyFrom(Obj*,Obj*){}
    void HandleReturnToPool(Obj*){}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
#undef DEFINE_EVENT_LAST_LANE_QUERY

inline bool InitializeLaneQueries(){
    auto base=BNM::Class("PvZCards.Engine.Queries","InHandQuery");
    for(const char* name:{"LaneFighterCountQuery","LanesRelativeToEntityQuery","LastLaneOfTargetQuery","LastLaneOfSourceQuery","LastLaneOfPrimaryTargetQuery"}){
        auto cls=BNM::Class("PvZCards.Engine.Queries",name);
        if(!cls.IsValid() || !PatchInDeckQueryDispatch(cls,base,name) ||
           !PatchDiscardedCopyDispatch(cls,base) || !Core::ComponentLifecycle::Patch(cls,base))return false;
        const char* full=std::string_view(name)=="LaneFighterCountQuery"?"PvZCards.Engine.Queries.LaneFighterCountQuery":std::string_view(name)=="LanesRelativeToEntityQuery"?"PvZCards.Engine.Queries.LanesRelativeToEntityQuery":std::string_view(name)=="LastLaneOfTargetQuery"?"PvZCards.Engine.Queries.LastLaneOfTargetQuery":std::string_view(name)=="LastLaneOfSourceQuery"?"PvZCards.Engine.Queries.LastLaneOfSourceQuery":"PvZCards.Engine.Queries.LastLaneOfPrimaryTargetQuery";
        if(!RegisterQuery({name,full,"PvZCards.Engine.Queries",name,"PvZCards.Engine.Queries","InHandQuery"}))return false;
        const bool count=std::string_view(name)=="LaneFighterCountQuery";
        if(count){
            if(!Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries",name,"PvZCards.Engine.Queries","InHandQuery",{{"Minimum",K::Int32},{"Maximum",K::Int32},{"Faction",K::String}},false))return false;
        }else if(std::string_view(name)=="LanesRelativeToEntityQuery") {
            if(!Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries",name,"PvZCards.Engine.Queries","InHandQuery",{{"Direction",K::String},{"OriginEntityType",K::String},{"MaximumDistance",K::Int32}},false))return false;
        }else if(!Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries",name,"PvZCards.Engine.Queries","InHandQuery",{},false))return false;
    }
    return true;
}
}
