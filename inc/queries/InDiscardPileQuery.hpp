#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "queries/QueryRegistry.hpp"
#include "log.h"

namespace CustomComponents::Queries {

using MonoObjList = BNM::Structures::Mono::List<Obj*>;
using MonoIntList = BNM::Structures::Mono::List<int>;

inline BNM::Field<int> inDiscardPileEntityId;
inline BNM::Field<MonoObjList*> inDiscardPileModelEntities;
inline BNM::Field<MonoObjList*> inDiscardPileEntityComponents;
inline BNM::Field<MonoIntList*> inDiscardPileCardIds;
inline BNM::Field<int> inDiscardPileCardGuid;
inline BNM::Field<Obj*> inDiscardPileContextModel;
inline BNM::Method<Obj*> inDiscardPileGetEntityById;
inline BNM::IL2CPP::Il2CppClass* inDiscardPileClass = nullptr;
inline BNM::IL2CPP::Il2CppClass* inDiscardPileCardClass = nullptr;

template <typename T>
inline T* InDiscardPileListAt(
    BNM::Structures::Mono::List<T*>* list,
    int index
) {
    if (!list || !list->items || index < 0 || index >= list->size)
        return nullptr;
    return list->items->m_Items[index];
}

inline int InDiscardPileIntAt(MonoIntList* list, int index) {
    if (!list || !list->items || index < 0 || index >= list->size)
        return -1;
    return list->items->m_Items[index];
}

inline int InDiscardPileCardGuidOf(Obj* entity) {
    if (!entity || !inDiscardPileEntityComponents.IsValid() ||
        !inDiscardPileCardGuid.IsValid() || !inDiscardPileCardClass)
        return -1;

    MonoObjList* components = inDiscardPileEntityComponents[entity].Get();
    if (!components || !components->items)
        return -1;

    for (int i = 0; i < components->size; ++i) {
        Obj* component = InDiscardPileListAt(components, i);
        if (!component || component->klass != inDiscardPileCardClass)
            continue;
        return inDiscardPileCardGuid[component].Get();
    }

    return -1;
}

inline bool InitializeInDiscardPileMembershipMetadata() {
    auto entity = BNM::Class("PvZCards.Engine", "Entity");
    auto model = BNM::Class("PvZCards.Engine", "EntityModel");
    auto context = BNM::Class("PvZCards.Engine", "EntityQueryContext");
    auto discardPile = BNM::Class(
        "PvZCards.Engine.Components",
        "DiscardPile"
    );
    auto card = BNM::Class(
        "PvZCards.Engine.Components",
        "Card"
    );

    if (!entity.IsValid() ||
        !model.IsValid() ||
        !context.IsValid() ||
        !discardPile.IsValid() ||
        !card.IsValid()) {
        LOG_ERR("CustomComponents/InDiscardPileQuery: membership classes missing");
        return false;
    }

    inDiscardPileEntityId =
        entity.GetField("<Id>k__BackingField").cast<int>();
    inDiscardPileModelEntities =
        model.GetField("Entities").cast<MonoObjList*>();
    inDiscardPileEntityComponents =
        entity.GetField("Components").cast<MonoObjList*>();
    inDiscardPileCardIds =
        discardPile.GetField("CardIds").cast<MonoIntList*>();
    inDiscardPileCardGuid =
        card.GetField("Guid").cast<int>();
    inDiscardPileContextModel =
        context.GetField("<Model>k__BackingField").cast<Obj*>();
    inDiscardPileGetEntityById =
        model.GetMethod("GetEntityById", 1).cast<Obj*>();
    inDiscardPileClass = discardPile.GetClass();
    inDiscardPileCardClass = card.GetClass();

    if (!inDiscardPileEntityId.IsValid() ||
        !inDiscardPileModelEntities.IsValid() ||
        !inDiscardPileEntityComponents.IsValid() ||
        !inDiscardPileCardIds.IsValid() ||
        !inDiscardPileCardGuid.IsValid() ||
        !inDiscardPileContextModel.IsValid() ||
        !inDiscardPileGetEntityById.IsValid() ||
        !inDiscardPileClass ||
        !inDiscardPileCardClass) {
        LOG_ERR(
            "CustomComponents/InDiscardPileQuery: membership API missing "
            "id=%d entities=%d components=%d cardIds=%d cardGuid=%d contextModel=%d getEntity=%d pileClass=%p cardClass=%p",
            inDiscardPileEntityId.IsValid(),
            inDiscardPileModelEntities.IsValid(),
            inDiscardPileEntityComponents.IsValid(),
            inDiscardPileCardIds.IsValid(),
            inDiscardPileCardGuid.IsValid(),
            inDiscardPileContextModel.IsValid(),
            inDiscardPileGetEntityById.IsValid(),
            inDiscardPileClass,
            inDiscardPileCardClass
        );
        return false;
    }

    return true;
}

// JSON-visible IL2CPP query.
//
// Managed shape:
//   PvZCards.Engine.Queries.InDiscardPileQuery
//       : PvZCards.Engine.Queries.InHandQuery
//
// Like InDeckQuery, this inherits the stateless stock InHandQuery only to reuse
// the proven EntityQuery / Poolable / DeepCopyable lifecycle and interface shape.
// Membership itself is independent of hand state:
//
//   candidate Entity.Id == any DiscardPile.CardIds[i]
//
// DiscardPile.CardIds stores real entity IDs directly; there is no CardPoolEntry
// indirection as there is for OrderedCardPool.
struct InDiscardPileQuery final : BNM::IL2CPP::Il2CppObject {
    void Constructor() {
        auto baseClass = BNM::Class(
            "PvZCards.Engine.Queries",
            "InHandQuery"
        );
        auto baseCtor = baseClass.GetMethod(".ctor", 0);
        if (!baseCtor.IsValid()) {
            LOG_ERR(
                "CustomComponents/InDiscardPileQuery: "
                ".ctor could not resolve InHandQuery::.ctor"
            );
            return;
        }

        baseCtor.cast<void>()[this]();
    }

