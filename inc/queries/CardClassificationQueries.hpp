#pragma once

#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>

#include "queries/CardClassificationPolicy.hpp"
#include "queries/InDeckQuery.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedRuntime.hpp"
#include "engine/BoardPlacement.hpp"
#include "presentation/detail/Pick3CardMetadata.hpp"

namespace CustomComponents::Queries {
namespace Systems = CustomComponents::Systems;

using ClassificationString = BNM::Structures::Mono::String;

inline void CopyClassificationString(Obj* owner, ClassificationString** slot, ClassificationString* value) {
    if (Core::ManagedRuntime::writeBarrier)
        Core::ManagedRuntime::writeBarrier(owner, reinterpret_cast<void**>(slot), value);
    else *slot = value;
}

// Reads only the live Rarity component. No CardData.Rarity fallback.
struct HasRarityQuery : Obj {
    ClassificationString* Rarity = nullptr;
    BNM_CustomClass(HasRarityQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "HasRarityQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomField(Rarity, BNM::Defaults::Get<ClassificationString*>(), "Rarity");

    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0).cast<void>();
        if (ctor.IsValid()) ctor[this]();
        Rarity = nullptr;
    }
    bool Matches(Obj* candidate, Obj*) {
        if (!Rarity || !candidate) return false;
        Obj* component = Engine::EntityAccess::Component(candidate, "Rarity");
        if (!component) return false;
        static auto rarityValue = BNM::Class("PvZCards.Engine.Components", "Rarity").GetField("Value").cast<int>();
        return rarityValue.IsValid() &&
               CardClassificationPolicy::HasRarity(rarityValue[component].Get(), Rarity->str());
    }
    void DeepCopyFrom(Obj* source, Obj*) {
        auto* from = reinterpret_cast<HasRarityQuery*>(source);
        CopyClassificationString(this, &Rarity, from ? from->Rarity : nullptr);
    }
    void HandleReturnToPool(Obj*) { CopyClassificationString(this, &Rarity, nullptr); }
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};

// Reads card_data's top-level `set` metadata, keyed by the candidate's Card.Guid.
// Deliberately does NOT test Tags, Subtypes or a nonexistent "Set" component.
struct HasSetQuery : Obj {
    ClassificationString* Set = nullptr;
    BNM_CustomClass(HasSetQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "HasSetQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomField(Set, BNM::Defaults::Get<ClassificationString*>(), "Set");

    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0).cast<void>();
        if (ctor.IsValid()) ctor[this]();
        Set = nullptr;
    }
    bool Matches(Obj* candidate, Obj*) {
        if (!Set || !candidate) return false;
        const int guid = Engine::BoardPlacement::Guid(candidate);
        if (guid <= 0) return false;
        return Presentation::Pick3CardMetadata::HasCapturedSet(guid, Set->str());
    }
    void DeepCopyFrom(Obj* source, Obj*) {
        auto* from = reinterpret_cast<HasSetQuery*>(source);
        CopyClassificationString(this, &Set, from ? from->Set : nullptr);
    }
    void HandleReturnToPool(Obj*) { CopyClassificationString(this, &Set, nullptr); }
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};

// Checks the card definition's CardData.Color flag. Works with PickFromThree
// database candidates; class values are captured during stock SetCardData.
struct ClassQuery : Obj {
    ClassificationString* Class = nullptr;
    BNM_CustomClass(ClassQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "ClassQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomField(Class, BNM::Defaults::Get<ClassificationString*>(), "Class");
    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0).cast<void>();
        if (ctor.IsValid()) ctor[this]();
        Class = nullptr;
    }
    bool Matches(Obj* candidate, Obj*) {
        if (!Class || !candidate) return false;
        const auto flag = CardClassificationPolicy::ParseClass(Class->str());
        if (!flag) return false;
        return Presentation::Pick3CardMetadata::HasCapturedClass(Engine::BoardPlacement::Guid(candidate), *flag);
    }
    void DeepCopyFrom(Obj* source, Obj*) {
        auto* from = reinterpret_cast<ClassQuery*>(source);
        CopyClassificationString(this, &Class, from ? from->Class : nullptr);
    }
    void HandleReturnToPool(Obj*) { CopyClassificationString(this, &Class, nullptr); }
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};

