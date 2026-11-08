#pragma once
#include "variables/Components.hpp"
#include "variables/Expressions.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EffectGuards.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/InDiscardPileQuery.hpp"
namespace CustomComponents::Variables {
struct Context {Obj* model=nullptr;int source=-1,target=-1,effect=-1;Obj* runtime=nullptr;Obj* parentContext=nullptr;};
inline int Field(Obj* o,const char* name){if(!o)return -1;auto field=BNM::Class(o).GetField(name).cast<int>();return field.IsValid()?field[o].Get():-1;}
inline Obj* NativeContextModel(Obj* c){static auto metadata=BNM::Class("PvZCards.Engine","EntityQueryContext").GetField("<Model>k__BackingField").cast<Obj*>();auto field=metadata;return c?field[c].Get():nullptr;}
inline Obj* ContextModel(Obj* c){return Engine::BoardPlacement::Model(c);}
inline int ContextId(Obj* c,const char* name){if(!c)return -1;auto get=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod(name,0).cast<int>();return get.IsValid()?get[c]():-1;}
struct QueryScope {
    Core::ManagedRuntime::Root finder;Obj* context=nullptr;Obj* instances=nullptr;bool borrowed=false;
    explicit QueryScope(Context c):finder(BNM::Class("PvZCards.Engine","EntityFinder").CreateNewObjectParameters(c.model)){
        if(!c.model || !finder.Get())return;if(c.parentContext){context=c.parentContext;return;}auto type=BNM::Class("PvZCards.Engine","EntityFinder");context=type.GetField("QueryContext").cast<Obj*>()[finder.Get()].Get();if(!context)return;
        if(c.runtime){instances=Engine::Match::Instances(Engine::EntityAccess::Find(c.model,c.source));if(instances){auto native=BNM::Class("PvZCards.Engine","Effect").GetMethod("ToEntityQueryContext",1).cast<Obj*>();if(native.IsValid()){Obj* value=native[c.runtime](instances);if(value){context=value;borrowed=true;}}}}
        auto q=BNM::Class("PvZCards.Engine","EntityQueryContext");q.GetField("<Model>k__BackingField").cast<Obj*>()[context].Set(c.model);q.GetField("SelfId").cast<int>()[context].Set(c.source);auto source=q.GetMethod("set_SourceId",1).cast<void>();auto target=q.GetMethod("set_TargetId",1).cast<void>();auto effect=q.GetMethod("set_EffectEntityId",1).cast<void>();if(!borrowed)source[context](c.source);target[context](c.target);effect[context](c.effect);
    }
    ~QueryScope(){if(borrowed && context)Core::ManagedPool::Return(instances,context);}
    std::vector<Obj*> Find(Obj* query,bool database=false){
        if(!query || !context)return {};Engine::EntityAccess::List* list=nullptr;
        // The one-argument database finder discards the caller's live context.
        // Use its contextual overload for picked GUIDs and source variables.
        auto type=BNM::Class(database?"PvZCards.Core.Utilities":"PvZCards.Engine",database?"CardDatabase":"EntityFinder");
        auto find=type.GetMethod("FindEntities",{BNM::Class("PvZCards.Engine","EntityQuery").GetCompileTimeClass(),BNM::Class("PvZCards.Engine","EntityQueryContext").GetCompileTimeClass()}).cast<Engine::EntityAccess::List*>();
        Obj* owner=database?Engine::BoardPlacement::Database():finder.Get();if(!owner || !find.IsValid())return {};list=find[owner](query,context);
        Core::ManagedRuntime::Root result(reinterpret_cast<Obj*>(list));if(!Engine::EntityAccess::Valid(list) || list->size>20000)return {};return {list->items->m_Items,list->items->m_Items+list->size};
    }
};
inline Obj* Find(Obj* model,int id){return Engine::EntityAccess::Find(model,id);}
inline int Source(Obj* effect){Obj* c=Engine::EntityAccess::Component(effect,"EffectEntity");return Field(c,"SourceId");}
inline bool BindingTargets(Obj* primary){return primary && Field(primary,"SelectionType")==0 && Field(primary,"NumTargets")==0 && Field(primary,"TargetScopeType")==0 && Field(primary,"AdditionalTargetType")==0;}
inline bool ZoneActive(Context c,Obj* source,std::string_view zones){
    if(zones.empty())zones="Hand,Board";
    if((zones.find("Hand")!=zones.npos && Engine::EntityAccess::Component(source,"HandElement")) || (zones.find("Board")!=zones.npos && Engine::EntityAccess::Component(source,"LaneElement")))return true;
    QueryScope scope(c);if(!scope.context)return false;
    if(zones.find("Deck")!=zones.npos){Queries::InDeckQuery query;if(query.Matches(source,scope.context))return true;}
    if(zones.find("Discard")!=zones.npos){Queries::InDiscardPileQuery query;if(query.Matches(source,scope.context))return true;}return false;
}
struct Index {int version=-1;Obj* listIdentity=nullptr;std::vector<int> setters,grants,cards;std::uint64_t revision=~std::uint64_t{};std::map<std::pair<int,std::string>,std::optional<Value>> values;std::optional<std::map<int,std::vector<int>>> desired;std::vector<int> grantedTargets;bool grantsInspected=false;};
struct IndexedModel {std::shared_ptr<Core::ManagedRuntime::Root> model;Index index;};
inline thread_local std::vector<IndexedModel> indexes;
inline Index& ModelIndex(Obj* model){
    indexes.erase(std::remove_if(indexes.begin(),indexes.end(),[](auto& e){return !e.model->Get();}),indexes.end());
    auto it=std::find_if(indexes.begin(),indexes.end(),[&](auto& e){return e.model->Get()==model;});if(it==indexes.end()){indexes.push_back({std::make_shared<Core::ManagedRuntime::Root>(model,true),{}});it=std::prev(indexes.end());}
    auto api=Engine::EntityAccess::Api();auto* list=model?api.entities[model].Get():nullptr;auto& index=it->index;
    if(Engine::EntityAccess::Valid(list) && (index.listIdentity!=reinterpret_cast<Obj*>(list) || index.version!=list->version)){
        index.setters.clear();index.grants.clear();index.cards.clear();
        for(int i=0;i<list->size;++i){Obj* e=list->items->m_Items[i];if(!e)continue;int id=api.id[e].Get();if(Engine::EntityAccess::Component(e,"Card"))index.cards.push_back(id);if(!Engine::EntityAccess::Component(e,"Continuous"))continue;if(Engine::EntityAccess::Component(e,"SetVariableEffectDescriptor"))index.setters.push_back(id);if(Engine::EntityAccess::Component(e,"GrantAbilityEffectDescriptor"))index.grants.push_back(id);}
        std::sort(index.setters.begin(),index.setters.end());std::sort(index.grants.begin(),index.grants.end());index.version=list->version;index.listIdentity=reinterpret_cast<Obj*>(list);index.values.clear();index.desired.reset();
    }
    auto revision=Engine::SimulationChanges::Revision(model);if(index.revision!=revision){index.revision=revision;index.values.clear();index.desired.reset();}return index;
}
inline bool Conditions(Context c,Obj* effect,Obj* owner,std::string_view zones){
    Obj* source=Find(c.model,c.source);if(!source || !ZoneActive(c,source,zones))return false;
    QueryScope scope(c);if(!scope.context)return false;
    Obj* filter=Engine::EntityAccess::Component(effect,"PrimaryTargetFilter");auto queryField=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();
    if(filter){Obj* query=queryField[filter].Get();if(!query || !Engine::BoardPlacement::Matches(query,owner,scope.context))return false;}
    // The stock validator evaluates all Condition implementations without firing
    // a trigger. Reuse it so player/lane/query/once conditions retain their
    // native meaning instead of maintaining a second, partial condition engine.
    auto validator=Engine::Match::Service(Engine::Match::Instances(source),BNM::Class("PvZCards.Core.Systems.Utilities","TriggerValidator"));
    if(!validator || !validator->Get())return false;
    auto check=BNM::Class("PvZCards.Core.Systems.Utilities","TriggerValidator").GetMethod("ShouldTriggerEffect",2).cast<bool>();
    bool accepted=false;auto error=BNM::TryInvoke([&]{accepted=check.IsValid() && check[validator->Get()](effect,scope.context);});return !error.IsValid() && accepted;
}
template<class Spec> inline std::vector<Obj*> Recipients(const Spec& spec,Context c){if(spec.OwnerQuery){QueryScope scope(c);return scope.Find(spec.OwnerQuery);}Obj* owner=Find(c.model,Text(spec.Owner,"Target")=="Self"?c.source:c.target);return owner?std::vector<Obj*>{owner}:std::vector<Obj*>{};}
inline std::optional<Value> Read(Obj*,Obj*,std::string_view);
inline std::optional<int>(*continuousMultiplier)(Context,Obj*)=nullptr;
template<class Spec> inline std::optional<Value> Resolve(const Spec& spec,Context c,Obj* expressionOwner){
    auto kind=ParseKind(Text(spec.ValueType,"Int"));if(!kind)return {};auto mode=Text(spec.ValueSource,"Literal");
    if(mode=="ArrayElement" || mode=="ArrayLength"){
        auto v=Read(c.model,expressionOwner,Text(spec.Expression));if(!v || !v->Array())return {};
        if(mode=="ArrayLength")return *kind==Kind::Int?std::optional<Value>(Value{Kind::Int,{int(v->items.size())}}):std::nullopt;
        if(int(*kind)>=3 || int(*kind)!=int(v->kind)-3 || spec.Value<0 || spec.Value>=int(v->items.size()))return {};return Value{*kind,{v->items[spec.Value]}};
    }
    if(mode=="Variable"){auto value=Read(c.model,expressionOwner,Text(spec.Expression));return value && value->kind==*kind?value:std::nullopt;}
    if(mode=="Query" || mode=="DatabaseQuery" || mode=="QueryCount"){
        if(mode!="QueryCount" && (*kind==Kind::Int || *kind==Kind::Ints || (mode=="DatabaseQuery" && (*kind==Kind::Entity || *kind==Kind::Entities))))return {};
        QueryScope scope(c);auto matches=scope.Find(spec.Query,mode=="DatabaseQuery");if(mode=="QueryCount")return *kind==Kind::Int?std::optional<Value>(Value{Kind::Int,{int(matches.size())}}):std::nullopt;
        Value value{*kind,{}};for(Obj* e:matches){int n=(*kind==Kind::Guid || *kind==Kind::Guids)?Engine::BoardPlacement::Guid(e):Engine::Match::Id(e);if(n<0)return {};value.items.push_back(n);}return value.Valid()?std::optional<Value>(value):std::nullopt;
    }
    if(mode=="Attack" || mode=="Health" || mode=="Cost"){
        if(*kind!=Kind::Int || !expressionOwner)return {};Obj* stat=Engine::EntityAccess::Component(expressionOwner,mode=="Cost"?"SunCost":mode.c_str());if(!stat)return {};
        if(mode=="Attack" || mode=="Health"){auto method=BNM::Class(stat).GetMethod(mode=="Attack"?"CalculateAttack":"CalculateHealth",0).template cast<int>();return method.IsValid()?std::optional<Value>(Value{Kind::Int,{method[stat]()}}):std::nullopt;}
        if(Engine::EntityAccess::Component(expressionOwner,"IsFree"))return Value{Kind::Int,{0}};auto field=BNM::Class(stat).GetField("SunCostValue").cast<Obj*>();auto calculate=BNM::Class("PvZCards.Engine.Utilities","ModifiableIntCalculator").GetMethod("CalculateValue",1).cast<int>();return Value{Kind::Int,{std::max(0,calculate(field[stat].Get()))}};
    }
    if(mode=="Self" || mode=="Target" || mode=="PrimaryGuid"){
        if(*kind==Kind::Int || *kind==Kind::Ints || (mode=="PrimaryGuid" && *kind!=Kind::Guid && *kind!=Kind::Guids))return {};
        Obj* e=Find(c.model,mode=="Self"?c.source:c.target);int n=(*kind==Kind::Guid || *kind==Kind::Guids)?Engine::BoardPlacement::Guid(e):Engine::Match::Id(e);if(n<0)return {};Value v{*kind,{n}};return v.Valid()?std::optional<Value>(v):std::nullopt;
    }
    if(mode!="Literal" || *kind!=Kind::Int){if(mode=="Literal" && *kind!=Kind::Int && int(*kind)<3){Value v{*kind,{spec.Value}};if(v.Valid())return v;}return {};}
    if(spec.Expression && !spec.Expression->str().empty()){auto n=Expressions::Evaluate(spec.Expression->str(),[&](auto name){return Read(c.model,expressionOwner,name);});return n?std::optional<Value>(Value{Kind::Int,{*n}}):std::nullopt;}
    return Value{Kind::Int,{spec.Value}};
}
struct Reading {Obj* model;int owner;std::string name;};
inline thread_local std::vector<Reading> reading;
inline thread_local unsigned readWork=0;
inline std::optional<Value> Read(Obj* model,Obj* owner,std::string_view name){
    if(reading.empty())readWork=0;if(!model || !owner || !Name(name) || reading.size()>=32 || ++readWork>4096)return {};int id=Engine::Match::Id(owner);
    if(std::any_of(reading.begin(),reading.end(),[&](auto& r){return r.model==model && r.owner==id && r.name==name;}))return {};
    auto key=std::make_pair(id,std::string(name));{auto& index=ModelIndex(model);if(auto found=index.values.find(key);!Engine::SimulationChanges::depth && found!=index.values.end())return found->second;}
    reading.push_back({model,id,std::string(name)});struct Pop{~Pop(){reading.pop_back();}}pop;
    auto stored=Stored(owner);if(!stored)return {};Table table=*stored;auto setters=ModelIndex(model).setters;
    for(int effectId:setters){
        Obj* e=Find(model,effectId);auto* spec=reinterpret_cast<Descriptor*>(Engine::EntityAccess::Component(e,"SetVariableEffectDescriptor"));if(!spec || Text(spec->Variable)!=name)continue;
        Context c{model,Source(e),-1,effectId};Obj* primary=Engine::EntityAccess::Component(e,"PrimaryTargetFilter");if(!BindingTargets(primary))continue;
        auto query=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();QueryScope selection(c);
        for(Obj* target:selection.Find(query[primary].Get())){c.target=Engine::Match::Id(target);auto owners=Recipients(*spec,c);if(std::find(owners.begin(),owners.end(),owner)==owners.end() || !Conditions(c,e,target,Text(spec->ActiveZones,"Hand,Board")))continue;
            auto value=Resolve(*spec,c,owner);if(!value)return {};if(auto number=value->Integer();number && continuousMultiplier){auto factor=continuousMultiplier(c,e);if(!factor)return {};auto total=std::int64_t(*number)* *factor;if(total<INT32_MIN || total>INT32_MAX)return {};value->items[0]=int(total);}
            if(!Assign(table,std::string(name),*value,Text(spec->Operation,"Set")))return {};
        }
    }
    auto found=table.find(name);std::optional<Value> result=found==table.end()?std::nullopt:std::optional<Value>(found->second);if(!Engine::SimulationChanges::depth)ModelIndex(model).values[key]=result;return result;
}
inline int Multiplier(Context c,Obj* runtime){
    auto service=Engine::Match::Service(Engine::Match::Instances(Engine::EntityAccess::Find(c.model,c.source)),BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator"));if(!service || !service->Get())return 0;
    auto calculate=BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator").GetMethod("CalculateEffectEntityMultiplier",2).cast<int>();return calculate.IsValid()?calculate[service->Get()](runtime,0):0;
}
inline bool Prepare(const Engine::EffectGuards::Context& context){
    if(!context.effect || !context.effect->klass || std::string_view(context.effect->klass->name)!="SetVariableEffect")return true;
    auto* spec=reinterpret_cast<Runtime*>(context.effect);if(Text(spec->Operation)=="NoOp")return false;Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));Obj* e=Find(model,Field(context.effect,"EffectEntityId"));Context c{model,Source(e),Field(context.effect,"TargetId"),Field(context.effect,"EffectEntityId"),context.effect};
    if(Engine::EntityAccess::Component(e,"Continuous"))return false;
    bool applied=false;
    for(Obj* owner:Recipients(*spec,c)){auto table=Stored(owner);auto value=Resolve(*spec,c,owner);
        if(table && value){if(auto n=value->Integer()){auto total=std::int64_t(*n)*Multiplier(c,context.effect);if(total<INT32_MIN || total>INT32_MAX)value={};else value->items[0]=int(total);}if(value){auto next=*table;if(Assign(next,Text(spec->Variable),*value,Text(spec->Operation,"Set")) && Write(model,owner,next))applied=true;}}
    }if(applied)return false;
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<16)LOG_WARN("CustomComponents/Variables: assignment rejected variable=%s",Text(spec->Variable).c_str());return false;
}
}
