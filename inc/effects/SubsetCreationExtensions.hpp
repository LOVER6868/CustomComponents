#pragma once
#include "core/WeakObjectSettings.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "effects/SubsetPlacementPolicy.hpp"
#include "engine/CardTypes.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/SubsetSelection.hpp"
namespace CustomComponents::Effects::SubsetCreationExtensions {
using Obj=BNM::IL2CPP::Il2CppObject;
using namespace SubsetPlacement;
inline Core::WeakObjectSettings<Settings> descriptors,effects;
inline BNM::Class descriptorClass,runtimeClass;
// Exact stock value layouts, verified against this EngineLib build.
struct CreationData {int guid;bool faceDown,teamup,front;unsigned char padding;};
struct Placement {int card,lane,player,order;};
static_assert(sizeof(CreationData)==8 && sizeof(Placement)==16);
inline void Save(Core::WeakObjectSettings<Settings>& store,Obj* object,Settings value){store.Set(object,value,Enabled(value));}
inline void ReadMap(Obj* object,Obj* properties) {
    if(!object || !properties || object->klass!=descriptorClass.GetClass())return;
    namespace Bridge=Systems::CustomFieldBridgeSystem;
    Obj* team=Bridge::DictionaryValue(properties,"GrantTeamup");
    Obj* aquatic=Bridge::DictionaryValue(properties,"GrantAquatic");
    Obj* position=Bridge::DictionaryValue(properties,"CreationPosition");
    if(!team && !aquatic && !position)return; // A later stock-only pass must not erase captured options.
    const auto text=[](Obj* value){auto* s=value?Bridge::ObjectToString(value):nullptr;return s?s->str():std::string{};};
    const auto boolean=[&](Obj* value){const auto t=text(value);return t=="True" || t=="true" || t=="1";};
    Save(descriptors,object,{boolean(team),boolean(aquatic),Parse(text(position))});
}
inline void ReadSerialized(Obj* object,Obj* serialized) {
    if(!object || !serialized || object->klass!=descriptorClass.GetClass())return;
    namespace Bridge=Systems::CustomFieldBridgeSystem;
    auto convert=Bridge::convertPropertiesToDictionary;if(!convert.IsValid())return;
    Obj* properties=convert[static_cast<Obj*>(nullptr)](serialized);if(!properties)return;
    if(Obj* data=Bridge::DictionaryValue(properties,"$data"))
        if(Obj* nested=convert[static_cast<Obj*>(nullptr)](data))properties=nested;
    ReadMap(object,properties);
}
inline void ReadProperties(Obj* object,Obj* properties) {
    if(!object || !properties || object->klass!=descriptorClass.GetClass())return;
    ReadMap(object,properties);
    auto remove=BNM::Class(properties).GetMethod("Remove",1).cast<bool>();
    if(remove.IsValid())for(const char* key:{"GrantTeamup","GrantAquatic","CreationPosition"})remove[properties](BNM::CreateMonoString(key));
}
inline void(*originalDescriptorCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*originalDescriptorReturn)(Obj*,Obj*,const void*)=nullptr;
inline void(*originalEffectCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*originalEffectReturn)(Obj*,Obj*,const void*)=nullptr;
inline Obj*(*originalToEffect)(Obj*,Obj*,Obj*,const void*)=nullptr;
static void DescriptorCopy(Obj* self,Obj* source,Obj* instances,const void* method){auto value=descriptors.Get(source);originalDescriptorCopy(self,source,instances,method);Save(descriptors,self,value);}
static void DescriptorReturn(Obj* self,Obj* instances,const void* method){descriptors.Set(self,{},false);originalDescriptorReturn(self,instances,method);}
static void EffectCopy(Obj* self,Obj* source,Obj* instances,const void* method){auto value=effects.Get(source);originalEffectCopy(self,source,instances,method);Save(effects,self,value);}
static void EffectReturn(Obj* self,Obj* instances,const void* method){effects.Set(self,{},false);originalEffectReturn(self,instances,method);}
static Obj* ToEffect(Obj* self,Obj* instances,Obj* entity,const void* method){auto value=descriptors.Get(self);Obj* result=originalToEffect(self,instances,entity,method);Save(effects,result,value);return result;}
inline CreationData(*originalCreate)(Obj*,Obj*,const void*)=nullptr;
static CreationData Create(Obj* self,Obj* effect,const void* method) {
    auto data=originalCreate(self,effect,method);auto value=effects.Get(effect);
    data.teamup=data.teamup || value.teamup;
    if(value.position!=Position::Stock)data.front=value.position==Position::Front;
    return data;
}
inline BNM::Method<Obj*> getOrAdd;
inline BNM::Method<void> addCounter;
inline BNM::Field<int> effectEntityId;
inline BNM::MonoType* aquaticType=nullptr;
inline void(*originalPlace)(Obj*,Obj*,Obj*,Placement,CreationData,const void*)=nullptr;
static void Place(Obj* self,Obj* effect,Obj* card,Placement placement,CreationData data,const void* method) {
    const auto value=effects.Get(effect);
    if(Enabled(value) && Engine::CardTypes::Fighter(card)) {
        // Stock placement already grants Team-Up before checking legality.
        // Aquatic must likewise be active before its water-lane validator runs.
        if(value.aquatic)if(Obj* ability=getOrAdd[card](aquaticType))addCounter[ability](0,effectEntityId[effect].Get(),0);
        placement.order=Order(value.position,placement.order);
    }
    originalPlace(self,effect,card,placement,data,method);
}
inline void(*originalRecord)(Obj*,Obj*,CreationData,Obj*,const void*)=nullptr;
inline BNM::Field<Obj*> granterHistory;
inline BNM::Method<Obj*> addRecord;
inline BNM::Method<void> defaults,setTarget,setAbility,setValue;
inline BNM::MonoType* grantedRecordType=nullptr;
static void Record(Obj* self,Obj* card,CreationData data,Obj* effect,const void* method) {
    originalRecord(self,card,data,effect,method);
    if(!effects.Get(effect).aquatic || !Engine::CardTypes::Fighter(card))return;
    // Called by stock only after a successful placement; failed creations get no grant herald.
    Obj* history=granterHistory[self].Get();if(!history)return;
    Obj* record=addRecord[history](grantedRecordType);if(!record)return;
    auto id=Engine::EntityAccess::Api().id;
    defaults[record](effect);setTarget[record](id[card].Get());setAbility[record](11);setValue[record](0);
}
inline bool Initialize() {
    descriptorClass=BNM::Class("PvZCards.Engine.Components","CreateCardFromSubsetEffectDescriptor");
    runtimeClass=BNM::Class("PvZCards.Engine.Effects","CreateCardFromSubsetEffect");
    auto entity=BNM::Class("PvZCards.Engine","Entity");auto ability=BNM::Class("PvZCards.Engine.Components","GrantableAbility");
    getOrAdd=entity.GetMethod("GetOrAddComponent",1).cast<Obj*>();addCounter=ability.GetMethod("AddCounter",3).cast<void>();
    effectEntityId=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();
    aquaticType=BNM::Class("PvZCards.Engine.Components","Aquatic").GetMonoType();
    auto granter=BNM::Class("PvZCards.Core.Systems.Utilities","CreatedCardAbilityGranter");
    granterHistory=granter.GetField("<History>k__BackingField").cast<Obj*>();
    auto record=BNM::Class("PvZCards.Engine.Records","AbilityGrantedRecord");
    grantedRecordType=record.GetMonoType();addRecord=BNM::Class("PvZCards.Engine","History").GetMethod("AddRecord",1).cast<Obj*>();
    defaults=record.GetMethod("SetDefaultIdsFromEffect",1).cast<void>();setTarget=record.GetMethod("set_TargetId",1).cast<void>();
    setAbility=record.GetMethod("set_AbilityType",1).cast<void>();setValue=record.GetMethod("set_AbilityValue",1).cast<void>();
    if(!descriptorClass.IsValid() || !runtimeClass.IsValid() || !getOrAdd.IsValid() || !addCounter.IsValid() || !effectEntityId.IsValid() || !aquaticType || !granterHistory.IsValid() || !grantedRecordType || !addRecord.IsValid() || !defaults.IsValid() || !setTarget.IsValid() || !setAbility.IsValid() || !setValue.IsValid())return false;
    Engine::BoardPlacement::augment=[](Obj* entity,Engine::BoardPlacementPolicy::Card& traits){
        Obj* descriptor=Engine::EntityAccess::Component(entity,"CreateCardFromSubsetEffectDescriptor");const auto value=descriptors.Get(descriptor);traits.teamup|=value.teamup;traits.aquatic|=value.aquatic;
    };
    Engine::SubsetSelection::hasAquaticCreationGrant=[](Obj* effect){return effect && effect->klass==runtimeClass.GetClass() && effects.Get(effect).aquatic;};
    Systems::CustomFieldBridgeSystem::beforeProperties.Add(ReadProperties);
    Systems::CustomFieldBridgeSystem::afterDeserialize.Add(ReadSerialized);
    return Core::Hooks::Install(descriptorClass.GetMethod("DeepCopyFrom",2),DescriptorCopy,originalDescriptorCopy) &&
        Core::Hooks::Install(descriptorClass.GetMethod("HandleReturnToPool",1),DescriptorReturn,originalDescriptorReturn) &&
        Core::Hooks::Install(descriptorClass.GetMethod("ToEffect",2),ToEffect,originalToEffect) &&
        Core::Hooks::Install(runtimeClass.GetMethod("DeepCopyFrom",2),EffectCopy,originalEffectCopy) &&
        Core::Hooks::Install(runtimeClass.GetMethod("HandleReturnToPool",1),EffectReturn,originalEffectReturn) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","CardCreationDataFactory").GetMethod("Create",{BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","CreateCardFromSubsetEffect","EngineLib.dll").Build()}),Create,originalCreate) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","CreatedCardPlacer").GetMethod("PlaceOrDestroyCard",4),Place,originalPlace) &&
        Core::Hooks::Install(granter.GetMethod("RecordGrantedAbilities",3),Record,originalRecord);
}
}