    bool Matches(Obj* entity, Obj* context) {
        if (!entity || !context)
            return false;

        const int candidateId = inDiscardPileEntityId[entity].Get();
        if (candidateId <= 0)
            return false;

        Obj* model = inDiscardPileContextModel[context].Get();
        if (!model)
            return false;

        MonoObjList* entities = inDiscardPileModelEntities[model].Get();
        if (!entities || !entities->items)
            return false;

        // A card is in a discard pile iff its live Entity.Id occurs directly in
        // a DiscardPile.CardIds list.
        for (int e = 0; e < entities->size; ++e) {
            Obj* pileOwner = InDiscardPileListAt(entities, e);
            if (!pileOwner)
                continue;

            MonoObjList* components =
                inDiscardPileEntityComponents[pileOwner].Get();
            if (!components || !components->items)
                continue;

            for (int c = 0; c < components->size; ++c) {
                Obj* component = InDiscardPileListAt(components, c);
                if (!component || component->klass != inDiscardPileClass)
                    continue;

                MonoIntList* cardIds =
                    inDiscardPileCardIds[component].Get();
                if (!cardIds || !cardIds->items)
                    continue;

                for (int i = 0; i < cardIds->size; ++i) {
                    if (InDiscardPileIntAt(cardIds, i) == candidateId)
                        return true;
                }
            }
        }

        return false;
    }

    BNM_CustomClass(
        InDiscardPileQuery,
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries",
            "InDiscardPileQuery",
            "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries",
            "InHandQuery",
            "EngineLib.dll"
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
            "PvZCards.Engine",
            "Entity",
            "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine",
            "EntityQueryContext",
            "EngineLib.dll"
        ).Build()
    );

    BNM_CustomMethodSkipTypeMatch(Matches);
};


