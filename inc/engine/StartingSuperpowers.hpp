#pragma once
#include <BNM/ClassesManagement.hpp>
#include "engine/MatchAccess.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/CardPoolAccess.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/ManagedReference.hpp"
#include "core/HookRegistry.hpp"
#include "core/NativeConstruction.hpp"
#include <charconv>

namespace CustomComponents::Engine::StartingSuperpowers {
using Obj=Match::Obj;
using Str=BNM::Structures::Mono::String;
struct Snapshot : Obj {
    Str* Guids=nullptr;
    BNM_CustomClass(Snapshot,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","StartingSuperpowers","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(Guids,BNM::Defaults::Get<Str*>(),"Guids");
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    void Assign(Str* value){if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&Guids),value);else Guids=value;}
    void DeepCopyFrom(Obj* source,Obj*){Assign(source?reinterpret_cast<Snapshot*>(source)->Guids:nullptr);}
    void HandleReturnToPool(Obj*){Assign(nullptr);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline BNM::Field<Obj*> initializerModel;
inline BNM::Field<EntityAccess::List*> entries;
inline BNM::Field<int> entryGuid,entryId;
inline void (*originalInitialize)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void Capture(Obj* model,Obj* pool){
    if(!pool || EntityAccess::Component(pool,"StartingSuperpowers"))return;
    Obj* ordered=EntityAccess::Component(pool,"OrderedCardPool");
    const auto* list=ordered?Core::Metadata::Bind(entries,ordered).Get():nullptr;
    if(!EntityAccess::Valid(list))return;
    std::string guids;
    for(int i=0;i<list->size;++i){Obj* entry=list->items->m_Items[i];if(!entry)continue;
        int guid=Core::Metadata::Bind(entryGuid,entry).Get();
        if(guid<=0)guid=BoardPlacement::Guid(EntityAccess::Find(model,Core::Metadata::Bind(entryId,entry).Get()));
        if(guid<=0)continue;
        if(!guids.empty())guids+=',';guids+=std::to_string(guid);
    }
    auto* state=reinterpret_cast<Snapshot*>(Match::Component(pool,BNM::Class("PvZCards.Engine.Components","StartingSuperpowers").GetMonoType(),true));
    if(state)state->Assign(BNM::CreateMonoString(guids));
}
static void Initialized(Obj* system,Obj* data,Obj* player,const void* method){
    originalInitialize(system,data,player,method);
    Obj* model=Core::Metadata::Bind(initializerModel,system).Get();
    Capture(model,CardPoolAccess::Get(model,Match::Id(player),true));
}
inline bool Matches(Obj* candidate,Obj* context){
    Obj* model=BoardPlacement::Model(context);const int guid=BoardPlacement::Guid(candidate);
    if(!model || guid<=0)return false;
    const auto api=EntityAccess::Api();const auto* entities=Core::Metadata::Bind(api.entities,model).Get();if(!EntityAccess::Valid(entities))return false;
    for(int i=0;i<entities->size;++i){
        auto* state=reinterpret_cast<Snapshot*>(EntityAccess::Component(entities->items->m_Items[i],"StartingSuperpowers"));
        if(!state || !state->Guids)continue;
        const auto text=state->Guids->str();std::size_t begin=0;
        while(begin<text.size()){
            const auto end=text.find(',',begin);const auto limit=end==std::string::npos?text.size():end;int value=-1;
            const auto parsed=std::from_chars(text.data()+begin,text.data()+limit,value);
            if(parsed.ec==std::errc{} && parsed.ptr==text.data()+limit && value==guid)return true;
            begin=limit+1;
        }
    }
    return false;
}
inline bool Initialize(){
    const auto snapshot=BNM::Class("PvZCards.Engine.Components","StartingSuperpowers");
    const auto system=BNM::Class("PvZCards.Core.Utilities","SuperpowerInitializer");
    initializerModel=system.GetField("EntityModel").cast<Obj*>();
    entries=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<EntityAccess::List*>();
    const auto entry=BNM::Class("PvZCards.Engine.Components.Data","CardPoolEntry");entryGuid=entry.GetField("Guid").cast<int>();entryId=entry.GetField("EntityId").cast<int>();
    return CardPoolAccess::Initialize() && snapshot.IsValid() && initializerModel.IsValid() && entries.IsValid() && entryGuid.IsValid() && entryId.IsValid() &&
        CustomComponentRegistry::Register({"StartingSuperpowers","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(snapshot,BNM::Class("PvZCards.Engine.Components","MarkerComponent")) &&
        Core::Hooks::Install(system.GetMethod("InitializeSuperpowers",2),Initialized,originalInitialize);
}
}
