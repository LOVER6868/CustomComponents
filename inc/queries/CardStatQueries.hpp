#pragma once
#include "variables/Resolver.hpp"
#include "engine/CardStatValues.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/QueryRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/ManagedQuery.hpp"

namespace CustomComponents::Queries::CardStats {
using Obj=BNM::IL2CPP::Il2CppObject;
using Str=BNM::Structures::Mono::String;
using K=Systems::CustomFieldBridgeSystem::FieldKind;
inline auto QueryType(){return BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQuery","EngineLib.dll").Build();}
inline void Reference(Obj* object,Str** field,Str* value){
    if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(object,reinterpret_cast<void**>(field),value);
    else *field=value;
}
inline std::string Text(Str* s,const char* fallback=""){return s?s->str():fallback;}
inline bool Compare(int left,int right,Str* op){return Variables::Compare(left,right,Text(op,"Equal"));}

// Example: Stat="BaseAttack", OtherStat="BaseHealth", OtherQuery=SelfQuery.
// If OtherStat is omitted, compare against Value (a literal integer).
struct StatComparisonQuery:Obj{
    Str* Stat=nullptr;Str* OtherStat=nullptr;Str* ComparisonOperator=nullptr;
    int Value=0;Obj* OtherQuery=nullptr;
    BNM_CustomClass(StatComparisonQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","StatComparisonQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Stat,BNM::Defaults::Get<Str*>(),"Stat");
    BNM_CustomField(OtherStat,BNM::Defaults::Get<Str*>(),"OtherStat");
    BNM_CustomField(ComparisonOperator,BNM::Defaults::Get<Str*>(),"ComparisonOperator");
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(OtherQuery,QueryType(),"OtherQuery");
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();}
    bool Matches(Obj* candidate,Obj* context){
        auto left=Engine::CardStatValues::Read(candidate,Text(Stat));
        if(!left)return false;
        std::optional<int> right=Value;
        if(OtherStat && !OtherStat->str().empty()){
            Obj* other=candidate;
            if(OtherQuery){
                Variables::QueryScope scope({Variables::ContextModel(context),-1,-1,-1,nullptr,context});
                auto owners=scope.Find(OtherQuery);
                if(owners.size()!=1)return false;
                other=owners.front();
            }
            right=Engine::CardStatValues::Read(other,Text(OtherStat));
        }
        return right && Compare(*left,*right,ComparisonOperator);
    }
    void DeepCopyFrom(Obj* source,Obj* instances){
        auto* s=reinterpret_cast<StatComparisonQuery*>(source);
        Reference(this,&Stat,s?s->Stat:nullptr);Reference(this,&OtherStat,s?s->OtherStat:nullptr);
        Reference(this,&ComparisonOperator,s?s->ComparisonOperator:nullptr);Value=s?s->Value:0;
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("OtherQuery").cast<Obj*>(),s?s->OtherQuery:nullptr,instances);
    }
    void HandleReturnToPool(Obj* instances){
        Reference(this,&Stat,nullptr);Reference(this,&OtherStat,nullptr);Reference(this,&ComparisonOperator,nullptr);Value=0;
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("OtherQuery").cast<Obj*>(),instances);
    }
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};

// Health checks its CALCULATED MAX, not missing HP (CurrentDamage).
struct StatModifierQuery:Obj{
    Str* Stat=nullptr;Str* Mode=nullptr;
    BNM_CustomClass(StatModifierQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","StatModifierQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Stat,BNM::Defaults::Get<Str*>(),"Stat");
    BNM_CustomField(Mode,BNM::Defaults::Get<Str*>(),"Mode");
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();if(ctor.IsValid())ctor[this]();}
    bool Matches(Obj* candidate,Obj*){
        const auto diff=Engine::CardStatValues::Difference(candidate,Text(Stat));
        if(!diff)return false;
        const auto mode=Text(Mode,"Modified");
        return mode=="Buffed"?*diff>0:mode=="Nerfed"?*diff<0:mode=="Modified"?*diff!=0:mode=="Unmodified"?*diff==0:false;
    }
    void DeepCopyFrom(Obj* source,Obj*){auto* s=reinterpret_cast<StatModifierQuery*>(source);Reference(this,&Stat,s?s->Stat:nullptr);Reference(this,&Mode,s?s->Mode:nullptr);}
    void HandleReturnToPool(Obj*){Reference(this,&Stat,nullptr);Reference(this,&Mode,nullptr);}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline bool Initialize(){
    auto base=BNM::Class("PvZCards.Engine.Queries","InHandQuery");
    auto comparison=BNM::Class("PvZCards.Engine.Queries","StatComparisonQuery");
    auto modified=BNM::Class("PvZCards.Engine.Queries","StatModifierQuery");
    return PatchInDeckQueryDispatch(comparison,base,"StatComparisonQuery") && Core::ComponentLifecycle::Patch(comparison,base) &&
        RegisterQuery({"StatComparisonQuery","PvZCards.Engine.Queries.StatComparisonQuery","PvZCards.Engine.Queries","StatComparisonQuery","PvZCards.Engine.Queries","InHandQuery"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries","StatComparisonQuery","PvZCards.Engine.Queries","InHandQuery",{{"Stat",K::String},{"OtherStat",K::String},{"ComparisonOperator",K::String},{"Value",K::Int32}},false) &&
        PatchInDeckQueryDispatch(modified,base,"StatModifierQuery") && Core::ComponentLifecycle::Patch(modified,base) &&
        RegisterQuery({"StatModifierQuery","PvZCards.Engine.Queries.StatModifierQuery","PvZCards.Engine.Queries","StatModifierQuery","PvZCards.Engine.Queries","InHandQuery"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries","StatModifierQuery","PvZCards.Engine.Queries","InHandQuery",{{"Stat",K::String},{"Mode",K::String}},false);
}
}
