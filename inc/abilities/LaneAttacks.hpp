#pragma once
#include <algorithm>
#include "abilities/LaneAttackPolicy.hpp"
#include <charconv>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "core/AbilityRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/NativeConstruction.hpp"
#include "core/HookRegistry.hpp"
#include "engine/BoardPlacement.hpp"

namespace CustomComponents::Abilities::LaneAttacks {
using Obj=BNM::IL2CPP::Il2CppObject;
namespace Bridge=Systems::CustomFieldBridgeSystem;
inline constexpr int AlsoId=33,OnlyId=34;
#define SPRIG_LANE_COMPONENT(Type,Name) \
struct Type:Obj {std::uint8_t inherited[8]{};int InnateMask=0; \
BNM_CustomClass(Type,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components",Name,"EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{}); \
BNM_CustomField(InnateMask,BNM::Defaults::Get<int>(),"InnateMask"); \
void Construct(){Core::NativeConstruction::Base(this);} \
BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor"); \
void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto field=BNM::Class(this).GetField("InnateMask").cast<int>();if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(source?Core::Metadata::Bind(field,source).Get():0);} \
void HandleReturnToPool(Obj* instances){auto field=BNM::Class(this).GetField("InnateMask").cast<int>();if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(0);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);} \
BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build()); \
BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());};
SPRIG_LANE_COMPONENT(Also,"AttackAlsoInLanes")
SPRIG_LANE_COMPONENT(Only,"AttackOnlyInLanes")
#undef SPRIG_LANE_COMPONENT
inline void* Get(Obj* card,const char* name,bool create){return Engine::Match::Component(card,BNM::Class("PvZCards.Engine.Components",name).GetMonoType(),create);}
inline void* GetAlso(void* card,bool create){return Get(static_cast<Obj*>(card),"AttackAlsoInLanes",create);}
inline void* GetOnly(void* card,bool create){return Get(static_cast<Obj*>(card),"AttackOnlyInLanes",create);}
inline bool Active(Obj* component){return component && GrantableAbilityRuntime::Active(component);}
inline Obj* OnlyActive(Obj* fighter){Obj* component=Engine::EntityAccess::Component(fighter,"AttackOnlyInLanes");return Active(component)?component:nullptr;}
inline int Mask(Obj* component){
    if(!component)return 0;
    auto innate=BNM::Class(component).GetField("InnateMask").cast<int>();
    int mask=innate.IsValid()?Core::Metadata::Bind(innate,component).Get():0;
    Obj* group=static_cast<Obj*>(Core::Metadata::Bind(GrantableAbilityRuntime::countersField,component).Get());
    auto* list=group?reinterpret_cast<BNM::Structures::Mono::List<GrantableAbilityRuntime::CounterEntry>*>(Core::Metadata::Bind(GrantableAbilityRuntime::entriesField,group).Get()):nullptr;
    if(Engine::EntityAccess::Valid(list))for(int i=0;i<list->size;++i)mask|=list->items->m_Items[i].value;
    return mask;
}
inline bool Grant(Obj* component,int duration,int source,int mask,bool replace){
    if(!component || mask<0)return false;
    Obj* group=static_cast<Obj*>(Core::Metadata::Bind(GrantableAbilityRuntime::countersField,component).Get());
    auto* list=group?reinterpret_cast<BNM::Structures::Mono::List<GrantableAbilityRuntime::CounterEntry>*>(Core::Metadata::Bind(GrantableAbilityRuntime::entriesField,group).Get()):nullptr;
    if(!replace && Engine::EntityAccess::Valid(list))for(int i=0;i<list->size;++i){const auto& row=list->items->m_Items[i];if(row.duration==duration && row.sourceId==source)mask|=row.value;}
    Core::Metadata::Bind(GrantableAbilityRuntime::removeCounter,component)(duration,source);
    Core::Metadata::Bind(GrantableAbilityRuntime::addCounter,component)(duration,source,mask);
    return true;
}
inline void Properties(Obj* object,Obj* properties){
    if(!object || !object->klass)return;
    const auto name=std::string_view(object->klass->name);
    const bool component=name=="AttackAlsoInLanes" || name=="AttackOnlyInLanes";
    if(!component && name!="GrantAbilityEffectDescriptor")return;
    Obj* value=Bridge::DictionaryValue(properties,"Lanes");if(!value)return;
    if(!component){auto* kind=Bridge::ObjectToString(Bridge::DictionaryValue(properties,"GrantableAbilityType"));if(!kind || (kind->str()!="AttackAlsoInLanes" && kind->str()!="AttackOnlyInLanes"))return;}
    auto count=BNM::Class(value).GetMethod("get_Count",0).cast<int>();auto item=BNM::Class(value).GetMethod("get_Item",1).cast<Obj*>();
    auto* array=value->klass->rank==1?reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(value):nullptr;
    int size=array?int(array->capacity):count.IsValid()?Core::Metadata::Bind(count,value)():-1;
    int mask=0;bool valid=size>=0 && size<=1024 && (array || item.IsValid());
    for(int i=0;valid && i<size;++i){Obj* boxed=array?array->m_Items[i]:Core::Metadata::Bind(item,value)(i);auto* text=Bridge::ObjectToString(boxed);int index=-1;if(!text){valid=false;break;}const auto s=text->str();const auto parsed=std::from_chars(s.data(),s.data()+s.size(),index);valid=parsed.ec==std::errc{} && parsed.ptr==s.data()+s.size() && index>=0 && index<31;if(valid)mask|=1<<index;}
    auto set=BNM::Class(properties).GetMethod("set_Item",2).cast<void>();auto remove=BNM::Class(properties).GetMethod("Remove",1).cast<bool>();
    if(!valid || !set.IsValid() || !remove.IsValid()){LOG_ERR("CustomComponents/LaneAttacks: invalid Lanes array");return;}
    Obj* encoded=BNM::Class("System","Int32").BoxObject(mask);Core::ManagedRuntime::Root root(encoded);
    Core::Metadata::Bind(set,properties)(BNM::CreateMonoString(component?"InnateMask":"AbilityValue"),encoded);
    Core::Metadata::Bind(remove,properties)(BNM::CreateMonoString("Lanes"));
}
inline void Authored(Obj* object,Obj* properties){
    if(!object || !object->klass || !Bridge::DictionaryValue(properties,"InnateMask"))return;
    const std::string_view name=object->klass->name;
    if(name!="AttackAlsoInLanes" && name!="AttackOnlyInLanes")return;
    auto field=BNM::Class(object).GetField("InnateMask").cast<int>();if(!field.IsValid())return;
    // An innate marker uses the same persistent counter convention as stock
    // grantable abilities. Even an empty Only list is an active authored marker.
    Grant(object,0,-1,Core::Metadata::Bind(field,object).Get(),true);
    Obj* group=static_cast<Obj*>(Core::Metadata::Bind(GrantableAbilityRuntime::countersField,object).Get());
    static auto persistent=BNM::Class("PvZCards.Engine.Components.Data","ModifiableCounterGroup").GetField("IsPersistent").cast<bool>();
    if(group && persistent.IsValid())Core::Metadata::Bind(persistent,group).Set(true);
}
struct AttackData {int damage;bool overridden;std::uint8_t padding[3]{};};
struct CombatData {Obj* fighter;int opponent;std::uint32_t padding;Obj* lane;int laneId;AttackData attack;};
static_assert(sizeof(CombatData)==40);
inline thread_local bool suppressExpansion=false;
inline bool(*originalAdjacent)(Obj*,const void*)=nullptr;
inline bool(*originalAll)(Obj*,const void*)=nullptr;
inline bool(*originalOnlyAdjacent)(Obj*,const void*)=nullptr;
inline void(*originalPrimary)(Obj*,CombatData,Obj*,Obj*,const void*)=nullptr;
inline void(*originalSecondary)(Obj*,CombatData,Obj*,const void*)=nullptr;
inline bool Adjacent(Obj* fighter,const void* method){return !suppressExpansion && originalAdjacent(fighter,method);}
inline bool All(Obj* fighter,const void* method){return !suppressExpansion && originalAll(fighter,method);}
inline bool OnlyAdjacent(Obj* fighter,const void* method){return !suppressExpansion && originalOnlyAdjacent(fighter,method);}
inline bool Custom(Obj* fighter){return Active(Engine::EntityAccess::Component(fighter,"AttackAlsoInLanes")) || OnlyActive(fighter);}
inline void Primary(Obj* creator,CombatData combat,Obj* opponents,Obj* builder,const void* method){
    if(!Custom(combat.fighter)){originalPrimary(creator,combat,opponents,builder,method);return;}
    static auto modelField=BNM::Class("PvZCards.Core.Systems.Utilities","FighterDamageEffectCreator").GetField("<Entities>k__BackingField").cast<Obj*>();
    static auto index=BNM::Class("PvZCards.Engine.Components","Lane").GetField("CombatOrderIndex").cast<int>();
    static auto add=BNM::Class("PvZCards.Core.Systems.Utilities","DamageListBuilder").GetMethod("AddDamageEffects",1).cast<void>();
    Obj* model=modelField.IsValid()?Core::Metadata::Bind(modelField,creator).Get():nullptr;
    if(!model || !index.IsValid() || !add.IsValid())return;
    Obj* only=OnlyActive(combat.fighter);
    const int center=combat.lane?Core::Metadata::Bind(index,combat.lane).Get():-1;
    const auto mask=LaneAttackPolicy::Plan(only!=nullptr,only?Mask(only):0,
        Active(Engine::EntityAccess::Component(combat.fighter,"AttackAlsoInLanes"))?Mask(Engine::EntityAccess::Component(combat.fighter,"AttackAlsoInLanes")):0,center,
        originalAdjacent(combat.fighter,nullptr),originalOnlyAdjacent(combat.fighter,nullptr),originalAll(combat.fighter,nullptr));
    std::vector<std::pair<int,Obj*>> lanes;
    auto api=Engine::EntityAccess::Api();auto* entities=Core::Metadata::Bind(api.entities,model).Get();
    if(!Engine::EntityAccess::Valid(entities))return;
    for(int i=0;i<entities->size;++i)if(Obj* lane=Engine::EntityAccess::Component(entities->items->m_Items[i],"Lane")){
        int order=Core::Metadata::Bind(index,lane).Get();if(order>=0 && order<31 && (mask&LaneAttackPolicy::Bit(order)))lanes.push_back({order,entities->items->m_Items[i]});
    }
    std::sort(lanes.begin(),lanes.end(),[](auto a,auto b){return a.first<b.first;});
    struct Scope {bool old=suppressExpansion;Scope(){suppressExpansion=true;}~Scope(){suppressExpansion=old;}} scope;
    for(auto [order,entity]:lanes){
        auto laneCombat=combat;laneCombat.lane=Engine::EntityAccess::Component(entity,"Lane");laneCombat.laneId=Engine::Match::Id(entity);
        if(order==center && !only)originalPrimary(creator,laneCombat,opponents,builder,method);
        else Core::Metadata::Bind(add,builder)(laneCombat);
        // Suppress lane expansion predicates only. Native splash generation
        // remains active at each actual attack lane.
        originalSecondary(creator,laneCombat,builder,nullptr);
    }
}
inline void Secondary(Obj* creator,CombatData combat,Obj* builder,const void* method){if(!Custom(combat.fighter))originalSecondary(creator,combat,builder,method);}
inline bool Initialize(){
    auto base=BNM::Class("PvZCards.Engine.Components","GrantableAbility");
    for(auto name:{"AttackAlsoInLanes","AttackOnlyInLanes"}){
        auto type=BNM::Class("PvZCards.Engine.Components",name);
        if(!CustomComponentRegistry::Register({name,"PvZCards.Engine.Components","EngineLib.dll","GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"}) || !Core::ComponentLifecycle::Patch(type,base) || !Bridge::Register("PvZCards.Engine.Components",name,"PvZCards.Engine.Components","GrantableAbility",{{"InnateMask",Bridge::FieldKind::Int32}},false))return false;
    }
    if(!AbilityRegistry::Register({AlsoId,"AttackAlsoInLanes",AbilityStorage::Grantable,GetAlso,nullptr}) || !AbilityRegistry::Register({OnlyId,"AttackOnlyInLanes",AbilityStorage::Grantable,GetOnly,nullptr}) || !Bridge::beforeProperties.Add(Properties) || !Bridge::afterProperties.Add(Authored))return false;
    auto creator=BNM::Class("PvZCards.Core.Systems.Utilities","FighterDamageEffectCreator");
    return Core::Hooks::Install(creator.GetMethod("IsAttackingInAdjacentLanes",1),Adjacent,originalAdjacent) && Core::Hooks::Install(creator.GetMethod("IsAttackingInAllLanes",1),All,originalAll) && Core::Hooks::Install(creator.GetMethod("IsAttackingOnlyInAdjacentLanes",1),OnlyAdjacent,originalOnlyAdjacent) && Core::Hooks::Install(creator.GetMethod("AddPrimaryDamageEffects",3),Primary,originalPrimary) && Core::Hooks::Install(creator.GetMethod("AddSecondaryDamageEffects",2),Secondary,originalSecondary);
}
}
