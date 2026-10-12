#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>

#include "queries/QueryRegistry.hpp"
#include "log.h"

namespace CustomComponents::Queries {

using MonoObjList = BNM::Structures::Mono::List<Obj*>;

inline BNM::Field<int> inDeckEntityId;
inline BNM::Field<MonoObjList*> inDeckModelEntities;
inline BNM::Field<MonoObjList*> inDeckEntityComponents;
inline BNM::Field<MonoObjList*> inDeckOrderedPoolEntries;
inline BNM::Field<int> inDeckPoolEntryEntityId;
inline BNM::Field<int> inDeckPoolEntryGuid;
inline BNM::Field<int> inDeckCardGuid;
inline BNM::Field<Obj*> inDeckContextModel;
inline BNM::IL2CPP::Il2CppClass* inDeckOrderedPoolClass = nullptr;
inline BNM::IL2CPP::Il2CppClass* inDeckCardClass = nullptr;

template <typename T>
inline T* InDeckListAt(BNM::Structures::Mono::List<T*>* list, int index) {
    if (!list || !list->items || index < 0 || index >= list->size)
        return nullptr;
    return list->items->m_Items[index];
}

inline int InDeckCardGuidOf(Obj* entity) {
    if (!entity || !inDeckEntityComponents.IsValid() ||
        !inDeckCardGuid.IsValid() || !inDeckCardClass)
        return -1;

    MonoObjList* components = inDeckEntityComponents[entity].Get();
    if (!components || !components->items)
        return -1;

    for (int i = 0; i < components->size; ++i) {
        Obj* component = InDeckListAt(components, i);
        if (!component || component->klass != inDeckCardClass)
            continue;
        return inDeckCardGuid[component].Get();
    }

    return -1;
}

inline bool InitializeInDeckMembershipMetadata() {
    auto entity = BNM::Class("PvZCards.Engine", "Entity");
    auto model = BNM::Class("PvZCards.Engine", "EntityModel");
    auto context = BNM::Class("PvZCards.Engine", "EntityQueryContext");
    auto orderedPool = BNM::Class("PvZCards.Engine.Components", "OrderedCardPool");
    auto poolEntry = BNM::Class("PvZCards.Engine.Components.Data", "CardPoolEntry");
    auto card = BNM::Class("PvZCards.Engine.Components", "Card");

    if (!entity.IsValid() || !model.IsValid() || !context.IsValid() ||
        !orderedPool.IsValid() || !poolEntry.IsValid() || !card.IsValid()) {
        LOG_ERR("CustomComponents/InDeckQuery: membership classes missing");
        return false;
    }

    inDeckEntityId = entity.GetField("<Id>k__BackingField").cast<int>();
    inDeckModelEntities = model.GetField("Entities").cast<MonoObjList*>();
    inDeckEntityComponents = entity.GetField("Components").cast<MonoObjList*>();
    inDeckOrderedPoolEntries = orderedPool.GetField("Entries").cast<MonoObjList*>();
    inDeckPoolEntryEntityId = poolEntry.GetField("EntityId").cast<int>();
    inDeckPoolEntryGuid = poolEntry.GetField("Guid").cast<int>();
    inDeckCardGuid = card.GetField("Guid").cast<int>();
    inDeckContextModel = context.GetField("<Model>k__BackingField").cast<Obj*>();
    inDeckOrderedPoolClass = orderedPool.GetClass();
    inDeckCardClass = card.GetClass();

    if (!inDeckEntityId.IsValid() ||
        !inDeckModelEntities.IsValid() ||
        !inDeckEntityComponents.IsValid() ||
        !inDeckOrderedPoolEntries.IsValid() ||
        !inDeckPoolEntryEntityId.IsValid() ||
        !inDeckPoolEntryGuid.IsValid() ||
        !inDeckCardGuid.IsValid() ||
        !inDeckContextModel.IsValid() ||
        !inDeckOrderedPoolClass ||
        !inDeckCardClass) {
        LOG_ERR(
            "CustomComponents/InDeckQuery: membership API missing id=%d entities=%d components=%d entries=%d entryId=%d entryGuid=%d cardGuid=%d contextModel=%d poolClass=%p cardClass=%p",
            inDeckEntityId.IsValid(),
            inDeckModelEntities.IsValid(),
            inDeckEntityComponents.IsValid(),
            inDeckOrderedPoolEntries.IsValid(),
            inDeckPoolEntryEntityId.IsValid(),
            inDeckPoolEntryGuid.IsValid(),
            inDeckCardGuid.IsValid(),
            inDeckContextModel.IsValid(),
            inDeckOrderedPoolClass,
            inDeckCardClass
        );
        return false;
    }

    return true;
}

// JSON-visible IL2CPP query. We inherit the stateless stock InHandQuery solely
// to reuse its proven EntityQuery / Poolable / DeepCopyable lifecycle.
//
// Managed shape:
//   PvZCards.Engine.Queries.InDeckQuery : PvZCards.Engine.Queries.InHandQuery
//
// InHandQuery provides the stock EntityQuery / Poolable / DeepCopyable lifecycle.
// This subclass overrides Matches() with real entity-backed deck membership.
// Phase 0 keeps main-deck CardPoolEntry.EntityId values materialized, so membership
// is a direct Entity.Id == CardPoolEntry.EntityId test.
struct InDeckQuery final : BNM::IL2CPP::Il2CppObject {
    // Real public parameterless IL2CPP constructor. InstanceManager's stock
    // factory uses Activator.CreateInstance(Type), so the custom runtime type
    // must expose its own .ctor instead of relying only on manual allocation.
    void Constructor() {
        auto baseClass = BNM::Class(
            "PvZCards.Engine.Queries", "InHandQuery"
        );
        auto baseCtor = baseClass.GetMethod(".ctor", 0);
        if (!baseCtor.IsValid()) {
            LOG_ERR("CustomComponents/InDeckQuery: .ctor could not resolve InHandQuery::.ctor");
            return;
        }

        baseCtor.cast<void>()[this]();
    }

