#pragma once
#include "variables/Resolver.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/CardStatQueries.hpp"
namespace CustomComponents::Variables {
inline std::vector<Obj*> Owners(Obj* candidate,Obj* context,Obj* query){
    Obj* model=ContextModel(context);const bool bridged=model!=NativeContextModel(context);Obj* effect=Engine::PlacementQueryContext::active.effect;
    int source=bridged?Source(effect):Field(context,"SelfId");if(source<0 && !bridged)source=ContextId(context,"get_SourceId");if(bridged && source<0)return {};
    if(query){QueryScope scope({model,source,bridged?-1:ContextId(context,"get_TargetId"),bridged?Engine::Match::Id(effect):ContextId(context,"get_EffectEntityId"),nullptr,bridged?nullptr:context});return scope.Find(query);}
    Obj* owner=Find(model,source);return owner?std::vector<Obj*>{owner}:candidate?std::vector<Obj*>{candidate}:std::vector<Obj*>{};
}
inline std::optional<Value> StatOrVariable(Obj* model,Obj* owner,std::string_view name){
    // Keep existing variable names authoritative; stat identifiers are fallback
    // for names not present in the card's variable table.
    if(auto value=Read(model,owner,name))return value;
    auto stat=Engine::CardStatValues::Read(owner,name);
    return stat?std::optional<Value>(Value{Kind::Int,{*stat}}):std::nullopt;
}
template<class Q> inline bool QueryMatch(Q* q,Obj* candidate,Obj* context,int mode){
    Obj* model=ContextModel(context);if(!model || !candidate)return false;auto owners=Owners(candidate,context,q->EntityQuery);if(owners.empty())return false;bool all=Text(q->MatchMode,"Any")=="All";
    for(Obj* owner:owners){bool result=false;
        if(mode==1)result=Read(model,owner,Text(q->Variable)).has_value();
        else if(mode==2){auto v=Read(model,owner,Text(q->Variable));if(v){int n=-1;if(v->kind==Kind::Guid || v->kind==Kind::Guids)n=Engine::BoardPlacement::Guid(candidate);else if(v->kind==Kind::Entity || v->kind==Kind::Entities){int id=Engine::Match::Id(candidate);if(Find(model,id)==candidate)n=id;}result=n>=0 && std::find(v->items.begin(),v->items.end(),n)!=v->items.end();}}
        else {
            auto lookup=[&](auto name){return StatOrVariable(model,owner,name);};
            auto value=StatOrVariable(model,owner,Text(q->Variable));
            auto op=Text(q->ComparisonOperator,"Equal");
            Obj* compareOwner=owner;
            if constexpr(requires(Q* query){query->CompareEntityQuery;}) {
                if(q->CompareEntityQuery){
                    const bool bridged=model!=NativeContextModel(context);
                    Obj* effect=Engine::PlacementQueryContext::active.effect;
                    QueryScope scope({model,bridged?Source(effect):ContextId(context,"get_SourceId"),
                        bridged?-1:ContextId(context,"get_TargetId"),
                        bridged?Engine::Match::Id(effect):ContextId(context,"get_EffectEntityId"),
                        nullptr,bridged?nullptr:context});
                    auto matches=scope.Find(q->CompareEntityQuery);
                    if(matches.size()!=1)return false;
                    compareOwner=matches.front();
                }
            }
            auto rhsLookup=[&](auto name){return StatOrVariable(model,compareOwner,name);};
            auto right=Text(q->ValueExpression).empty()?std::optional<int>(q->Value):Expressions::Evaluate(Text(q->ValueExpression),rhsLookup);
            if(Text(q->Expression).empty() && value && value->kind!=Kind::Int){if(right){if(value->Array() && (op=="Contains" || op=="NotContains")){bool contains=std::find(value->items.begin(),value->items.end(),*right)!=value->items.end();result=op=="Contains"?contains:!contains;}else if(!value->Array() && (op=="Equal" || op=="NotEqual"))result=Compare(value->items[0],*right,op);}}
            else {auto left=Text(q->Expression).empty()?(value?value->Integer():std::nullopt):Expressions::Evaluate(Text(q->Expression),lookup);result=left && right && Compare(*left,*right,op);}}
        if(result && !all)return true;if(!result && all)return false;
    }return all;
}
struct VariableComparisonQuery:Obj {
    Str* Variable=nullptr;
    Str* Expression=nullptr;
    Str* ValueExpression=nullptr;
    Str* ComparisonOperator=nullptr;
    Str* MatchMode=nullptr;
    int Value=0;
    Obj* EntityQuery=nullptr;
    Obj* CompareEntityQuery=nullptr;
    BNM_CustomClass(VariableComparisonQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","VariableComparisonQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Variable,BNM::Defaults::Get<Str*>(),"Variable");
    BNM_CustomField(Expression,BNM::Defaults::Get<Str*>(),"Expression");
    BNM_CustomField(ValueExpression,BNM::Defaults::Get<Str*>(),"ValueExpression");
    BNM_CustomField(ComparisonOperator,BNM::Defaults::Get<Str*>(),"ComparisonOperator");
    BNM_CustomField(MatchMode,BNM::Defaults::Get<Str*>(),"MatchMode");
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(EntityQuery,QueryType(),"EntityQuery");
    BNM_CustomField(CompareEntityQuery,QueryType(),"CompareEntityQuery");
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();ctor[this]();}
    bool Matches(Obj* candidate,Obj* context){return QueryMatch(this,candidate,context,0);}
    void DeepCopyFrom(Obj* source,Obj* instances){auto* from=reinterpret_cast<VariableComparisonQuery*>(source);
        Reference(this,&Variable,from?from->Variable:nullptr);
        Reference(this,&Expression,from?from->Expression:nullptr);
        Reference(this,&ValueExpression,from?from->ValueExpression:nullptr);
        Reference(this,&ComparisonOperator,from?from->ComparisonOperator:nullptr);
        Reference(this,&MatchMode,from?from->MatchMode:nullptr);
        Value=from?from->Value:0;
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("EntityQuery").cast<Obj*>(),from?from->EntityQuery:nullptr,instances);
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("CompareEntityQuery").cast<Obj*>(),from?from->CompareEntityQuery:nullptr,instances);}
    void HandleReturnToPool(Obj* instances){
        Reference(this,&Variable,nullptr);
        Reference(this,&Expression,nullptr);
        Reference(this,&ValueExpression,nullptr);
        Reference(this,&ComparisonOperator,nullptr);
        Reference(this,&MatchMode,nullptr);
        Value=0;
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("EntityQuery").cast<Obj*>(),instances);
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("CompareEntityQuery").cast<Obj*>(),instances);}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct HasVariableQuery:Obj {
    Str* Variable=nullptr;
    Str* Expression=nullptr;
    Str* ValueExpression=nullptr;
    Str* ComparisonOperator=nullptr;
    Str* MatchMode=nullptr;
    int Value=0;
    Obj* EntityQuery=nullptr;
    BNM_CustomClass(HasVariableQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","HasVariableQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Variable,BNM::Defaults::Get<Str*>(),"Variable");
    BNM_CustomField(Expression,BNM::Defaults::Get<Str*>(),"Expression");
    BNM_CustomField(ValueExpression,BNM::Defaults::Get<Str*>(),"ValueExpression");
    BNM_CustomField(ComparisonOperator,BNM::Defaults::Get<Str*>(),"ComparisonOperator");
    BNM_CustomField(MatchMode,BNM::Defaults::Get<Str*>(),"MatchMode");
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(EntityQuery,QueryType(),"EntityQuery");
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();ctor[this]();}
    bool Matches(Obj* candidate,Obj* context){return QueryMatch(this,candidate,context,1);}
    void DeepCopyFrom(Obj* source,Obj* instances){auto* from=reinterpret_cast<HasVariableQuery*>(source);
        Reference(this,&Variable,from?from->Variable:nullptr);
        Reference(this,&Expression,from?from->Expression:nullptr);
        Reference(this,&ValueExpression,from?from->ValueExpression:nullptr);
        Reference(this,&ComparisonOperator,from?from->ComparisonOperator:nullptr);
        Reference(this,&MatchMode,from?from->MatchMode:nullptr);
        Value=from?from->Value:0;
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("EntityQuery").cast<Obj*>(),from?from->EntityQuery:nullptr,instances);}
    void HandleReturnToPool(Obj* instances){
        Reference(this,&Variable,nullptr);
        Reference(this,&Expression,nullptr);
        Reference(this,&ValueExpression,nullptr);
        Reference(this,&ComparisonOperator,nullptr);
        Reference(this,&MatchMode,nullptr);
        Value=0;
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("EntityQuery").cast<Obj*>(),instances);}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct VariableCardQuery:Obj {
    Str* Variable=nullptr;
    Str* Expression=nullptr;
    Str* ValueExpression=nullptr;
    Str* ComparisonOperator=nullptr;
    Str* MatchMode=nullptr;
    int Value=0;
    Obj* EntityQuery=nullptr;
    BNM_CustomClass(VariableCardQuery,BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","VariableCardQuery","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries","InHandQuery","EngineLib.dll").Build(),{});
    BNM_CustomField(Variable,BNM::Defaults::Get<Str*>(),"Variable");
    BNM_CustomField(Expression,BNM::Defaults::Get<Str*>(),"Expression");
    BNM_CustomField(ValueExpression,BNM::Defaults::Get<Str*>(),"ValueExpression");
    BNM_CustomField(ComparisonOperator,BNM::Defaults::Get<Str*>(),"ComparisonOperator");
    BNM_CustomField(MatchMode,BNM::Defaults::Get<Str*>(),"MatchMode");
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(EntityQuery,QueryType(),"EntityQuery");
    void Constructor(){auto ctor=BNM::Class("PvZCards.Engine.Queries","InHandQuery").GetMethod(".ctor",0).cast<void>();ctor[this]();}
    bool Matches(Obj* candidate,Obj* context){return QueryMatch(this,candidate,context,2);}
    void DeepCopyFrom(Obj* source,Obj* instances){auto* from=reinterpret_cast<VariableCardQuery*>(source);
        Reference(this,&Variable,from?from->Variable:nullptr);
        Reference(this,&Expression,from?from->Expression:nullptr);
        Reference(this,&ValueExpression,from?from->ValueExpression:nullptr);
        Reference(this,&ComparisonOperator,from?from->ComparisonOperator:nullptr);
        Reference(this,&MatchMode,from?from->MatchMode:nullptr);
        Value=from?from->Value:0;
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("EntityQuery").cast<Obj*>(),from?from->EntityQuery:nullptr,instances);}
    void HandleReturnToPool(Obj* instances){
        Reference(this,&Variable,nullptr);
        Reference(this,&Expression,nullptr);
        Reference(this,&ValueExpression,nullptr);
        Reference(this,&ComparisonOperator,nullptr);
        Reference(this,&MatchMode,nullptr);
        Value=0;
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("EntityQuery").cast<Obj*>(),instances);}
    BNM_CustomMethod(Constructor,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(Matches,false,BNM::Defaults::Get<bool>(),"Matches",BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQueryContext","EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline bool InitializeQueries(){using K=Bridge::FieldKind;auto base=BNM::Class("PvZCards.Engine.Queries","InHandQuery");
    {auto type=BNM::Class("PvZCards.Engine.Queries","VariableComparisonQuery");if(!Queries::PatchInDeckQueryDispatch(type,base,"VariableComparisonQuery") || !Core::ComponentLifecycle::Patch(type,base) || !Queries::RegisterQuery({"VariableComparisonQuery","PvZCards.Engine.Queries.VariableComparisonQuery","PvZCards.Engine.Queries","VariableComparisonQuery","PvZCards.Engine.Queries","InHandQuery"}) || !Bridge::Register("PvZCards.Engine.Queries","VariableComparisonQuery","PvZCards.Engine.Queries","InHandQuery",{{"Variable",K::String},{"Expression",K::String},{"ValueExpression",K::String},{"ComparisonOperator",K::String},{"MatchMode",K::String},{"Value",K::Int32}},false))return false;}
    {auto type=BNM::Class("PvZCards.Engine.Queries","HasVariableQuery");if(!Queries::PatchInDeckQueryDispatch(type,base,"HasVariableQuery") || !Core::ComponentLifecycle::Patch(type,base) || !Queries::RegisterQuery({"HasVariableQuery","PvZCards.Engine.Queries.HasVariableQuery","PvZCards.Engine.Queries","HasVariableQuery","PvZCards.Engine.Queries","InHandQuery"}) || !Bridge::Register("PvZCards.Engine.Queries","HasVariableQuery","PvZCards.Engine.Queries","InHandQuery",{{"Variable",K::String},{"Expression",K::String},{"ValueExpression",K::String},{"ComparisonOperator",K::String},{"MatchMode",K::String},{"Value",K::Int32}},false))return false;}
    {auto type=BNM::Class("PvZCards.Engine.Queries","VariableCardQuery");if(!Queries::PatchInDeckQueryDispatch(type,base,"VariableCardQuery") || !Core::ComponentLifecycle::Patch(type,base) || !Queries::RegisterQuery({"VariableCardQuery","PvZCards.Engine.Queries.VariableCardQuery","PvZCards.Engine.Queries","VariableCardQuery","PvZCards.Engine.Queries","InHandQuery"}) || !Bridge::Register("PvZCards.Engine.Queries","VariableCardQuery","PvZCards.Engine.Queries","InHandQuery",{{"Variable",K::String},{"Expression",K::String},{"ValueExpression",K::String},{"ComparisonOperator",K::String},{"MatchMode",K::String},{"Value",K::Int32}},false))return false;}
    return Queries::CardStats::Initialize();
}
}
