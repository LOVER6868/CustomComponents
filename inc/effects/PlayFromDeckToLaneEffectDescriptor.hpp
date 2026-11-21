#pragma once
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Class.hpp>
#include "core/ManagedQuery.hpp"

namespace CustomComponents::Components {
// Transfers the selected physical deck card. Optional SubsetQuery is only used
// by the PluckFailSafe fallback; when omitted, derive the fallback from the
// authored PrimaryTargetFilter.Query (with entity InDeckQuery constraints removed).
struct PlayFromDeckToLaneEffectDescriptor : BNM::IL2CPP::Il2CppObject {
    using Obj = BNM::IL2CPP::Il2CppObject;
    using Str = BNM::Structures::Mono::String;
    bool PluckFailSafe{};
    bool ForceFaceDown{};
    bool GrantTeamup{};
    bool GrantAquatic{};
    Str* CreationPosition{};
    Obj* SubsetQuery{};

    BNM_CustomClass(PlayFromDeckToLaneEffectDescriptor,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","PlayFromDeckToLaneEffectDescriptor","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(PluckFailSafe,BNM::Defaults::Get<bool>(),"PluckFailSafe");
    BNM_CustomField(ForceFaceDown,BNM::Defaults::Get<bool>(),"ForceFaceDown");
    BNM_CustomField(GrantTeamup,BNM::Defaults::Get<bool>(),"GrantTeamup");
    BNM_CustomField(GrantAquatic,BNM::Defaults::Get<bool>(),"GrantAquatic");
    BNM_CustomField(CreationPosition,BNM::Defaults::Get<Str*>(),"CreationPosition");
    BNM_CustomField(SubsetQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQuery","EngineLib.dll").Build(),"SubsetQuery");

    void DeepCopyFrom(Obj* source, Obj* instances) {
        BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);
        auto* other=static_cast<PlayFromDeckToLaneEffectDescriptor*>(source);
        PluckFailSafe=other && other->PluckFailSafe;
        ForceFaceDown=other && other->ForceFaceDown;
        GrantTeamup=other && other->GrantTeamup;
        GrantAquatic=other && other->GrantAquatic;
        Obj* position=other ? reinterpret_cast<Obj*>(other->CreationPosition) : nullptr;
        if(Core::ManagedRuntime::writeBarrier)
            Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&CreationPosition),position);
        else CreationPosition=reinterpret_cast<Str*>(position);
        // EntityQuery references are pooled and must be copied, not aliased.
        auto field=BNM::Class(this).GetField("SubsetQuery").cast<Obj*>();
        if(!Core::ManagedQuery::CopyOwnedField(this,field,other?other->SubsetQuery:nullptr,instances)) {
            PluckFailSafe=false; // Never use an invalid/stale authored query.
            LOG_ERR("CustomComponents/PlayFromDeckToLane: failed to clone optional fallback query");
        }
    }
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());

    void HandleReturnToPool(Obj* instances) {
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("SubsetQuery").cast<Obj*>(),instances);
        PluckFailSafe=false;
        ForceFaceDown=false;
        GrantTeamup=false;
        GrantAquatic=false;
        if(Core::ManagedRuntime::writeBarrier)
            Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&CreationPosition),nullptr);
        else CreationPosition=nullptr;
        BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);
    }
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
}