    bool Matches(Obj* entity, Obj* context) {
        if (!entity || !context)
            return false;

        const int candidateId = inDeckEntityId[entity].Get();
        if (candidateId <= 0)
            return false;

        Obj* model = inDeckContextModel[context].Get();
        if (!model)
            return false;

        MonoObjList* entities = inDeckModelEntities[model].Get();
        if (!entities || !entities->items)
            return false;

        // A card is in deck iff its live Entity.Id occurs in an OrderedCardPool
        // CardPoolEntry.EntityId. Phase 0 guarantees main-deck entries remain
        // materialized through mulligan and normal gameplay.
        for (int e = 0; e < entities->size; ++e) {
            Obj* poolOwner = InDeckListAt(entities, e);
            if (!poolOwner)
                continue;

            MonoObjList* components = inDeckEntityComponents[poolOwner].Get();
            if (!components || !components->items)
                continue;

            for (int c = 0; c < components->size; ++c) {
                Obj* component = InDeckListAt(components, c);
                if (!component || component->klass != inDeckOrderedPoolClass)
                    continue;

                MonoObjList* entries = inDeckOrderedPoolEntries[component].Get();
                if (!entries || !entries->items)
                    continue;

                for (int i = 0; i < entries->size; ++i) {
                    Obj* entry = InDeckListAt(entries, i);
                    if (!entry)
                        continue;

                    if (inDeckPoolEntryEntityId[entry].Get() == candidateId)
                        return true;
                }
            }
        }

        return false;
    }

    BNM_CustomClass(
        InDeckQuery,
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries", "InDeckQuery", "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll"
        ).Build(),
        {}
    );

    BNM_CustomMethod(
        Constructor,
        false,
        BNM::Defaults::Get<void>(),
        ".ctor"
    );

    BNM_CustomMethod(
        Matches,
        false,
        BNM::Defaults::Get<bool>(),
        "Matches",
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine", "Entity", "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine", "EntityQueryContext", "EngineLib.dll"
        ).Build()
    );

    // InHandQuery::Matches is the concrete implementation behind the
    // EntityQuery interface slot. BNM normally requires exact Il2CppClass
    // identity for every parameter before replacing an inherited vtable entry.
    // The game assemblies can expose equivalent parameter types through
    // metadata objects that do not compare pointer-identical here, leaving the
    // inherited InHandQuery slot untouched. Match by name + arity instead.
    BNM_CustomMethodSkipTypeMatch(Matches);
};