// CardDatabase-facing variant.
// Tests whether the candidate card definition's Card.Guid is represented by
// any live entity currently referenced by a DiscardPile.CardIds list.
struct InDiscardPileGuidQuery final : BNM::IL2CPP::Il2CppObject {
    void Constructor() {
        auto baseClass = BNM::Class(
            "PvZCards.Engine.Queries",
            "InHandQuery"
        );
        auto baseCtor = baseClass.GetMethod(".ctor", 0);
        if (!baseCtor.IsValid()) {
            LOG_ERR(
                "CustomComponents/InDiscardPileGuidQuery: "
                ".ctor could not resolve InHandQuery::.ctor"
            );
            return;
        }
        baseCtor.cast<void>()[this]();
    }

    bool Matches(Obj* entity, Obj* context) {
        if (!entity || !context)
            return false;

        const int candidateGuid = InDiscardPileCardGuidOf(entity);
        if (candidateGuid <= 0)
            return false;

        Obj* model = inDiscardPileContextModel[context].Get();
        if (!model)
            return false;

        MonoObjList* entities = inDiscardPileModelEntities[model].Get();
        if (!entities || !entities->items)
            return false;

        for (int e = 0; e < entities->size; ++e) {
            Obj* pileOwner = InDiscardPileListAt(entities, e);
            if (!pileOwner)
                continue;

            MonoObjList* components =
                inDiscardPileEntityComponents[pileOwner].Get();
            if (!components || !components->items)
                continue;

            for (int c = 0; c < components->size; ++c) {
                Obj* component = InDiscardPileListAt(components, c);
                if (!component || component->klass != inDiscardPileClass)
                    continue;

                MonoIntList* cardIds =
                    inDiscardPileCardIds[component].Get();
                if (!cardIds || !cardIds->items)
                    continue;

                for (int i = 0; i < cardIds->size; ++i) {
                    const int discardEntityId =
                        InDiscardPileIntAt(cardIds, i);
                    if (discardEntityId <= 0)
                        continue;

                    Obj* discardedEntity =
                        inDiscardPileGetEntityById[model](discardEntityId);
                    if (!discardedEntity)
                        continue;

                    if (InDiscardPileCardGuidOf(discardedEntity) == candidateGuid)
                        return true;
                }
            }
        }

        return false;
    }

    BNM_CustomClass(
        InDiscardPileGuidQuery,
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries",
            "InDiscardPileGuidQuery",
            "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine.Queries",
            "InHandQuery",
            "EngineLib.dll"
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
            "PvZCards.Engine",
            "Entity",
            "EngineLib.dll"
        ).Build(),
        BNM::CompileTimeClassBuilder(
            "PvZCards.Engine",
            "EntityQueryContext",
            "EngineLib.dll"
        ).Build()
    );

    BNM_CustomMethodSkipTypeMatch(Matches);
};

