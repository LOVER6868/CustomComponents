#pragma once
#include "queries/LaneQueries.hpp"
#include "engine/MatchAccess.hpp"

namespace CustomComponents::Queries {
// Actual damage recorded since the latest TurnStartRecord, regardless of
// current health; healing does not erase the record. No combat classification.
struct WasDamagedThisTurnQuery:Obj {
    BNM_CustomClass(WasDamagedThisTurnQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","WasDamagedThisTurnQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();}
    bool Matches(Obj* candidate,Obj*){
        if(!candidate)return false;
        Obj* history=Engine::Match::Provide(Engine::Match::Instances(candidate),BNM::Class("PvZCards.Engine","History").GetMonoType());
        if(!history)return false;
        auto recordsMethod=BNM::Class("PvZCards.Engine","History").GetMethod("GetRecords",0).cast<Obj*>();
        Obj* records=recordsMethod.IsValid()?recordsMethod[history]():nullptr;
        if(!records)return false;
        auto count=BNM::Class(records).GetMethod("get_Count",0).cast<int>();
        auto item=BNM::Class(records).GetMethod("get_Item",1).cast<Obj*>();
        auto target=BNM::Class("PvZCards.Engine.Records","EffectRecord").GetMethod("get_TargetId",0).cast<int>();
        auto amount=BNM::Class("PvZCards.Engine.Records","DamageRecord").GetMethod("get_DamageAmount",0).cast<int>();
        if(!count.IsValid() || !item.IsValid() || !target.IsValid() || !amount.IsValid())return false;
        const int id=Engine::Match::Id(candidate);
        for(int i=count[records]()-1;i>=0;--i){
            Obj* record=item[records](i);
            if(!record || !record->klass || !record->klass->name)continue;
            const std::string_view name=record->klass->name;
            if(name=="TurnStartRecord")break;
            if(name=="DamageRecord" && target[record]()==id && amount[record]()>0)return true;
        }
        return false;
    }
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
};
inline bool InitializeWasDamagedThisTurnQuery(){
    const auto cls=BNM::Class("PvZCards.Engine.Queries","WasDamagedThisTurnQuery");
    const auto base=BNM::Class("PvZCards.Engine.Queries","InHandQuery");
    return PatchInDeckQueryDispatch(cls,base,"WasDamagedThisTurnQuery") &&
        RegisterQuery({"WasDamagedThisTurnQuery","PvZCards.Engine.Queries.WasDamagedThisTurnQuery","PvZCards.Engine.Queries","WasDamagedThisTurnQuery","PvZCards.Engine.Queries","InHandQuery"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries","WasDamagedThisTurnQuery","PvZCards.Engine.Queries","InHandQuery",{},false);
}
}