// CardDatabase-facing variant.
// Tests whether the candidate card definition's Card.Guid is represented in
// any current OrderedCardPool entry. This intentionally does NOT use Entity.Id.
struct InDeckGuidQuery final : BNM::IL2CPP::Il2CppObject {
    void Constructor() {
        auto baseClass = BNM::Class(
            "PvZCards.Engine.Queries", "InHandQuery"
        );
        auto baseCtor = baseClass.GetMethod(".ctor", 0);
        if (!baseCtor.IsValid()) {
            LOG_ERR("CustomComponents/InDeckGuidQuery: .ctor could not resolve InHandQuery::.ctor");
            return;
        }
        baseCtor.cast<void>()[this]();
    }

    bool Matches(Obj* entity, Obj* context) {
        if (!entity || !context)
            return false;

        const int candidateGuid = InDeckCardGuidOf(entity);
        if (candidateGuid <= 0)
            return false;

        Obj* model = inDeckContextModel[context].Get();
        if (!model)
            return false;

        MonoObjList* entities = inDeckModelEntities[model].Get();
        if (!entities || !entities->items)
            return false;

        for (int e = 0; e < entities->size; ++e) {
            Obj* poolOwner = InDeckListAt(entities, e);
            if (!poolOwner)
                continue;

            MonoObjList* components = inDeckEntityComponents[poolOwner].Get();
            if (!components || !components->items)
                continue;

            for (int c = 0; c < components->size; ++c) {
                Obj* component = InDeckListAt(components, c);
                if (!component || component->klass != inDeckOrderedPoolClass)
                    continue;

                MonoObjList* entries = inDeckOrderedPoolEntries[component].Get();
                if (!entries || !entries->items)
                    continue;

                for (int i = 0; i < entries->size; ++i) {
                    Obj* entry = InDeckListAt(entries, i);
                    if (!entry)
                        continue;

                    if (inDeckPoolEntryGuid[entry].Get() == candidateGuid)
                        return true;
                }
            }
        }

        return false;
    }

    BNM_CustomClass(
        InDeckGuidQuery,
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries", "InDeckGuidQuery", "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll"
        ).Build(),
        {}
    );

    BNM_CustomMethod(
        Constructor,
        false,
        BNM::Defaults::Get<void>(),
        ".ctor"
    );

    BNM_CustomMethod(
        Matches,
        false,
        BNM::Defaults::Get<bool>(),
        "Matches",
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine", "Entity", "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine", "EntityQueryContext", "EngineLib.dll"
        ).Build()
    );

    BNM_CustomMethodSkipTypeMatch(Matches);
};