// Matches a live card carrying an active instance of a particular granted
// triggered ability. This checks GrantedTriggeredAbilities.Counters, not the
// card's printed abilities, tags, or ability-entity GUIDs.
struct HasGrantedTriggeredAbilityQuery : Obj {
    int AbilityGuid = 0;
    BNM_CustomClass(HasGrantedTriggeredAbilityQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "HasGrantedTriggeredAbilityQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomField(AbilityGuid, BNM::Defaults::Get<int>(), "AbilityGuid");

    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0).cast<void>();
        if (ctor.IsValid()) ctor[this]();
        AbilityGuid = 0;
    }
    bool Matches(Obj* candidate, Obj*) {
        if (!candidate || AbilityGuid <= 0) return false;
        Obj* grants = Engine::EntityAccess::Component(candidate, "GrantedTriggeredAbilities");
        if (!grants) return false;
        static auto counters = BNM::Class("PvZCards.Engine.Components", "GrantedTriggeredAbilities")
            .GetField("Counters").cast<BNM::Structures::Mono::List<Obj*>*>();
        static auto guid = BNM::Class("PvZCards.Engine.Components", "AbilityCounter")
            .GetField("Guid").cast<int>();
        static auto active = BNM::Class("PvZCards.Engine.Components", "AbilityCounter")
            .GetMethod("IsActive", 0).cast<bool>();
        if (!counters.IsValid() || !guid.IsValid() || !active.IsValid()) return false;
        auto* list = counters[grants].Get();
        if (!list || !list->items) return false;
        for (int i = 0; i < list->size; ++i) {
            Obj* counter = list->items->m_Items[i];
            if (counter && guid[counter].Get() == AbilityGuid && active[counter]()) return true;
        }
        return false;
    }
    void DeepCopyFrom(Obj* source, Obj*) {
        AbilityGuid = source ? reinterpret_cast<HasGrantedTriggeredAbilityQuery*>(source)->AbilityGuid : 0;
    }
    void HandleReturnToPool(Obj*) { AbilityGuid = 0; }
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};

inline bool InitializeCardClassificationQueries() {
    const auto base = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
    const auto rarity = BNM::Class("PvZCards.Engine.Queries", "HasRarityQuery");
    const auto set = BNM::Class("PvZCards.Engine.Queries", "HasSetQuery");
    const auto cls = BNM::Class("PvZCards.Engine.Queries", "ClassQuery");
    const auto granted = BNM::Class("PvZCards.Engine.Queries", "HasGrantedTriggeredAbilityQuery");
    using K = Systems::CustomFieldBridgeSystem::FieldKind;
    // Install the one existing SetCardData detour early, even when Pick3 UI is disabled.
    // The presentation layer reuses this hook later and never installs a second owner.
    if (!Presentation::Pick3CardMetadata::EnsureSetCapture()) return false;
    return PatchInDeckQueryDispatch(rarity, base, "HasRarityQuery") &&
        Core::ComponentLifecycle::Patch(rarity, base) &&
        RegisterQuery({"HasRarityQuery", "PvZCards.Engine.Queries.HasRarityQuery",
            "PvZCards.Engine.Queries", "HasRarityQuery", "PvZCards.Engine.Queries", "InHandQuery"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries", "HasRarityQuery",
            "PvZCards.Engine.Queries", "InHandQuery", {{"Rarity", K::String}}, false) &&
        PatchInDeckQueryDispatch(set, base, "HasSetQuery") &&
        Core::ComponentLifecycle::Patch(set, base) &&
        RegisterQuery({"HasSetQuery", "PvZCards.Engine.Queries.HasSetQuery",
            "PvZCards.Engine.Queries", "HasSetQuery", "PvZCards.Engine.Queries", "InHandQuery"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries", "HasSetQuery",
            "PvZCards.Engine.Queries", "InHandQuery", {{"Set", K::String}}, false) &&
        PatchInDeckQueryDispatch(cls, base, "ClassQuery") &&
        Core::ComponentLifecycle::Patch(cls, base) &&
        RegisterQuery({"ClassQuery", "PvZCards.Engine.Queries.ClassQuery",
            "PvZCards.Engine.Queries", "ClassQuery", "PvZCards.Engine.Queries", "InHandQuery"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries", "ClassQuery",
            "PvZCards.Engine.Queries", "InHandQuery", {{"Class", K::String}}, false) &&
        PatchInDeckQueryDispatch(granted, base, "HasGrantedTriggeredAbilityQuery") &&
        Core::ComponentLifecycle::Patch(granted, base) &&
        RegisterQuery({"HasGrantedTriggeredAbilityQuery", "PvZCards.Engine.Queries.HasGrantedTriggeredAbilityQuery",
            "PvZCards.Engine.Queries", "HasGrantedTriggeredAbilityQuery", "PvZCards.Engine.Queries", "InHandQuery"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries", "HasGrantedTriggeredAbilityQuery",
            "PvZCards.Engine.Queries", "InHandQuery", {{"AbilityGuid", K::Int32}}, false);
}

} // namespace CustomComponents::Queries
