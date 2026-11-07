#pragma once
#include "queries/InDeckQuery.hpp"
#include "queries/ListPositionPolicy.hpp"
#include "core/ComponentLifecycle.hpp"
#include "engine/EntityAccess.hpp"
#include "level/LevelQueryState.hpp"
namespace CustomComponents::Queries {
struct LeveledQuery:Obj {
    int Value=0;BNM::Structures::Mono::String* ComparisonOperator=nullptr;
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();Value=0;ComparisonOperator=nullptr;}
    BNM_CustomClass(LeveledQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","LeveledQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(ComparisonOperator,BNM::Defaults::Get<BNM::Structures::Mono::String*>(),"ComparisonOperator");
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    bool Matches(Obj* entity,Obj*) {
        auto preview=Level::QueryState::previewValue?Level::QueryState::previewValue(entity):std::nullopt;
        int actual=0;
        if(preview){actual=*preview;if(actual<=0)return false;}
        else {Obj* marker=Engine::EntityAccess::Component(entity,"Leveled");if(!marker)return false;auto value=BNM::Class(marker).GetField("Value").cast<int>();if(!value.IsValid())return false;actual=value[marker].Get();}
        const auto op=ComparisonOperator?ComparisonOperator->str():"Equal";
        if(op=="Equal")return actual==Value;if(op=="NotEqual")return actual!=Value;if(op=="Greater")return actual>Value;if(op=="GreaterOrEqual")return actual>=Value;if(op=="Less")return actual<Value;if(op=="LessOrEqual")return actual<=Value;return false;
    }
    void DeepCopyFrom(Obj* source,Obj*){auto* s=reinterpret_cast<LeveledQuery*>(source);Value=s?s->Value:0;auto* text=s?s->ComparisonOperator:nullptr;if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&ComparisonOperator),text);else ComparisonOperator=text;}
    void HandleReturnToPool(Obj* instances){DeepCopyFrom(nullptr,instances);}
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline bool InitializeLeveledQuery(){auto type=BNM::Class("PvZCards.Engine.Queries","LeveledQuery");auto base=BNM::Class("PvZCards.Engine.Queries","InHandQuery");using K=Systems::CustomFieldBridgeSystem::FieldKind;return PatchInDeckQueryDispatch(type,base,"LeveledQuery") && Core::ComponentLifecycle::Patch(type,base) && Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries","LeveledQuery","PvZCards.Engine.Queries","InHandQuery",{{"Value",K::Int32},{"ComparisonOperator",K::String}},false) && RegisterQuery({"LeveledQuery","PvZCards.Engine.Queries.LeveledQuery","PvZCards.Engine.Queries","LeveledQuery","PvZCards.Engine.Queries","InHandQuery"});}
}
