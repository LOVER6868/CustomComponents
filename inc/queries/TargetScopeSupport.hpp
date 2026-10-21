#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <string>

#include "core/EnumSerializationSupport.hpp"
#include "log.h"

namespace CustomComponents::Systems::TargetScopeSupport {

using Obj = BNM::IL2CPP::Il2CppObject;
template<typename T>
using MonoList = BNM::Structures::Mono::List<T>;

inline constexpr int ListPositionSortValue = 3;

inline BNM::Field<Obj*> scoperEntityModel;
inline BNM::Field<MonoList<Obj*>*> modelEntities;
inline BNM::Field<MonoList<Obj*>*> entityComponents;
inline BNM::Field<int> entityId;
inline BNM::Field<MonoList<Obj*>*> orderedPoolEntries;
inline BNM::Field<int> poolEntryEntityId;
inline BNM::Field<MonoList<int>*> discardCardIds;

inline BNM::IL2CPP::Il2CppClass* orderedPoolClass = nullptr;
inline BNM::IL2CPP::Il2CppClass* discardPileClass = nullptr;

using CalculateSortedValueFn =
    int (*)(void* self, int entityId, int sortValue, const void* method);

inline CalculateSortedValueFn originalCalculateSortedValue = nullptr;

inline Obj* ListAt(MonoList<Obj*>* list, int index) {
    if (!list || !list->items || index < 0 || index >= list->size)
        return nullptr;
    return list->items->m_Items[index];
}

inline int IntListAt(MonoList<int>* list, int index) {
    if (!list || !list->items || index < 0 || index >= list->size)
        return -1;
    return list->items->m_Items[index];
}

// Returns the literal zero-based index inside the owning ordered zone.
//
// OrderedCardPool:
//   0 = top / next-to-draw
//   last = bottom
//
// DiscardPile:
//   0 = oldest
//   last = newest / top
//
// -1 means the entity is not currently in one of the supported ordered zones.
inline int FindListPosition(Obj* model, int targetEntityId) {
    if (!model || targetEntityId <= 0)
        return -1;

    auto* entities = modelEntities[model].Get();
    if (!entities || !entities->items)
        return -1;

    for (int e = 0; e < entities->size; ++e) {
        Obj* owner = ListAt(entities, e);
        if (!owner)
            continue;

        auto* components = entityComponents[owner].Get();
        if (!components || !components->items)
            continue;

        for (int c = 0; c < components->size; ++c) {
            Obj* component = ListAt(components, c);
            if (!component || !component->klass)
                continue;

            if (component->klass == orderedPoolClass) {
                auto* entries = orderedPoolEntries[component].Get();
                if (!entries || !entries->items)
                    continue;

                for (int i = 0; i < entries->size; ++i) {
                    Obj* entry = ListAt(entries, i);
                    if (entry && poolEntryEntityId[entry].Get() == targetEntityId)
                        return i;
                }
            }

            if (component->klass == discardPileClass) {
                auto* ids = discardCardIds[component].Get();
                if (!ids || !ids->items)
                    continue;

                for (int i = 0; i < ids->size; ++i) {
                    if (IntListAt(ids, i) == targetEntityId)
                        return i;
                }
            }
        }
    }

    return -1;
}

static int CalculateSortedValueHook(
    void* self,
    int targetEntityId,
    int sortValue,
    const void* method
) {
    if (!originalCalculateSortedValue)
        return 0;

    if (sortValue != ListPositionSortValue)
        return originalCalculateSortedValue(
            self, targetEntityId, sortValue, method
        );

    Obj* model = scoperEntityModel[static_cast<Obj*>(self)].Get();
    const int position = FindListPosition(model, targetEntityId);

    // Log only custom ListPosition sorting; stock sorts pass through unchanged.
    // If an entity is unexpectedly outside deck/discard, return stock's value
    // (currently 0 for unknown sort values) rather than inventing ordering.
    if (position < 0) {
        LOG_ERR(
            "CustomComponents/TargetScope: ListPosition unresolved entityId=%d",
            targetEntityId
        );
        return originalCalculateSortedValue(
            self, targetEntityId, sortValue, method
        );
    }

    
    return position;
}

inline bool Initialize() {
    auto scoper = BNM::Class(
        "PvZCards.Core.Systems.Utilities",
        "TriggeredEffectTargetScoper"
    );
    auto model = BNM::Class("PvZCards.Engine", "EntityModel");
    auto entity = BNM::Class("PvZCards.Engine", "Entity");
    auto orderedPool = BNM::Class(
        "PvZCards.Engine.Components",
        "OrderedCardPool"
    );
    auto poolEntry = BNM::Class(
        "PvZCards.Engine.Components.Data",
        "CardPoolEntry"
    );
    auto discardPile = BNM::Class(
        "PvZCards.Engine.Components",
        "DiscardPile"
    );

    if (!scoper.IsValid() || !model.IsValid() || !entity.IsValid() ||
        !orderedPool.IsValid() || !poolEntry.IsValid() ||
        !discardPile.IsValid()) {
        LOG_ERR("CustomComponents/TargetScope: required classes missing");
        return false;
    }

    scoperEntityModel =
        scoper.GetField("<EntityModel>k__BackingField").cast<Obj*>();
    modelEntities =
        model.GetField("Entities").cast<MonoList<Obj*>*>();
    entityComponents =
        entity.GetField("Components").cast<MonoList<Obj*>*>();
    entityId =
        entity.GetField("<Id>k__BackingField").cast<int>();
    orderedPoolEntries =
        orderedPool.GetField("Entries").cast<MonoList<Obj*>*>();
    poolEntryEntityId =
        poolEntry.GetField("EntityId").cast<int>();
    discardCardIds =
        discardPile.GetField("CardIds").cast<MonoList<int>*>();

    orderedPoolClass = orderedPool.GetClass();
    discardPileClass = discardPile.GetClass();

    auto calculate =
        scoper.GetMethod("CalculateSortedValueFromEntityId", 2);
    auto sortValueEnum = BNM::Class(
        "PvZCards.Engine.Components", "TargetScopeSortValue"
    );

    if (!scoperEntityModel.IsValid() ||
        !modelEntities.IsValid() ||
        !entityComponents.IsValid() ||
        !entityId.IsValid() ||
        !orderedPoolEntries.IsValid() ||
        !poolEntryEntityId.IsValid() ||
        !discardCardIds.IsValid() ||
        !orderedPoolClass ||
        !discardPileClass ||
        !calculate.IsValid() ||
        !sortValueEnum.IsValid()) {
        LOG_ERR("CustomComponents/TargetScope: required API missing");
        return false;
    }

    // Register this feature's custom enum value before the generic
    // deserializer hook is installed later by the module entry point.
    auto* sortValueType = sortValueEnum.GetMonoType();
    if (!sortValueType ||
        !CustomComponents::EnumSerializationSupport::Register(
            sortValueType, "ListPosition", ListPositionSortValue)) {
        LOG_ERR("CustomComponents/TargetScope: ListPosition enum registration failed");
        return false;
    }

    BNM::BasicHook(
        calculate,
        CalculateSortedValueHook,
        originalCalculateSortedValue
    );

    if (!originalCalculateSortedValue) {
        LOG_ERR(
            "CustomComponents/TargetScope: CalculateSortedValue hook failed"
        );
        return false;
    }

    
    return true;
}

} // namespace CustomComponents::Systems::TargetScopeSupport
