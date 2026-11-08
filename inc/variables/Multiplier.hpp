#pragma once
#include "variables/Resolver.hpp"
namespace CustomComponents::Variables {
struct VariableMultiplier:Obj {
    unsigned char inherited[0x10]{};Str* Expression=nullptr;Str* Aggregation=nullptr;
    BNM_CustomClass(VariableMultiplier,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","VariableMultiplier","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","QueryMultiplier","EngineLib.dll").Build(),{});
    BNM_CustomField(Expression,BNM::Defaults::Get<Str*>(),"Expression");
    BNM_CustomField(Aggregation,BNM::Defaults::Get<Str*>(),"Aggregation");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* from=reinterpret_cast<VariableMultiplier*>(source);Reference(this,&Expression,from?from->Expression:nullptr);Reference(this,&Aggregation,from?from->Aggregation:nullptr);}
    void HandleReturnToPool(Obj* instances){Reference(this,&Expression,nullptr);Reference(this,&Aggregation,nullptr);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline std::optional<int> VariableFactor(VariableMultiplier* spec,Context c){
    auto field=BNM::Class("PvZCards.Engine.Components","QueryMultiplier").GetField("Query").cast<Obj*>();Obj* query=field[static_cast<Obj*>(spec)].Get();std::vector<Obj*> owners;
    if(query){QueryScope scope(c);owners=scope.Find(query);}else if(Obj* source=Find(c.model,c.source))owners.push_back(source);
    const auto aggregation=Text(spec->Aggregation,"Single");if(owners.empty() || (aggregation=="Single" && owners.size()!=1) || (aggregation!="Single" && aggregation!="Sum" && aggregation!="Min" && aggregation!="Max"))return {};
    std::int64_t total=0;bool first=true;for(Obj* owner:owners){auto n=Expressions::Evaluate(Text(spec->Expression),[&](auto name){return Read(c.model,owner,name);});if(!n)return {};if(aggregation=="Min")total=first?*n:std::min(total,std::int64_t(*n));else if(aggregation=="Max")total=first?*n:std::max(total,std::int64_t(*n));else total+=*n;first=false;}
    return total>=INT32_MIN && total<=INT32_MAX?std::optional<int>(int(total)):std::nullopt;
}
inline int(*originalSpecific)(Obj*,Obj*,Obj*,Obj*,int,const void*)=nullptr;
static int Specific(Obj* self,Obj* multiplier,Obj* entity,Obj* effect,int context,const void* method){
    if(!multiplier || !multiplier->klass || std::string_view(multiplier->klass->name)!="VariableMultiplier")return originalSpecific(self,multiplier,entity,effect,context,method);
    auto field=BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator").GetField("<Model>k__BackingField").cast<Obj*>();Context c{field[self].Get(),Source(entity),Field(effect,"TargetId"),Engine::Match::Id(entity),effect};
    return VariableFactor(reinterpret_cast<VariableMultiplier*>(multiplier),c).value_or(0);
}
inline std::optional<int> BindingFactor(Context c,Obj* entity){
    Obj* multiplier=Engine::EntityAccess::Component(entity,"VariableMultiplier");std::optional<int> n;
    if(multiplier)n=VariableFactor(reinterpret_cast<VariableMultiplier*>(multiplier),c);
    else if((multiplier=Engine::EntityAccess::Component(entity,"QueryMultiplier"))){auto field=BNM::Class("PvZCards.Engine.Components","QueryMultiplier").GetField("Query").cast<Obj*>();QueryScope scope(c);n=int(scope.Find(field[multiplier].Get()).size());}
    else return 1;
    if(!n)return {};auto dividerField=BNM::Class("PvZCards.Engine.Components","Multiplier").GetField("Divider").cast<int>();int divider=dividerField[multiplier].Get();if(!divider)divider=1;std::int64_t value=std::int64_t(*n)/divider;return value>=INT32_MIN && value<=INT32_MAX?std::optional<int>(int(value)):std::nullopt;
}
inline bool InitializeMultiplier(){using K=Bridge::FieldKind;
    continuousMultiplier=BindingFactor;
    return CustomComponentRegistry::Register({"VariableMultiplier","PvZCards.Engine.Components","EngineLib.dll","QueryMultiplier","PvZCards.Engine.Components","EngineLib.dll"}) && Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","VariableMultiplier"),BNM::Class("PvZCards.Engine.Components","QueryMultiplier")) && Bridge::Register("PvZCards.Engine.Components","VariableMultiplier","PvZCards.Engine.Components","QueryMultiplier",{{"Expression",K::String},{"Aggregation",K::String}},false) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator").GetMethod("CalculateMultiplierSpecificValue",4),Specific,originalSpecific);
}
}
