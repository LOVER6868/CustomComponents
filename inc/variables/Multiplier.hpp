#pragma once
#include "variables/Resolver.hpp"
namespace CustomComponents::Variables {
struct VariableMultiplier:Obj {
    unsigned char inherited[0x10]{};Str* Expression=nullptr;Str* Aggregation=nullptr;Str* Owner=nullptr;
    BNM_CustomClass(VariableMultiplier,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","VariableMultiplier","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","QueryMultiplier","EngineLib.dll").Build(),{});
    BNM_CustomField(Expression,BNM::Defaults::Get<Str*>(),"Expression");
    BNM_CustomField(Aggregation,BNM::Defaults::Get<Str*>(),"Aggregation");
    BNM_CustomField(Owner,BNM::Defaults::Get<Str*>(),"Owner");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* from=reinterpret_cast<VariableMultiplier*>(source);Reference(this,&Expression,from?from->Expression:nullptr);Reference(this,&Aggregation,from?from->Aggregation:nullptr);Reference(this,&Owner,from?from->Owner:nullptr);}
    void HandleReturnToPool(Obj* instances){Reference(this,&Expression,nullptr);Reference(this,&Aggregation,nullptr);Reference(this,&Owner,nullptr);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
// No new payment state: all readings come from the existing Leveled.Value.
// Query and Divider are inherited from the native QueryMultiplier component.
struct LevelSpentMultiplier:Obj {
    unsigned char inherited[0x10]{};
    BNM_CustomClass(LevelSpentMultiplier,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","LevelSpentMultiplier","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","QueryMultiplier","EngineLib.dll").Build(),{});
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);}
    void HandleReturnToPool(Obj* instances){BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline std::optional<int> LevelSpentFactor(Obj* spec,Context c){
    if(!spec || !c.model)return {};
    auto field=BNM::Class("PvZCards.Engine.Components","QueryMultiplier").GetField("Query").cast<Obj*>();
    if(!field.IsValid())return {};
    Obj* query=field[spec].Get();
    Obj* owner=nullptr;
    if(query){
        QueryScope scope(c);
        auto owners=scope.Find(query);
        if(owners.size()!=1)return {};
        owner=owners.front();
    }else owner=Find(c.model,c.source);
    return Level::ReadLevelValue(owner);
}
// Query ownership retains the old aggregation semantics. PrimaryTarget deliberately
// selects the ONE recipient of the current runtime effect; a separate runtime
// effect is generated for each selected primary card by the stock creator.
inline std::optional<int> VariableFactor(VariableMultiplier* spec,Context c){
    if(!spec || !c.model)return {};
    auto field=BNM::Class("PvZCards.Engine.Components","QueryMultiplier").GetField("Query").cast<Obj*>();
    if(!field.IsValid())return {};
    Obj* query=field[static_cast<Obj*>(spec)].Get();
    const auto ownerMode=Text(spec->Owner,"");
    std::vector<Obj*> owners;
    if(ownerMode=="PrimaryTarget"){
        // Query describes potentially many entities; it has no well-defined
        // meaning alongside this per-recipient mode.
        if(query || !c.runtime || c.target<0)return {};
        if(auto* card=Find(c.model,c.target))owners.push_back(card);
    }else if(ownerMode=="Self"){
        if(query)return {};
        if(auto* card=Find(c.model,c.source))owners.push_back(card);
    }else if(ownerMode.empty() || ownerMode=="Query"){
        if(query){QueryScope scope(c);owners=scope.Find(query);}
        else if(ownerMode.empty()) {if(auto* card=Find(c.model,c.source))owners.push_back(card);}
        else return {};
    }else return {}; // Target belongs to the trigger context, not this effect.
    const auto aggregation=Text(spec->Aggregation,"Single");
    if(ownerMode=="PrimaryTarget" && aggregation!="Single")return {};
    if(owners.empty() || (aggregation=="Single" && owners.size()!=1) ||
        (aggregation!="Single" && aggregation!="Sum" && aggregation!="Min" && aggregation!="Max"))return {};
    std::int64_t total=0;bool first=true;
    for(Obj* owner:owners){
        auto lookup=[&](std::string_view name)->std::optional<Value>{
            if(auto v=Read(c.model,owner,name))return v;
            // A bare stat name is not required to be registered as a variable.
            if(auto stat=Engine::CardStatValues::Read(owner,name))return Value{Kind::Int,{*stat}};
            return {};
        };
        auto n=Expressions::Evaluate(Text(spec->Expression),lookup);
        if(!n)return {};
        if(aggregation=="Min")total=first?*n:std::min(total,std::int64_t(*n));
        else if(aggregation=="Max")total=first?*n:std::max(total,std::int64_t(*n));
        else total+=*n;
        first=false;
    }
    return total>=INT32_MIN && total<=INT32_MAX?std::optional<int>(int(total)):std::nullopt;
}

inline int(*originalSpecific)(Obj*,Obj*,Obj*,Obj*,int,const void*)=nullptr;
static int Specific(Obj* self,Obj* multiplier,Obj* entity,Obj* effect,int context,const void* method){
    if(!multiplier || !multiplier->klass || !multiplier->klass->name)return originalSpecific(self,multiplier,entity,effect,context,method);
    const std::string_view name=multiplier->klass->name;
    if(name!="VariableMultiplier" && name!="LevelSpentMultiplier")return originalSpecific(self,multiplier,entity,effect,context,method);
    auto field=BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator").GetField("<Model>k__BackingField").cast<Obj*>();
    if(!field.IsValid())return 0;
    Context c{field[self].Get(),Source(entity),Field(effect,"TargetId"),Engine::Match::Id(entity),effect};
    if(name=="LevelSpentMultiplier")return LevelSpentFactor(multiplier,c).value_or(0);
    return VariableFactor(reinterpret_cast<VariableMultiplier*>(multiplier),c).value_or(0);
}
inline std::optional<int> BindingFactor(Context c,Obj* entity){
    Obj* multiplier=Engine::EntityAccess::Component(entity,"VariableMultiplier");std::optional<int> n;
    if(multiplier)n=VariableFactor(reinterpret_cast<VariableMultiplier*>(multiplier),c);
    else if((multiplier=Engine::EntityAccess::Component(entity,"LevelSpentMultiplier")))n=LevelSpentFactor(multiplier,c);
    else if((multiplier=Engine::EntityAccess::Component(entity,"QueryMultiplier"))){auto field=BNM::Class("PvZCards.Engine.Components","QueryMultiplier").GetField("Query").cast<Obj*>();QueryScope scope(c);n=int(scope.Find(field[multiplier].Get()).size());}
    else return 1;
    if(!n)return {};auto dividerField=BNM::Class("PvZCards.Engine.Components","Multiplier").GetField("Divider").cast<int>();int divider=dividerField[multiplier].Get();if(!divider)divider=1;std::int64_t value=std::int64_t(*n)/divider;return value>=INT32_MIN && value<=INT32_MAX?std::optional<int>(int(value)):std::nullopt;
}
inline bool InitializeMultiplier(){using K=Bridge::FieldKind;
    continuousMultiplier=BindingFactor;
    auto base=BNM::Class("PvZCards.Engine.Components","QueryMultiplier");
    return CustomComponentRegistry::Register({"VariableMultiplier","PvZCards.Engine.Components","EngineLib.dll","QueryMultiplier","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","VariableMultiplier"),base) &&
        Bridge::Register("PvZCards.Engine.Components","VariableMultiplier","PvZCards.Engine.Components","QueryMultiplier",{{"Expression",K::String},{"Aggregation",K::String},{"Owner",K::String}},false) &&
        CustomComponentRegistry::Register({"LevelSpentMultiplier","PvZCards.Engine.Components","EngineLib.dll","QueryMultiplier","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","LevelSpentMultiplier"),base) &&
        // The single existing native hook services VariableMultiplier and LevelSpentMultiplier.
        Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator").GetMethod("CalculateMultiplierSpecificValue",4),Specific,originalSpecific);
}
}
