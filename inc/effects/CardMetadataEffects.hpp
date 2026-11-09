#pragma once
#include "variables/Resolver.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/AbilityConversion.hpp"
#include "engine/SimulationChanges.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"

namespace CustomComponents::Effects::CardMetadata {
using Obj=BNM::IL2CPP::Il2CppObject;
using Str=BNM::Structures::Mono::String;
using K=Systems::CustomFieldBridgeSystem::FieldKind;
inline void Reference(Obj* obj,Str** field,Str* value){if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(obj,reinterpret_cast<void**>(field),value);else *field=value;}

struct AddTagEffect:Obj {
    unsigned char inherited[0x40]{};Str* Tag=nullptr;
    BNM_CustomClass(AddTagEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","AddTagEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","GrantAbilityEffect","EngineLib.dll").Build(),{});
    void Construct(){BNM_CallCustomMethodOrigin(Construct,this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomField(Tag,BNM::Defaults::Get<Str*>(),"Tag");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Reference(this,&Tag,src?reinterpret_cast<AddTagEffect*>(src)->Tag:nullptr);}
    void HandleReturnToPool(Obj* instances){Reference(this,&Tag,nullptr);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct AddTagEffectDescriptor:Obj {
    unsigned char inherited[0x10]{};Str* Tag=nullptr;
    BNM_CustomClass(AddTagEffectDescriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","AddTagEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantAbilityEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(Tag,BNM::Defaults::Get<Str*>(),"Tag");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Reference(this,&Tag,src?reinterpret_cast<AddTagEffectDescriptor*>(src)->Tag:nullptr);}
    void HandleReturnToPool(Obj* instances){Reference(this,&Tag,nullptr);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct AddSubtypeEffect:Obj {
    unsigned char inherited[0x40]{};int Subtype=-1;
    BNM_CustomClass(AddSubtypeEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","AddSubtypeEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","GrantAbilityEffect","EngineLib.dll").Build(),{});
    void Construct(){BNM_CallCustomMethodOrigin(Construct,this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomField(Subtype,BNM::Defaults::Get<int>(),"Subtype");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Subtype=src?reinterpret_cast<AddSubtypeEffect*>(src)->Subtype:-1;}
    void HandleReturnToPool(Obj* instances){Subtype=-1;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct AddSubtypeEffectDescriptor:Obj {
    unsigned char inherited[0x10]{};int Subtype=-1;
    BNM_CustomClass(AddSubtypeEffectDescriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","AddSubtypeEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantAbilityEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(Subtype,BNM::Defaults::Get<int>(),"Subtype");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Subtype=src?reinterpret_cast<AddSubtypeEffectDescriptor*>(src)->Subtype:-1;}
    void HandleReturnToPool(Obj* instances){Subtype=-1;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};

struct RemoveTagEffect:Obj {
    unsigned char inherited[0x40]{};Str* Tag=nullptr;
    BNM_CustomClass(RemoveTagEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","RemoveTagEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","GrantAbilityEffect","EngineLib.dll").Build(),{});
    void Construct(){BNM_CallCustomMethodOrigin(Construct,this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomField(Tag,BNM::Defaults::Get<Str*>(),"Tag");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Reference(this,&Tag,src?reinterpret_cast<RemoveTagEffect*>(src)->Tag:nullptr);}
    void HandleReturnToPool(Obj* instances){Reference(this,&Tag,nullptr);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct RemoveTagEffectDescriptor:Obj {
    unsigned char inherited[0x10]{};Str* Tag=nullptr;
    BNM_CustomClass(RemoveTagEffectDescriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","RemoveTagEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantAbilityEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(Tag,BNM::Defaults::Get<Str*>(),"Tag");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Reference(this,&Tag,src?reinterpret_cast<RemoveTagEffectDescriptor*>(src)->Tag:nullptr);}
    void HandleReturnToPool(Obj* instances){Reference(this,&Tag,nullptr);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct RemoveSubtypeEffect:Obj {
    unsigned char inherited[0x40]{};int Subtype=-1;
    BNM_CustomClass(RemoveSubtypeEffect,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","RemoveSubtypeEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","GrantAbilityEffect","EngineLib.dll").Build(),{});
    void Construct(){BNM_CallCustomMethodOrigin(Construct,this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomField(Subtype,BNM::Defaults::Get<int>(),"Subtype");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Subtype=src?reinterpret_cast<RemoveSubtypeEffect*>(src)->Subtype:-1;}
    void HandleReturnToPool(Obj* instances){Subtype=-1;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct RemoveSubtypeEffectDescriptor:Obj {
    unsigned char inherited[0x10]{};int Subtype=-1;
    BNM_CustomClass(RemoveSubtypeEffectDescriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","RemoveSubtypeEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantAbilityEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(Subtype,BNM::Defaults::Get<int>(),"Subtype");
    void DeepCopyFrom(Obj* src,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,src,instances);Subtype=src?reinterpret_cast<RemoveSubtypeEffectDescriptor*>(src)->Subtype:-1;}
    void HandleReturnToPool(Obj* instances){Subtype=-1;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};

inline bool AppendTag(Obj* card,Str* tag){
    if(!card || !tag)return false;
    std::string value=tag->str();if(value.empty() || value.size()>128)return false;
    for(const auto& previous:Engine::Match::Tags(card))if(previous==value)return true;
    auto type=BNM::Class("PvZCards.Engine.Components","Tags");
    auto* tags=Engine::Match::Component(card,type.GetMonoType(),true);
    auto field=type.GetField("TagList").cast<Obj*>();
    Obj* list=tags && field.IsValid()?field[tags].Get():nullptr;
    auto add=list?BNM::Class(list).GetMethod("Add",1).cast<void>():BNM::Method<void>{};
    if(!list || !add.IsValid())return false;
    auto error=BNM::TryInvoke([&]{add[list](BNM::CreateMonoString(value));});
    return !error.IsValid();
}
inline bool AppendSubtype(Obj* card,int subtype){
    if(!card || subtype<0)return false;
    auto type=BNM::Class("PvZCards.Engine.Components","Subtypes");
    if(!type.IsValid())return false;
    Obj* component=Engine::Match::Component(card,type.GetMonoType(),true);
    if(!component)return false;
    // The serialized stock name is 'subtypes'. Resolve its actual field at
    // runtime, rather than assuming serializer casing or modifying an array.
    BNM::Field<Obj*> listField;
    for(const char* name:{"subtypes","SubtypeList","Subtypes"}){
        listField=type.GetField(name).cast<Obj*>();if(listField.IsValid())break;
    }
    Obj* list=listField.IsValid()?listField[component].Get():nullptr;
    if(!list)return false;
    auto contains=BNM::Class(list).GetMethod("Contains",1).cast<bool>();
    auto add=BNM::Class(list).GetMethod("Add",1).cast<void>();
    if(!contains.IsValid() || !add.IsValid())return false;
    bool success=false;
    auto error=BNM::TryInvoke([&]{if(!contains[list](subtype))add[list](subtype);success=true;});
    return !error.IsValid() && success;
}
// Remove the requested managed list entries instead of mutating the live
// component layout directly.  No component is created on removal.
// Reverse RemoveAt preserves list iteration validity and removes duplicates
// that may have been supplied by unmodified card data.
inline bool RemoveTag(Obj* card,Str* tag){
    if(!card || !tag)return false;
    const std::string value=tag->str();
    if(value.empty() || value.size()>128)return false;
    Obj* component=Engine::EntityAccess::Component(card,"Tags");
    if(!component)return true;  // absent tag is already removed
    auto type=BNM::Class("PvZCards.Engine.Components","Tags");
    auto field=type.GetField("TagList").cast<Obj*>();
    Obj* list=field.IsValid()?field[component].Get():nullptr;
    auto* values=reinterpret_cast<BNM::Structures::Mono::List<Str*>*>(list);
    if(!values || !values->items || values->size<0 || values->size>int(values->items->capacity))return false;
    auto removeAt=BNM::Class(list).GetMethod("RemoveAt",1).cast<void>();
    if(!removeAt.IsValid())return false;
    auto error=BNM::TryInvoke([&]{
        for(int i=values->size-1;i>=0;--i){
            Str* candidate=values->items->m_Items[i];
            if(candidate && candidate->str()==value)removeAt[list](i);
        }
    });
    return !error.IsValid();
}
inline bool RemoveSubtype(Obj* card,int subtype){
    if(!card || subtype<0)return false;
    Obj* component=Engine::EntityAccess::Component(card,"Subtypes");
    if(!component)return true;  // absent subtype is already removed
    auto type=BNM::Class("PvZCards.Engine.Components","Subtypes");
    if(!type.IsValid())return false;
    BNM::Field<Obj*> field;
    for(const char* name:{"subtypes","SubtypeList","Subtypes"}){
        field=type.GetField(name).cast<Obj*>();
        if(field.IsValid())break;
    }
    Obj* list=field.IsValid()?field[component].Get():nullptr;
    auto* values=reinterpret_cast<BNM::Structures::Mono::List<int>*>(list);
    if(!values || !values->items || values->size<0 || values->size>int(values->items->capacity))return false;
    auto removeAt=BNM::Class(list).GetMethod("RemoveAt",1).cast<void>();
    if(!removeAt.IsValid())return false;
    auto error=BNM::TryInvoke([&]{
        for(int i=values->size-1;i>=0;--i){
            if(values->items->m_Items[i]==subtype)removeAt[list](i);
        }
    });
    return !error.IsValid();
}

inline bool Convert(Obj* descriptor,Obj* instances,Obj*,Obj*& result){
    if(!descriptor || !descriptor->klass || !descriptor->klass->name)return false;
    const std::string_view name=descriptor->klass->name;
    const bool addTag=name=="AddTagEffectDescriptor";
    const bool removeTag=name=="RemoveTagEffectDescriptor";
    const bool addSubtype=name=="AddSubtypeEffectDescriptor";
    const bool removeSubtype=name=="RemoveSubtypeEffectDescriptor";
    if(!addTag && !removeTag && !addSubtype && !removeSubtype)return false;
    const char* runtime=addTag?"AddTagEffect":removeTag?"RemoveTagEffect":addSubtype?"AddSubtypeEffect":"RemoveSubtypeEffect";
    result=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects",runtime));
    if(!result)return true;
    if(addTag)Reference(result,&reinterpret_cast<AddTagEffect*>(result)->Tag,reinterpret_cast<AddTagEffectDescriptor*>(descriptor)->Tag);
    else if(removeTag)Reference(result,&reinterpret_cast<RemoveTagEffect*>(result)->Tag,reinterpret_cast<RemoveTagEffectDescriptor*>(descriptor)->Tag);
    else if(addSubtype)reinterpret_cast<AddSubtypeEffect*>(result)->Subtype=reinterpret_cast<AddSubtypeEffectDescriptor*>(descriptor)->Subtype;
    else reinterpret_cast<RemoveSubtypeEffect*>(result)->Subtype=reinterpret_cast<RemoveSubtypeEffectDescriptor*>(descriptor)->Subtype;
    return true;
}

inline bool Prepare(const Engine::EffectGuards::Context& c){
    if(!c.effect || !c.effect->klass || !c.effect->klass->name)return true;
    const std::string_view name=c.effect->klass->name;
    const bool addTag=name=="AddTagEffect";
    const bool removeTag=name=="RemoveTagEffect";
    const bool addSubtype=name=="AddSubtypeEffect";
    const bool removeSubtype=name=="RemoveSubtypeEffect";
    if(!addTag && !removeTag && !addSubtype && !removeSubtype)return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(c.router));
    int targetId=Variables::Field(c.effect,"TargetId");
    Obj* target=Engine::EntityAccess::Find(model,targetId);
    if(!target){LOG_WARN("CustomComponents/CardMetadata: missing target id=%d",targetId);return false;}
    bool ok=addTag?AppendTag(target,reinterpret_cast<AddTagEffect*>(c.effect)->Tag):
        removeTag?RemoveTag(target,reinterpret_cast<RemoveTagEffect*>(c.effect)->Tag):
        addSubtype?AppendSubtype(target,reinterpret_cast<AddSubtypeEffect*>(c.effect)->Subtype):
        RemoveSubtype(target,reinterpret_cast<RemoveSubtypeEffect*>(c.effect)->Subtype);
    if(ok)Engine::SimulationChanges::Changed(model);
    else LOG_WARN("CustomComponents/CardMetadata: failed to apply %s target=%d",name.data(),targetId);
    return false; // Custom effect handled; never dispatch as stock GrantAbilityEffect.
}

inline bool Initialize(){
    for(auto config:{std::pair{"AddTagEffect","AddTagEffectDescriptor"},std::pair{"RemoveTagEffect","RemoveTagEffectDescriptor"},std::pair{"AddSubtypeEffect","AddSubtypeEffectDescriptor"},std::pair{"RemoveSubtypeEffect","RemoveSubtypeEffectDescriptor"}}){
        const auto runtime=BNM::Class("PvZCards.Engine.Effects",config.first);
        const auto descriptor=BNM::Class("PvZCards.Engine.Components",config.second);
        if(!CustomComponentRegistry::Register({config.first,"PvZCards.Engine.Effects","EngineLib.dll","GrantAbilityEffect","PvZCards.Engine.Effects","EngineLib.dll"}) ||
           !CustomComponentRegistry::Register({config.second,"PvZCards.Engine.Components","EngineLib.dll","GrantAbilityEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}) ||
           !Core::ComponentLifecycle::Patch(runtime,BNM::Class("PvZCards.Engine.Effects","GrantAbilityEffect")) ||
           !Core::ComponentLifecycle::Patch(descriptor,BNM::Class("PvZCards.Engine.Components","GrantAbilityEffectDescriptor")))return false;
    }
    return Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","AddTagEffectDescriptor","PvZCards.Engine.Components","GrantAbilityEffectDescriptor",{{"Tag",K::String}},false) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Effects","AddTagEffect","PvZCards.Engine.Effects","GrantAbilityEffect",{{"Tag",K::String}},false) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","RemoveTagEffectDescriptor","PvZCards.Engine.Components","GrantAbilityEffectDescriptor",{{"Tag",K::String}},false) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Effects","RemoveTagEffect","PvZCards.Engine.Effects","GrantAbilityEffect",{{"Tag",K::String}},false) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","AddSubtypeEffectDescriptor","PvZCards.Engine.Components","GrantAbilityEffectDescriptor",{{"Subtype",K::Int32}},false) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Effects","AddSubtypeEffect","PvZCards.Engine.Effects","GrantAbilityEffect",{{"Subtype",K::Int32}},false) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","RemoveSubtypeEffectDescriptor","PvZCards.Engine.Components","GrantAbilityEffectDescriptor",{{"Subtype",K::Int32}},false) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Effects","RemoveSubtypeEffect","PvZCards.Engine.Effects","GrantAbilityEffect",{{"Subtype",K::Int32}},false) &&
        Engine::AbilityConversion::Add(Convert) && Engine::EffectGuards::RegisterPreparation(Prepare,-28);
}
}