inline bool PatchInDeckQueryDispatch(
    const BNM::Class& customClass,
    const BNM::Class& baseClass,
    const char* queryName = "InDeckQuery"
) {
    auto entityQueryClass = BNM::Class("PvZCards.Engine", "EntityQuery");
    if (!entityQueryClass.IsValid()) {
        LOG_ERR("CustomComponents/InDeckQuery: EntityQuery interface missing");
        return false;
    }

    auto customMatches = customClass.GetMethod("Matches", 2);
    auto baseMatches = baseClass.GetMethod("Matches", 2);
    auto interfaceMatches = entityQueryClass.GetMethod("Matches", 2);

    auto* customInfo = customMatches.IsValid() ? customMatches.GetInfo() : nullptr;
    auto* baseInfo = baseMatches.IsValid() ? baseMatches.GetInfo() : nullptr;
    auto* interfaceInfo = interfaceMatches.IsValid() ? interfaceMatches.GetInfo() : nullptr;
    auto* klass = customClass.GetClass();

    if (!klass || !customInfo || !baseInfo || !interfaceInfo) {
        LOG_ERR(
            "CustomComponents/InDeckQuery: dispatch metadata missing class=%p custom=%p base=%p interface=%p",
            klass, customInfo, baseInfo, interfaceInfo
        );
        return false;
    }

    // Concrete virtual dispatch: inherit the exact slot used by InHandQuery::Matches.
    const uint16_t concreteSlot = baseInfo->slot;
    if (concreteSlot >= klass->vtable_count) {
        LOG_ERR(
            "CustomComponents/InDeckQuery: invalid concrete Matches slot=%u vtableCount=%u",
            static_cast<unsigned>(concreteSlot),
            static_cast<unsigned>(klass->vtable_count)
        );
        return false;
    }

    // Interface dispatch: EntityFinder::RunQuery resolves EntityQuery's interface
    // offset, then adds EntityQuery.Matches' interface slot (0 in this build).
    int32_t interfaceSlot = -1;
    for (uint16_t i = 0; i < klass->interface_offsets_count; ++i) {
        const auto& pair = klass->interfaceOffsets[i];
        if (pair.interfaceType != entityQueryClass.GetClass()) continue;

        interfaceSlot = pair.offset + static_cast<int32_t>(interfaceInfo->slot);
        break;
    }

    if (interfaceSlot < 0 || interfaceSlot >= klass->vtable_count) {
        LOG_ERR(
            "CustomComponents/InDeckQuery: EntityQuery Matches interface slot invalid slot=%d vtableCount=%u",
            interfaceSlot,
            static_cast<unsigned>(klass->vtable_count)
        );
        return false;
    }

    // Keep MethodInfo's virtual slot coherent with the inherited concrete method.
    customInfo->slot = concreteSlot;

    klass->vtable[concreteSlot].methodPtr = customInfo->methodPointer;
    klass->vtable[concreteSlot].method = customInfo;

    klass->vtable[interfaceSlot].methodPtr = customInfo->methodPointer;
    klass->vtable[interfaceSlot].method = customInfo;


    return true;
}

inline bool InitializeInDeckQuery() {
    auto customClass = BNM::Class("PvZCards.Engine.Queries", "InDeckQuery");
    auto guidClass = BNM::Class("PvZCards.Engine.Queries", "InDeckGuidQuery");
    auto baseClass = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
    if (!customClass.IsValid() || !customClass.GetMonoType() ||
        !guidClass.IsValid() || !guidClass.GetMonoType()) {
        LOG_ERR("CustomComponents/InDeckQuery: custom IL2CPP class was not created");
        return false;
    }
    if (!baseClass.IsValid()) {
        LOG_ERR("CustomComponents/InDeckQuery: stock InHandQuery base class missing");
        return false;
    }

    if (!InitializeInDeckMembershipMetadata()) {
        LOG_ERR("CustomComponents/InDeckQuery: membership metadata initialization failed");
        return false;
    }

    // EntityFinder::RunQuery invokes EntityQuery.Matches through the interface
    // vtable. Explicitly repair both the inherited concrete slot and the
    // EntityQuery interface slot to point at our custom Matches implementation.
    if (!PatchInDeckQueryDispatch(customClass, baseClass)) {
        LOG_ERR("CustomComponents/InDeckQuery: dispatch patch failed");
        return false;
    }

    if (!PatchInDeckQueryDispatch(guidClass, baseClass)) {
        LOG_ERR("CustomComponents/InDeckGuidQuery: dispatch patch failed");
        return false;
    }

    if (!RegisterQuery({
            "InDeckQuery",
            "PvZCards.Engine.Queries.InDeckQuery",
            "PvZCards.Engine.Queries",
            "InDeckQuery",
            "PvZCards.Engine.Queries",
            "InHandQuery"
        })) {
        LOG_ERR("CustomComponents/InDeckQuery: query registration failed");
        return false;
    }

    if (!RegisterQuery({
            "InDeckGuidQuery",
            "PvZCards.Engine.Queries.InDeckGuidQuery",
            "PvZCards.Engine.Queries",
            "InDeckGuidQuery",
            "PvZCards.Engine.Queries",
            "InHandQuery"
        })) {
        LOG_ERR("CustomComponents/InDeckGuidQuery: query registration failed");
        return false;
    }

    return true;
}

} // namespace CustomComponents::Queries
