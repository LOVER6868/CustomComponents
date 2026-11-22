#pragma once
#include "variables/Components.hpp"
#include "abilities/SelfTeleport.hpp"
#include "variables/Expressions.hpp"
#include "level/LevelValue.hpp"
#include "engine/CardStatValues.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EffectGuards.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/InDiscardPileQuery.hpp"
namespace CustomComponents::Variables {
inline thread_local bool executingBeforeTriggers=false;
inline void (*onVariableAssigned)(Obj*,Obj*,std::string_view)=nullptr;
inline BNM::Field<Obj*> nativeContextModelField;
inline BNM::Class entityFinderClass,entityQueryContextClass,effectClass;
inline BNM::Field<Obj*> finderQueryContext;
inline BNM::Field<int> querySelfId;
inline BNM::Method<void> querySetSourceId,querySetTargetId,querySetEffectId;
inline BNM::Method<Obj*> effectToQueryContext;
inline bool InitializeResolver(){
    entityFinderClass=BNM::Class("PvZCards.Engine","EntityFinder");
    entityQueryContextClass=BNM::Class("PvZCards.Engine","EntityQueryContext");
    effectClass=BNM::Class("PvZCards.Engine","Effect");
    nativeContextModelField=entityQueryContextClass.GetField("<Model>k__BackingField").cast<Obj*>();
    finderQueryContext=entityFinderClass.GetField("QueryContext").cast<Obj*>();
    querySelfId=entityQueryContextClass.GetField("SelfId").cast<int>();
    querySetSourceId=entityQueryContextClass.GetMethod("set_SourceId",1).cast<void>();
    querySetTargetId=entityQueryContextClass.GetMethod("set_TargetId",1).cast<void>();
    querySetEffectId=entityQueryContextClass.GetMethod("set_EffectEntityId",1).cast<void>();
    effectToQueryContext=effectClass.GetMethod("ToEntityQueryContext",1).cast<Obj*>();
    return entityFinderClass.IsValid()&&entityQueryContextClass.IsValid()&&effectClass.IsValid()&&nativeContextModelField.IsValid()&&finderQueryContext.IsValid()&&querySelfId.IsValid()&&querySetSourceId.IsValid()&&querySetTargetId.IsValid()&&querySetEffectId.IsValid()&&effectToQueryContext.IsValid();
}
struct Context {Obj* model=nullptr;int source=-1,target=-1,effect=-1;Obj* runtime=nullptr;Obj* parentContext=nullptr;};
inline int Field(Obj* o,const char* name){if(!o)return -1;auto field=BNM::Class(o).GetField(name).cast<int>();return field.IsValid()?field[o].Get():-1;}
inline Obj* NativeContextModel(Obj* c){auto field=nativeContextModelField;return c&&field.IsValid()?field[c].Get():nullptr;}
inline Obj* ContextModel(Obj* c){return Engine::BoardPlacement::Model(c);}
inline int ContextId(Obj* c,const char* name){
    if(!c)return -1;
    static auto source=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("get_SourceId",0).cast<int>();
    static auto target=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("get_TargetId",0).cast<int>();
    static auto effect=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("get_EffectEntityId",0).cast<int>();
    BNM::Method<int> get;
    if(std::string_view(name)=="get_SourceId")get=source;
    else if(std::string_view(name)=="get_TargetId")get=target;
    else if(std::string_view(name)=="get_EffectEntityId")get=effect;
    else get=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod(name,0).cast<int>();
    return get.IsValid()?Core::Metadata::Bind(get,c)():-1;
}
struct QueryScope {
    std::unique_ptr<Core::ManagedRuntime::Root> finder;
    Obj* context=nullptr;Obj* instances=nullptr;bool borrowed=false;
    Obj* Finder(Obj* model){
        // sprig-lint: allow managed-global finder owns a ManagedRuntime::Root GC handle; it never retains an unrooted managed pointer
        if(!finder && model)finder=std::make_unique<Core::ManagedRuntime::Root>(entityFinderClass.CreateNewObjectParameters(model));
        return finder?finder->Get():nullptr;
    }
    explicit QueryScope(Context c){
        if(!c.model)return;
        if(c.parentContext){context=c.parentContext;return;}
        Obj* nativeFinder=Finder(c.model);if(!nativeFinder)return;
        context=Core::Metadata::Bind(finderQueryContext,nativeFinder).Get();if(!context)return;
        if(c.runtime){instances=Engine::Match::Instances(Engine::EntityAccess::Find(c.model,c.source));if(instances){Obj* value=effectToQueryContext.IsValid()?Core::Metadata::Bind(effectToQueryContext,c.runtime)(instances):nullptr;if(value){context=value;borrowed=true;}}}
        if(!Core::ManagedRuntime::SetReference(context,nativeContextModelField,c.model)){context=nullptr;return;}Core::Metadata::Bind(querySelfId,context).Set(c.source);if(!borrowed)Core::Metadata::Bind(querySetSourceId,context)(c.source);Core::Metadata::Bind(querySetTargetId,context)(c.target);Core::Metadata::Bind(querySetEffectId,context)(c.effect);
    }
    ~QueryScope(){if(borrowed && context)Core::ManagedPool::Return(instances,context);}
    std::vector<Obj*> Find(Obj* query,bool database=false){
        if(!query || !context)return {};
        if(!database && query->klass && query->klass->name && std::string_view(query->klass->name)=="SelfQuery"){
            auto self=Core::Metadata::Bind(querySelfId,context).Get();
            Obj* owner=Engine::EntityAccess::Find(NativeContextModel(context),self);
            return owner?std::vector<Obj*>{owner}:std::vector<Obj*>{};
        }
        Engine::EntityAccess::List* list=nullptr;
        // The one-argument database finder discards the caller's live context.
        // Use its contextual overload for picked GUIDs and source variables.
        auto type=BNM::Class(database?"PvZCards.Core.Utilities":"PvZCards.Engine",database?"CardDatabase":"EntityFinder");
        auto find=type.GetMethod("FindEntities",{"query","context"}).cast<Engine::EntityAccess::List*>();
        Obj* owner=database?Engine::BoardPlacement::Database():Finder(NativeContextModel(context));if(!owner || !find.IsValid())return {};list=find[owner](query,context);
        Core::ManagedRuntime::Root result(reinterpret_cast<Obj*>(list));if(!Engine::EntityAccess::Valid(list) || list->size>20000)return {};return {list->items->m_Items,list->items->m_Items+list->size};
    }
};
inline Obj* Find(Obj* model,int id){return Engine::EntityAccess::Find(model,id);}
inline int Source(Obj* effect){Obj* c=Engine::EntityAccess::Component(effect,"EffectEntity");return Field(c,"SourceId");}
// The stock Effect.SourceId can be -1 for a triggered effect even when the
// owning card and its EffectEntities association remain in the live model.
// Never substitute an unrelated trigger target for Self: first resolve the
// *actual* associated card. This is used by both variable writes and the
// shared multiplier pipeline.
inline int OwningCard(Obj* model,int effectId){
    if(!model || effectId<0)return -1;
    static auto ids=BNM::Class("PvZCards.Engine.Components","EntityAssociation").GetField("EntityIds").cast<BNM::Structures::Mono::List<int>*>();
    if(!ids.IsValid())return -1;
    auto api=Engine::EntityAccess::Api();auto* entities=api.entities[model].Get();
    if(!Engine::EntityAccess::Valid(entities) || entities->size>10000)return -1;
    for(int i=0;i<entities->size;++i){
        Obj* card=entities->items->m_Items[i];
        if(!card || !Engine::EntityAccess::Component(card,"Card"))continue;
        Obj* association=Engine::EntityAccess::Component(card,"EffectEntities");
        auto* list=association?ids[association].Get():nullptr;
        if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))continue;
        for(int j=0;j<list->size;++j)if(list->items->m_Items[j]==effectId)return Engine::Match::Id(card);
    }
    return -1;
}
inline int ResolveSource(Obj* model,int source,int effectId,int target=-1,bool selfFallback=false){
    if(source>=0 && Find(model,source))return source;
    const int owner=OwningCard(model,effectId);
    if(owner>=0)return owner;
    if(selfFallback && target>=0 && Engine::EntityAccess::Component(Find(model,target),"Card"))return target;
    return source;
}
inline bool BindingTargets(Obj* primary){return primary && Field(primary,"SelectionType")==0 && Field(primary,"NumTargets")==0 && Field(primary,"TargetScopeType")==0 && Field(primary,"AdditionalTargetType")==0;}
inline bool ZoneActive(Context c,Obj* source,std::string_view zones){
    if(zones.empty())zones="Hand,Board";
    if((zones.find("Hand")!=zones.npos && Engine::EntityAccess::Component(source,"HandElement")) || (zones.find("Board")!=zones.npos && Engine::EntityAccess::Component(source,"LaneElement")))return true;
    if(zones.find("Deck")==zones.npos && zones.find("Discard")==zones.npos)return false;
    QueryScope scope(c);if(!scope.context)return false;
    if(zones.find("Deck")!=zones.npos){Queries::InDeckQuery query;if(query.Matches(source,scope.context))return true;}
    if(zones.find("Discard")!=zones.npos){Queries::InDiscardPileQuery query;if(query.Matches(source,scope.context))return true;}return false;
}
struct Index {int version=-1;Obj* listIdentity=nullptr;std::vector<int> setters,grants,cards,variableOwners;std::uint64_t revision=~std::uint64_t{};std::map<std::pair<int,std::string>,std::optional<Value>> values;std::optional<std::map<int,std::vector<int>>> desired;std::vector<int> grantedTargets;bool grantsInspected=false;};
struct IndexedModel {std::shared_ptr<Core::ManagedRuntime::Root> model;Index index;};
inline thread_local std::vector<IndexedModel> indexes;
inline Index& ModelIndex(Obj* model){
    indexes.erase(std::remove_if(indexes.begin(),indexes.end(),[](auto& e){return !e.model->Get();}),indexes.end());
    auto it=std::find_if(indexes.begin(),indexes.end(),[&](auto& e){return e.model->Get()==model;});if(it==indexes.end()){indexes.push_back({std::make_shared<Core::ManagedRuntime::Root>(model,true),{}});it=std::prev(indexes.end());}
    auto api=Engine::EntityAccess::Api();auto* list=model?api.entities[model].Get():nullptr;auto& index=it->index;
    if(Engine::EntityAccess::Valid(list) && (index.listIdentity!=reinterpret_cast<Obj*>(list) || index.version!=list->version)){
        index.setters.clear();index.grants.clear();index.cards.clear();index.variableOwners.clear();
        for(int i=0;i<list->size;++i){Obj* e=list->items->m_Items[i];if(!e)continue;int id=api.id[e].Get();if(Engine::EntityAccess::Component(e,"Variables"))index.variableOwners.push_back(id);if(Engine::EntityAccess::Component(e,"SelfTeleport"))index.cards.push_back(id);if(!Engine::EntityAccess::Component(e,"Continuous"))continue;if(Engine::EntityAccess::Component(e,"SetVariableEffectDescriptor"))index.setters.push_back(id);if(Field(Engine::EntityAccess::Component(e,"GrantAbilityEffectDescriptor"),"GrantableAbilityType")==Abilities::SelfTeleport::Id)index.grants.push_back(id);}
        std::sort(index.setters.begin(),index.setters.end());std::sort(index.grants.begin(),index.grants.end());index.version=list->version;index.listIdentity=reinterpret_cast<Obj*>(list);index.values.clear();index.desired.reset();
    }
    auto revision=Engine::SimulationChanges::Revision(model);if(index.revision!=revision){index.revision=revision;index.values.clear();index.desired.reset();}return index;
}
inline void RememberVariableOwner(Obj* model,Obj* owner){
    if(!model || !owner)return;
    const int id=Engine::Match::Id(owner);
    if(id<0)return;
    auto& owners=ModelIndex(model).variableOwners;
    if(std::find(owners.begin(),owners.end(),id)==owners.end())owners.push_back(id);
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
inline std::optional<std::optional<Value>> (*eventVariable)(Obj*,Obj*,std::string_view)=nullptr;
inline std::optional<Value> ReadExpression(Obj* model,Obj* owner,std::string_view name){
    if(eventVariable)if(auto value=eventVariable(model,owner,name))return *value;
    return Read(model,owner,name);
}
inline std::optional<int>(*continuousMultiplier)(Context,Obj*)=nullptr;
template<class Spec> inline std::optional<Value> Resolve(const Spec& spec,Context c,Obj* expressionOwner){
    auto kind=ParseKind(Text(spec.ValueType,"Int"));if(!kind)return {};auto mode=Text(spec.ValueSource,"Literal");
    if(mode=="ArrayElement" || mode=="ArrayLength"){
        auto v=ReadExpression(c.model,expressionOwner,Text(spec.Expression));if(!v || !v->Array())return {};
        if(mode=="ArrayLength")return *kind==Kind::Int?std::optional<Value>(Value{Kind::Int,{int(v->items.size())}}):std::nullopt;
        if(int(*kind)>=3 || int(*kind)!=int(v->kind)-3 || spec.Value<0 || spec.Value>=int(v->items.size()))return {};return Value{*kind,{v->items[spec.Value]}};
    }
    if(mode=="Variable"){auto value=ReadExpression(c.model,expressionOwner,Text(spec.Expression));return value && value->kind==*kind?value:std::nullopt;}
    if(mode=="Query" || mode=="DatabaseQuery" || mode=="QueryCount"){
        if(mode!="QueryCount" && (*kind==Kind::Int || *kind==Kind::Ints || (mode=="DatabaseQuery" && (*kind==Kind::Entity || *kind==Kind::Entities))))return {};
        QueryScope scope(c);auto matches=scope.Find(spec.Query,mode=="DatabaseQuery");if(mode=="QueryCount")return *kind==Kind::Int?std::optional<Value>(Value{Kind::Int,{int(matches.size())}}):std::nullopt;
        Value value{*kind,{}};for(Obj* e:matches){int n=(*kind==Kind::Guid || *kind==Kind::Guids)?Engine::BoardPlacement::Guid(e):Engine::Match::Id(e);if(n<0)return {};value.items.push_back(n);}return value.Valid()?std::optional<Value>(value):std::nullopt;
    }
    if(mode=="Level") {
        // Level is Leveled.Value (extra sun), not the option index or total cost.
        // A normal play has no Leveled component and yields Int(0).
        if(*kind!=Kind::Int) return {};
        auto level=Level::ReadLevelValue(expressionOwner);
        return level?std::optional<Value>(Value{Kind::Int,{*level}}):std::nullopt;
    }
    if(mode=="Attack" || mode=="Health" || mode=="Cost"){
        if(*kind!=Kind::Int || !expressionOwner)return {};
        auto value=Engine::CardStatValues::Read(expressionOwner,mode);
        return value?std::optional<Value>(Value{Kind::Int,{*value}}):std::nullopt;
    }
    if(mode=="BaseAttack" || mode=="BaseHealth" || mode=="BaseCost" ||
       mode=="CurrentHealth" || mode=="DamageTaken"){
        if(*kind!=Kind::Int)return {};
        auto value=Engine::CardStatValues::Read(expressionOwner,mode);
        return value?std::optional<Value>(Value{Kind::Int,{*value}}):std::nullopt;
    }
    if(mode=="Self" || mode=="Target" || mode=="PrimaryGuid"){
        if(*kind==Kind::Int || *kind==Kind::Ints || (mode=="PrimaryGuid" && *kind!=Kind::Guid && *kind!=Kind::Guids))return {};
        Obj* e=Find(c.model,mode=="Self"?c.source:c.target);int n=(*kind==Kind::Guid || *kind==Kind::Guids)?Engine::BoardPlacement::Guid(e):Engine::Match::Id(e);if(n<0)return {};Value v{*kind,{n}};return v.Valid()?std::optional<Value>(v):std::nullopt;
    }
    if(mode=="TriggerSource" || mode=="TriggerTarget"){
        if(!c.runtime || *kind==Kind::Int || *kind==Kind::Ints)return {};
        // sprig-lint: allow init-phase this lookup executes inside Resolve, never at library load
        auto contextField=BNM::Class("PvZCards.Engine","Effect").GetField("Context").cast<Obj*>();
        Obj* event=contextField.IsValid()?Core::Metadata::Bind(contextField,c.runtime).Get():nullptr;
        const int id=Field(event,mode=="TriggerSource"?"SourceId":"TargetId");
        if(id<0)return {};
        const int value=(*kind==Kind::Guid || *kind==Kind::Guids)?Engine::BoardPlacement::Guid(Find(c.model,id)):id;
        Value result{*kind,{value}};return result.Valid()?std::optional<Value>(result):std::nullopt;
    }
    if(mode!="Literal" || *kind!=Kind::Int){if(mode=="Literal" && *kind!=Kind::Int && int(*kind)<3){Value v{*kind,{spec.Value}};if(v.Valid())return v;}return {};}
    if(spec.Expression && !spec.Expression->str().empty()){auto n=Expressions::Evaluate(spec.Expression->str(),[&](auto name){return ReadExpression(c.model,expressionOwner,name);});return n?std::optional<Value>(Value{Kind::Int,{*n}}):std::nullopt;}
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
    auto stored=Stored(owner);if(!stored)return {};Table table=*stored;auto timed=Timed(owner);if(!timed || !ApplyTimed(table,*timed))return {};auto setters=ModelIndex(model).setters;
    for(int effectId:setters){
        Obj* e=Find(model,effectId);auto* spec=reinterpret_cast<Descriptor*>(Engine::EntityAccess::Component(e,"SetVariableEffectDescriptor"));if(!spec || Text(spec->Variable)!=name)continue;
        Context c{model,ResolveSource(model,Source(e),effectId),-1,effectId};Obj* primary=Engine::EntityAccess::Component(e,"PrimaryTargetFilter");if(!BindingTargets(primary))continue;
        auto query=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();QueryScope selection(c);
        for(Obj* target:selection.Find(query[primary].Get())){c.target=Engine::Match::Id(target);auto owners=Recipients(*spec,c);if(std::find(owners.begin(),owners.end(),owner)==owners.end() || !Conditions(c,e,target,Text(spec->ActiveZones,"Hand,Board")))continue;
            auto value=Resolve(*spec,c,owner);if(!value)return {};if(auto number=value->Integer();number && continuousMultiplier){auto factor=continuousMultiplier(c,e);if(!factor)return {};auto total=std::int64_t(*number)* *factor;if(total<INT32_MIN || total>INT32_MAX)return {};value->items[0]=int(total);}
            if(!Assign(table,std::string(name),*value,Text(spec->Operation,"Set")))return {};
        }
    }
    auto found=table.find(name);std::optional<Value> result=found==table.end()?std::nullopt:std::optional<Value>(found->second);if(!Engine::SimulationChanges::depth)ModelIndex(model).values[key]=result;return result;
}
inline int Multiplier(Context c,Obj* runtime){
    // Numeric SetVariable without any multiplier must add its literal Value,
    // not multiply it by zero because the trigger carried SourceId=-1.
    Obj* entity=Find(c.model,c.effect);
    if(!entity || (!Engine::EntityAccess::Component(entity,"VariableMultiplier") &&
                   !Engine::EntityAccess::Component(entity,"LevelSpentMultiplier") &&
                   !Engine::EntityAccess::Component(entity,"QueryMultiplier")))return 1;
    c.source=ResolveSource(c.model,c.source,c.effect,c.target);
    auto service=Engine::Match::Service(Engine::Match::Instances(Find(c.model,c.source)),BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator"));
    if(!service || !service->Get())return 0; // Explicit multiplier: fail closed.
    auto calculate=BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator").GetMethod("CalculateEffectEntityMultiplier",2).cast<int>();
    return calculate.IsValid()?calculate[service->Get()](runtime,0):0;
}
inline bool Prepare(const Engine::EffectGuards::Context& context){
    if(!context.effect || !context.effect->klass || std::string_view(context.effect->klass->name)!="SetVariableEffect")return true;
    auto* spec=reinterpret_cast<Runtime*>(context.effect);if(Text(spec->Operation)=="NoOp")return false;Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));Obj* e=Find(model,Field(context.effect,"EffectEntityId"));Context c{model,Source(e),Field(context.effect,"TargetId"),Field(context.effect,"EffectEntityId"),context.effect};
    if(Text(spec->ExecutionTiming)=="BeforeTriggers" && !executingBeforeTriggers)return false;
    if(c.source<0)c.source=Field(context.effect,"SourceId");
    c.source=ResolveSource(model,c.source,c.effect,c.target,Text(spec->Owner)=="Self");
    if(Engine::EntityAccess::Component(e,"Continuous"))return false;
    bool applied=false;int recipientCount=0;int missingStore=0,missingValue=0,missingTimed=0,failedWrite=0;
    for(Obj* owner:Recipients(*spec,c)){
        ++recipientCount;
        auto table=Stored(owner);auto value=Resolve(*spec,c,owner);
        if(!table)++missingStore;if(!value)++missingValue;
        if(table && value){if(auto n=value->Integer()){auto total=std::int64_t(*n)*Multiplier(c,context.effect);if(total<INT32_MIN || total>INT32_MAX)value={};else value->items[0]=int(total);}if(value){
                auto duration=Text(spec->Duration,"Permanent"),operation=Text(spec->Operation,"Set");
                const int durationId=Engine::EffectDurations::Parse(duration);
                if(durationId==Engine::EffectDurations::Permanent){
                    auto next=*table;if(Assign(next,Text(spec->Variable),*value,operation) && Write(model,owner,next)){applied=true;if(onVariableAssigned)onVariableAssigned(model,owner,Text(spec->Variable));}
                }else if(Engine::EffectDurations::Timed(durationId)){
                    // Timed mutations are reversible overlays; no reset effect is necessary.
                    // Non-numeric timed operations are rejected rather than corrupting saved values.
                    auto amount=value->Integer();auto entries=Timed(owner);
                    if(amount && entries && entries->size()<256 && TimedOperationSupported(ParseOperation(operation))){
                        entries->push_back({Text(spec->Variable),operation,durationId,*amount});
                        if(WriteTimed(model,owner,*entries)){applied=true;if(onVariableAssigned)onVariableAssigned(model,owner,Text(spec->Variable));}
                        else ++failedWrite;
                    } else ++missingTimed;
                }
            }}
    }
    if(!applied){
        static std::atomic<unsigned> modelSamples{0};if(modelSamples.fetch_add(1)<24)
            LOG_WARN("CustomComponents/Variables/MODEL_CONTEXT: router=%p token=%llu model=%p effect=%d source=%d target=%d",
                context.router,(unsigned long long)Engine::RouterContext::RouterModel(context.router),model,c.effect,c.source,c.target);
        static std::atomic<unsigned> failures{0};if(failures.fetch_add(1)<32)
        LOG_WARN("CustomComponents/Variables/WRITE_FAIL_DETAIL: source=%d target=%d effect=%d recipients=%d storeMissing=%d valueMissing=%d timedMissing=%d writeFailed=%d duration=%s operation=%s",
            c.source,c.target,c.effect,recipientCount,missingStore,missingValue,missingTimed,failedWrite,
            Text(spec->Duration).c_str(),Text(spec->Operation).c_str());
    }
    {
        static std::atomic<unsigned> traceSamples{0};
        if(traceSamples.fetch_add(1)<60)LOG_INFO("CustomComponents/Variables/WRITE_TRACE: variable=%s source=%d target=%d effectEntity=%d applied=%d",
            Text(spec->Variable).c_str(),c.source,c.target,c.effect,int(applied));
    }
    if(applied)return false;
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<16)LOG_WARN("CustomComponents/Variables: assignment rejected variable=%s",Text(spec->Variable).c_str());return false;
}
}
