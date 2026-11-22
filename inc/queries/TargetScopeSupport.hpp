#include "core/HookRegistry.hpp"
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

    enum class SortValue : int {ListPosition=3,LaneIndex=4};
    inline constexpr int ToNative(SortValue value){return static_cast<int>(value);}
    inline constexpr int ListPositionSortValue = ToNative(SortValue::ListPosition);
    // Sort fighters by the combat-order index of their current lane.
    inline constexpr int LaneIndexSortValue = ToNative(SortValue::LaneIndex);

    inline BNM::Field<Obj*> scoperEntityModel;
    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;
    inline BNM::Field<MonoList<Obj*>*> orderedPoolEntries;
    inline BNM::Field<int> poolEntryEntityId;
    inline BNM::Field<MonoList<int>*> discardCardIds;
    inline BNM::Field<MonoList<int>*> fighterLaneIds;
    inline BNM::Field<int> combatOrderIndex;

    inline BNM::IL2CPP::Il2CppClass* orderedPoolClass = nullptr;
    inline BNM::IL2CPP::Il2CppClass* discardPileClass = nullptr;
    inline BNM::IL2CPP::Il2CppClass* laneElementClass = nullptr;
    inline BNM::IL2CPP::Il2CppClass* laneClass = nullptr;

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

    // Resolve the actual combat-order position of an in-play fighter.
    // LaneElement.LaneIds belongs to the fighter; Lane.CombatOrderIndex belongs
    // to each lane entity, so we never confuse a lane's entity ID with its order.
    inline int FindBoardLaneIndex(Obj* model, int targetEntityId) {
        if (!model || targetEntityId <= 0 || !laneElementClass || !laneClass)
            return -1;
        auto* entities = modelEntities[model].Get();
        if (!entities || !entities->items) return -1;

        Obj* fighter = nullptr;
        for (int i = 0; i < entities->size; ++i) {
            Obj* entity = ListAt(entities, i);
            if (entity && entityId[entity].Get() == targetEntityId) {
                fighter = entity;
                break;
            }
        }
        if (!fighter) return -1;
        auto* comps = entityComponents[fighter].Get();
        if (!comps || !comps->items) return -1;

        int bestOrder = -1;
        for (int c = 0; c < comps->size; ++c) {
            Obj* component = ListAt(comps, c);
            if (!component || component->klass != laneElementClass) continue;
            auto* ids = fighterLaneIds[component].Get();
            if (!ids || !ids->items) continue;
            for (int l = 0; l < ids->size; ++l) {
                const int laneEntityId = IntListAt(ids, l);
                for (int e = 0; e < entities->size; ++e) {
                    Obj* laneEntity = ListAt(entities, e);
                    if (!laneEntity || entityId[laneEntity].Get() != laneEntityId)
                        continue;
                    auto* laneComponents = entityComponents[laneEntity].Get();
                    if (!laneComponents || !laneComponents->items) continue;
                    for (int lc = 0; lc < laneComponents->size; ++lc) {
                        Obj* lane = ListAt(laneComponents, lc);
                        if (lane && lane->klass == laneClass) {
                            const int order = combatOrderIndex[lane].Get();
                            if (order > bestOrder) bestOrder = order;
                        }
                    }
                }
            }
        }
        return bestOrder;
    }

    static int CalculateSortedValueHook(
        void* self,
        int targetEntityId,
        int sortValue,
        const void* method
    ) {
        if (!originalCalculateSortedValue)
            return 0;

        if (static_cast<SortValue>(sortValue) != SortValue::ListPosition &&
            static_cast<SortValue>(sortValue) != SortValue::LaneIndex)
            return originalCalculateSortedValue(
                self, targetEntityId, sortValue, method
            );

        Obj* model = scoperEntityModel[static_cast<Obj*>(self)].Get();
        if (static_cast<SortValue>(sortValue) == SortValue::LaneIndex) {
            const int laneIndex = FindBoardLaneIndex(model, targetEntityId);
            if (laneIndex < 0) {
                LOG_ERR("CustomComponents/TargetScope: LaneIndex unresolved entityId=%d", targetEntityId);
                return originalCalculateSortedValue(self, targetEntityId, sortValue, method);
            }
            return laneIndex;
        }
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
        auto laneElement = BNM::Class("PvZCards.Engine.Components", "LaneElement");
        auto lane = BNM::Class("PvZCards.Engine.Components", "Lane");

        if (!scoper.IsValid() || !model.IsValid() || !entity.IsValid() ||
            !orderedPool.IsValid() || !poolEntry.IsValid() ||
            !discardPile.IsValid() || !laneElement.IsValid() || !lane.IsValid()) {
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
        fighterLaneIds = laneElement.GetField("LaneIds").cast<MonoList<int>*>();
        combatOrderIndex = lane.GetField("CombatOrderIndex").cast<int>();

        orderedPoolClass = orderedPool.GetClass();
        discardPileClass = discardPile.GetClass();
        laneElementClass = laneElement.GetClass();
        laneClass = lane.GetClass();

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
            !discardCardIds.IsValid() || !fighterLaneIds.IsValid() ||
            !combatOrderIndex.IsValid() ||
            !orderedPoolClass || !discardPileClass ||
            !laneElementClass || !laneClass ||
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
                sortValueType, "ListPosition", ListPositionSortValue) ||
            !CustomComponents::EnumSerializationSupport::Register(
                sortValueType, "LaneIndex", LaneIndexSortValue)) {
            LOG_ERR("CustomComponents/TargetScope: ListPosition enum registration failed");
            return false;
        }

        CustomComponents::Core::Hooks::Install(
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