inline bool PatchInDiscardPileQueryDispatch(
    const BNM::Class& customClass,
    const BNM::Class& baseClass
) {
    auto entityQueryClass = BNM::Class("PvZCards.Engine", "EntityQuery");
    if (!entityQueryClass.IsValid()) {
        LOG_ERR("CustomComponents/InDiscardPileQuery: EntityQuery interface missing");
        return false;
    }

    auto customMatches = customClass.GetMethod("Matches", 2);
    auto baseMatches = baseClass.GetMethod("Matches", 2);
    auto interfaceMatches = entityQueryClass.GetMethod("Matches", 2);

    auto* customInfo = customMatches.IsValid()
        ? customMatches.GetInfo()
        : nullptr;
    auto* baseInfo = baseMatches.IsValid()
        ? baseMatches.GetInfo()
        : nullptr;
    auto* interfaceInfo = interfaceMatches.IsValid()
        ? interfaceMatches.GetInfo()
        : nullptr;
    auto* klass = customClass.GetClass();

    if (!klass || !customInfo || !baseInfo || !interfaceInfo) {
        LOG_ERR(
            "CustomComponents/InDiscardPileQuery: dispatch metadata missing "
            "class=%p custom=%p base=%p interface=%p",
            klass,
            customInfo,
            baseInfo,
            interfaceInfo
        );
        return false;
    }

    // InHandQuery::Matches is slot 4 in this build. Use the metadata slot rather
    // than hard-coding it so this stays aligned with the proven InDeckQuery path.
    const uint16_t concreteSlot = baseInfo->slot;
    if (concreteSlot >= klass->vtable_count) {
        LOG_ERR(
            "CustomComponents/InDiscardPileQuery: invalid concrete Matches "
            "slot=%u vtableCount=%u",
            static_cast<unsigned>(concreteSlot),
            static_cast<unsigned>(klass->vtable_count)
        );
        return false;
    }

    // EntityFinder::RunQuery (RVA 0x22818A4) invokes EntityQuery.Matches through
    // the EntityQuery interface dispatch table. Patch that slot explicitly, just
    // like InDeckQuery.
    int32_t interfaceSlot = -1;
    for (uint16_t i = 0; i < klass->interface_offsets_count; ++i) {
        const auto& pair = klass->interfaceOffsets[i];
        if (pair.interfaceType != entityQueryClass.GetClass())
            continue;

        interfaceSlot =
            pair.offset + static_cast<int32_t>(interfaceInfo->slot);
        break;
    }

    if (interfaceSlot < 0 || interfaceSlot >= klass->vtable_count) {
        LOG_ERR(
            "CustomComponents/InDiscardPileQuery: EntityQuery Matches interface "
            "slot invalid slot=%d vtableCount=%u",
            interfaceSlot,
            static_cast<unsigned>(klass->vtable_count)
        );
        return false;
    }

    customInfo->slot = concreteSlot;

    klass->vtable[concreteSlot].methodPtr = customInfo->methodPointer;
    klass->vtable[concreteSlot].method = customInfo;

    klass->vtable[interfaceSlot].methodPtr = customInfo->methodPointer;
    klass->vtable[interfaceSlot].method = customInfo;

    return true;
}

inline bool InitializeInDiscardPileQuery() {
    auto customClass = BNM::Class(
        "PvZCards.Engine.Queries",
        "InDiscardPileQuery"
    );
    auto guidClass = BNM::Class(
        "PvZCards.Engine.Queries",
        "InDiscardPileGuidQuery"
    );
    auto baseClass = BNM::Class(
        "PvZCards.Engine.Queries",
        "InHandQuery"
    );

    if (!customClass.IsValid() || !customClass.GetMonoType() ||
        !guidClass.IsValid() || !guidClass.GetMonoType()) {
        LOG_ERR(
            "CustomComponents/InDiscardPileQuery: custom IL2CPP class was not created"
        );
        return false;
    }

    if (!baseClass.IsValid()) {
        LOG_ERR(
            "CustomComponents/InDiscardPileQuery: stock InHandQuery base class missing"
        );
        return false;
    }

    if (!InitializeInDiscardPileMembershipMetadata()) {
        LOG_ERR(
            "CustomComponents/InDiscardPileQuery: membership metadata initialization failed"
        );
        return false;
    }

    if (!PatchInDiscardPileQueryDispatch(customClass, baseClass)) {
        LOG_ERR("CustomComponents/InDiscardPileQuery: dispatch patch failed");
        return false;
    }

    if (!PatchInDiscardPileQueryDispatch(guidClass, baseClass)) {
        LOG_ERR("CustomComponents/InDiscardPileGuidQuery: dispatch patch failed");
        return false;
    }

    if (!RegisterQuery({
            "InDiscardPileQuery",
            "PvZCards.Engine.Queries.InDiscardPileQuery",
            "PvZCards.Engine.Queries",
            "InDiscardPileQuery",
            "PvZCards.Engine.Queries",
            "InHandQuery"
        })) {
        LOG_ERR("CustomComponents/InDiscardPileQuery: query registration failed");
        return false;
    }

    if (!RegisterQuery({
            "InDiscardPileGuidQuery",
            "PvZCards.Engine.Queries.InDiscardPileGuidQuery",
            "PvZCards.Engine.Queries",
            "InDiscardPileGuidQuery",
            "PvZCards.Engine.Queries",
            "InHandQuery"
        })) {
        LOG_ERR("CustomComponents/InDiscardPileGuidQuery: query registration failed");
        return false;
    }

    return true;
}

} // namespace CustomComponents::Queries
