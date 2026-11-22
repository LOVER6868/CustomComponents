#pragma once
#include "core/SceneViewContext.hpp"
#include "diagnostics/HistoryTrace.hpp"
#include "variables/Resolver.hpp"
#include "variables/QueuedTriggerValues.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/CardStatQueries.hpp"
namespace CustomComponents::Variables {
inline std::vector<Obj*> Owners(Obj* candidate,Obj* context,Obj* query){
    Obj* model=ContextModel(context);const bool bridged=model!=NativeContextModel(context);Obj* effect=Engine::PlacementQueryContext::active.effect;
    int source=bridged?Source(effect):Field(context,"SelfId");if(source<0 && !bridged)source=ContextId(context,"get_SourceId");if(bridged && source<0)return {};
    if(query && query->klass && query->klass->name && std::string_view(query->klass->name)=="SelfQuery"){
        Obj* owner=Find(model,source);
        return owner?std::vector<Obj*>{owner}:std::vector<Obj*>{};
    }
    if(query){QueryScope scope({model,source,bridged?-1:ContextId(context,"get_TargetId"),bridged?Engine::Match::Id(effect):ContextId(context,"get_EffectEntityId"),nullptr,bridged?nullptr:context});return scope.Find(query);}
    Obj* owner=Find(model,source);return owner?std::vector<Obj*>{owner}:candidate?std::vector<Obj*>{candidate}:std::vector<Obj*>{};
}
inline std::optional<Value> StatOrVariable(Obj* model,Obj* owner,std::string_view name){
    // All trigger markers read their own event-time variable snapshot.
    // Outside TriggeredEffectResolver, the live model remains authoritative.
    if(auto frozen=QueuedTriggerValues::Frozen(model,owner,name))return *frozen;
    // Keep existing variable names authoritative; stat identifiers are fallback
    // for names not present in the card's variable table.
    if(auto value=Read(model,owner,name))return value;
    auto stat=Engine::CardStatValues::Read(owner,name);
    return stat?std::optional<Value>(Value{Kind::Int,{*stat}}):std::nullopt;
}
enum class QueryKind : int {Comparison=0,Exists=1,ContainsCard=2};
enum class MatchMode : int {Any=0,All=1};
inline constexpr MatchMode ParseMatchMode(std::string_view name){return name=="All"?MatchMode::All:MatchMode::Any;}
template<class Q> inline bool QueryMatch(Q* q,Obj* candidate,Obj* context,QueryKind mode){
    Core::Performance::Scope queryCost(Core::Performance::Area::VariableQuery,
        q && q->klass?q->klass->name:"variable-query");
    Obj* model=ContextModel(context);if(!model || !candidate)return false;auto owners=Owners(candidate,context,q->EntityQuery);
    if(mode==QueryKind::Comparison && reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity()){
        static std::atomic<unsigned> samples{0};
        if(samples.fetch_add(1)<80)LOG_INFO("CustomComponents/Variables/QUERY_ENTER: candidate=%d self=%d owners=%zu variable=%s expression=%s value=%d",
            Engine::Match::Id(candidate),Field(context,"SelfId"),owners.size(),Text(q->Variable).c_str(),Text(q->Expression).c_str(),q->Value);
    }
    if(owners.empty())return false;const bool all=ParseMatchMode(Text(q->MatchMode,"Any"))==MatchMode::All;
    for(Obj* owner:owners){bool result=false;std::optional<int> left,right;
        auto read=[&](auto name)->std::optional<Value>{if(auto frozen=QueuedTriggerValues::Frozen(model,owner,name))return *frozen;return StatOrVariable(model,owner,name);};
        if(mode==QueryKind::Exists)result=read(Text(q->Variable)).has_value();
        else if(mode==QueryKind::ContainsCard){auto v=read(Text(q->Variable));if(v){int n=-1;if(v->kind==Kind::Guid || v->kind==Kind::Guids)n=Engine::BoardPlacement::Guid(candidate);else if(v->kind==Kind::Entity || v->kind==Kind::Entities){int id=Engine::Match::Id(candidate);if(Find(model,id)==candidate)n=id;}result=n>=0 && std::find(v->items.begin(),v->items.end(),n)!=v->items.end();}}
        else {
            // A triggered reward must read the value from *its own* play,
            // even if a later queued PlayCardEffect already advanced the live
            // timed counter. Stock may validate the same trigger more than once.
            auto lookup=[&](auto name)->std::optional<Value>{
                if(auto frozen=QueuedTriggerValues::Frozen(model,owner,name))return *frozen;
                return StatOrVariable(model,owner,name);
            };
            auto value=lookup(Text(q->Variable));
            const auto op=Core::Comparisons::Parse(Text(q->ComparisonOperator,"Equal"));
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
            auto rhsLookup=[&](auto name)->std::optional<Value>{if(auto frozen=QueuedTriggerValues::Frozen(model,compareOwner,name))return *frozen;return StatOrVariable(model,compareOwner,name);};
            right=Text(q->ValueExpression).empty()?std::optional<int>(q->Value):Expressions::Evaluate(Text(q->ValueExpression),rhsLookup);
            if(Text(q->Expression).empty() && value && value->kind!=Kind::Int){if(right){if(value->Array() && (op==Core::Comparisons::Operator::Contains || op==Core::Comparisons::Operator::NotContains)){bool contains=std::find(value->items.begin(),value->items.end(),*right)!=value->items.end();result=op==Core::Comparisons::Operator::Contains?contains:!contains;}else if(!value->Array() && (op==Core::Comparisons::Operator::Equal || op==Core::Comparisons::Operator::NotEqual))result=Compare(value->items[0],*right,op);}}
            else {left=Text(q->Expression).empty()?(value?value->Integer():std::nullopt):Expressions::Evaluate(Text(q->Expression),lookup);result=left && right && Compare(*left,*right,op);}}
        if(mode==QueryKind::Comparison && reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity()){
            static std::atomic<unsigned> samples{0};
            if(Diagnostics::HistoryTrace::enabled?Diagnostics::HistoryTrace::Allow():samples.fetch_add(1)<60){
                const int source=ContextId(context,"get_SourceId"),target=ContextId(context,"get_TargetId");
                LOG_INFO("CustomComponents/Variables/COMPARISON_TRACE: owner=%d source=%d target=%d variable=%s expression=%s snapshot=%llu condition=%d result=%d leftDefined=%d left=%d rightDefined=%d right=%d",
                    Engine::Match::Id(owner),source,target,Text(q->Variable).c_str(),Text(q->Expression).c_str(),
                    QueuedTriggerValues::active?QueuedTriggerValues::active->sequence:0,q->Value,int(result),int(left.has_value()),left.value_or(0),int(right.has_value()),right.value_or(0));
            }
        }
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
    bool Matches(Obj* candidate,Obj* context){return QueryMatch(this,candidate,context,QueryKind::Comparison);}
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
    bool Matches(Obj* candidate,Obj* context){return QueryMatch(this,candidate,context,QueryKind::Exists);}
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
    bool Matches(Obj* candidate,Obj* context){return QueryMatch(this,candidate,context,QueryKind::ContainsCard);}
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
