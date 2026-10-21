#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "effects/MoveCardInDeckEffectDescriptor.hpp"
#include "effects/ReturnFromDiscardPileEffectDescriptor.hpp"
#include "log.h"

namespace CustomComponents::Systems::MoveCardInDeckSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    using MonoString = BNM::Structures::Mono::String;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    // Stock ReturnCardToPlayersDeckEffect has a DeckPosition enum (int32).
    // Only 0/1/2 are used by stock. Our carrier uses this disjoint high-bit
    // encoding so it survives the stock runtime's DeepCopyFrom/pooling without
    // native pointer-keyed sidecar state or a second EffectRouter registration.
    // A 27-bit signed position has ample range even for an extremely large deck.
    // Tag 0x7... is disjoint from RemoveFromDiscardPile marker 0x6F000001.
    inline constexpr std::uint32_t kCarrierTagMask = 0xF0000000u;
    inline constexpr std::uint32_t kCarrierTag = 0x70000000u;
    inline constexpr std::uint32_t kModeBit = 0x08000000u;
    inline constexpr std::uint32_t kPositionBits = 0x07FFFFFFu;
    inline constexpr std::int32_t kMinimumPosition = -(1 << 26);
    inline constexpr std::int32_t kMaximumPosition = (1 << 26) - 1;

    inline BNM::Field<MonoString*> descriptorMode;
    inline BNM::Field<int> descriptorPosition;
    inline BNM::Field<int> runtimeDeckPosition;
    inline BNM::Field<int> effectTargetId;
    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;
    inline BNM::Field<int> playerDeckId;
    inline BNM::Field<MonoList<Obj*>*> orderedPoolEntries;
    inline BNM::Field<int> poolEntryEntityId;
    inline BNM::Method<int> getPlayerIdFromCard;
    inline BNM::IL2CPP::Il2CppClass* orderedPoolClass = nullptr;
    inline BNM::IL2CPP::Il2CppClass* deckAssociationClass = nullptr;
    inline bool initialized = false;

    inline bool IsCarrier(int encoded) {
        return (static_cast<std::uint32_t>(encoded) & kCarrierTagMask) == kCarrierTag;
    }

    inline int Encode(bool absolute, int position) {
        const int limited = std::clamp(position, kMinimumPosition, kMaximumPosition);
        const std::uint32_t packed = kCarrierTag |
            (absolute ? kModeBit : 0) |
            (static_cast<std::uint32_t>(limited) & kPositionBits);
        return static_cast<int>(packed);
    }

    inline void Decode(int encoded, bool& absolute, int& position) {
        const std::uint32_t packed = static_cast<std::uint32_t>(encoded);
        absolute = (packed & kModeBit) != 0;
        std::int64_t value = packed & kPositionBits;
        if (value & (1LL << 26)) value -= (1LL << 27);
        position = static_cast<int>(value);
    }

    inline int Destination(int from, int count, bool absolute, int position) {
        if (count <= 0) return 0;
        // Absolute negative indices start at the end: -1 = bottom.
        const std::int64_t wanted = absolute
            ? (position < 0 ? std::int64_t(count) + position : std::int64_t(position))
            : (std::int64_t(from) + position);
        return static_cast<int>(std::clamp(wanted, std::int64_t(0), std::int64_t(count - 1)));
    }

    // Invoked only from the existing ReturnCardToPlayersDeckEffectDescriptor.ToEffect
    // hook, which is already owned by ReturnFromDiscardPileSystem. Do NOT hook it again.
    inline Obj* CreateRuntime(Obj* instances, Obj* descriptor) {
        if (!initialized || !instances || !descriptor)
            return nullptr;
        auto* modeValue = descriptorMode[descriptor].Get();
        const std::string mode = (modeValue && !modeValue->str().empty()) ? modeValue->str() : "Add";
        const bool absolute = mode == "Move";
        const bool valid = mode == "Add" || absolute;
        const int requested = descriptorPosition[descriptor].Get();
        // Bad descriptors produce a safe no-op carrier, not a stock return that
        // would actually transfer the target out of its current zone.
        const int position = valid ? requested : 0;
        if (!valid) {
            LOG_ERR("CustomComponents/MoveCardInDeck: invalid Mode='%s'; effect will do nothing", mode.c_str());
        }
        if (position < kMinimumPosition || position > kMaximumPosition) {
            CC_DIAG("CustomComponents/MoveCardInDeck: Position=%d saturated to carrier bounds", position);
        }
        Obj* runtime = Components::ReturnFromDiscardPileEffectDescriptor::CreateRuntime(instances);
        if (!runtime) return nullptr;
        runtimeDeckPosition[runtime] = Encode(absolute, position);
        return runtime;
    }

    inline Obj* FindEntity(Obj* model, int targetId) {
        if (!model || targetId < 0) return nullptr;
        auto* entities = modelEntities[model].Get();
        if (!entities || !entities->items) return nullptr;
        for (int i = 0; i < entities->size; ++i) {
            auto* entity = entities->items->m_Items[i];
            if (entity && entityId[entity].Get() == targetId) return entity;
        }
        return nullptr;
    }

    inline Obj* FindComponent(Obj* entity, BNM::IL2CPP::Il2CppClass* klass) {
        if (!entity || !klass) return nullptr;
        auto* components = entityComponents[entity].Get();
        if (!components || !components->items) return nullptr;
        for (int i = 0; i < components->size; ++i) {
            auto* component = components->items->m_Items[i];
            if (component && component->klass == klass) return component;
        }
        return nullptr;
    }

    // Invoked only from the existing ReturnCardToPlayersDeckSystem.ProcessEffect
    // hook for our marked stock runtime; original stock effects remain unchanged.
    // The target must be a real deck card, not a GUID or effect source entity.
    inline void ProcessEffect(Obj* model, Obj* effect, int encoded) {
        if (!initialized || !model || !effect || !IsCarrier(encoded)) return;
        const int targetId = effectTargetId[effect].Get();
        Obj* card = FindEntity(model, targetId);
        if (!card) {
            CC_DIAG("CustomComponents/MoveCardInDeck: target id=%d missing; no-op", targetId);
            return;
        }
        const int ownerId = getPlayerIdFromCard[static_cast<Obj*>(nullptr)](card);
        Obj* player = FindEntity(model, ownerId);
        Obj* deckAssociation = FindComponent(player, deckAssociationClass);
        if (!deckAssociation) {
            CC_DIAG("CustomComponents/MoveCardInDeck: target=%d has no owner deck; no-op", targetId);
            return;
        }
        const int deckId = playerDeckId[deckAssociation].Get();
        Obj* deck = FindEntity(model, deckId);
        Obj* pool = FindComponent(deck, orderedPoolClass);
        auto* entries = pool ? orderedPoolEntries[pool].Get() : nullptr;
        if (!entries || !entries->items || entries->size <= 1) return;

        int found = -1;
        for (int i = 0; i < entries->size; ++i) {
            Obj* entry = entries->items->m_Items[i];
            if (entry && poolEntryEntityId[entry].Get() == targetId) {
                if (found != -1) {
                    LOG_ERR("CustomComponents/MoveCardInDeck: duplicate target entry=%d deck=%d; no-op", targetId, deckId);
                    return;
                }
                found = i;
            }
        }
        if (found < 0) {
            CC_DIAG("CustomComponents/MoveCardInDeck: target=%d is not in owner deck=%d; no-op", targetId, deckId);
            return;
        }

        bool absolute = false;
        int position = 0;
        Decode(encoded, absolute, position);
        const int destination = Destination(found, entries->size, absolute, position);
        if (found == destination) return;

        auto methods = BNM::Class(static_cast<Obj*>(entries));
        auto removeAt = methods.GetMethod("RemoveAt", 1).cast<void>();
        auto insert = methods.GetMethod("Insert", 2).cast<void>();
        if (!removeAt.IsValid() || !insert.IsValid()) {
            LOG_ERR("CustomComponents/MoveCardInDeck: List.RemoveAt/Insert unavailable");
            return;
        }
        // Do not create or destroy a card. Reorder the original CardPoolEntry,
        // preserving EntityId, GUID, ownership, modifiers, and other entries.
        Obj* entry = entries->items->m_Items[found];
        removeAt[static_cast<Obj*>(entries)](found);
        insert[static_cast<Obj*>(entries)](destination, entry);
        CC_DIAG("CustomComponents/MoveCardInDeck: entity=%d deck=%d %d->%d mode=%s position=%d",
            targetId, deckId, found, destination, absolute ? "Move" : "Add", position);
    }

    inline bool Initialize() {
        if (initialized) return true;
        auto descriptor = BNM::Class("PvZCards.Engine.Components", "MoveCardInDeckEffectDescriptor");
        auto runtime = BNM::Class("PvZCards.Engine.Effects", "ReturnCardToPlayersDeckEffect");
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto deckAssoc = BNM::Class("PvZCards.Engine.Components", "DeckAssociation");
        auto orderedPool = BNM::Class("PvZCards.Engine.Components", "OrderedCardPool");
        auto entry = BNM::Class("PvZCards.Engine.Components.Data", "CardPoolEntry");
        auto playerUtils = BNM::Class("PvZCards.Core.Utilities", "PlayerUtils");
        if (!descriptor.IsValid() || !runtime.IsValid() || !effect.IsValid() ||
            !model.IsValid() || !entity.IsValid() || !deckAssoc.IsValid() ||
            !orderedPool.IsValid() || !entry.IsValid() || !playerUtils.IsValid()) {
            LOG_ERR("CustomComponents/MoveCardInDeck: required managed classes not found");
            return false;
        }
        if (!CustomComponentRegistry::FindByName(
                "PvZCards.Engine.Components", "MoveCardInDeckEffectDescriptor") &&
            !CustomComponentRegistry::Register({
                "MoveCardInDeckEffectDescriptor", "PvZCards.Engine.Components", "EngineLib.dll",
                "ReturnCardToPlayersDeckEffectDescriptor", "PvZCards.Engine.Components", "EngineLib.dll"
            })) return false;
        if (!CustomFieldBridgeSystem::Register(
                "PvZCards.Engine.Components", "MoveCardInDeckEffectDescriptor",
                "PvZCards.Engine.Components", "ReturnCardToPlayersDeckEffectDescriptor",
                {
                    {"Mode", CustomFieldBridgeSystem::FieldKind::String},
                    {"Position", CustomFieldBridgeSystem::FieldKind::Int32}
                }, false)) return false;

        descriptorMode = descriptor.GetField("Mode").cast<MonoString*>();
        descriptorPosition = descriptor.GetField("Position").cast<int>();
        runtimeDeckPosition = runtime.GetField("DeckPosition").cast<int>();
        effectTargetId = effect.GetField("TargetId").cast<int>();
        modelEntities = model.GetField("Entities").cast<MonoList<Obj*>*>();
        entityComponents = entity.GetField("Components").cast<MonoList<Obj*>*>();
        entityId = entity.GetField("<Id>k__BackingField").cast<int>();
        playerDeckId = deckAssoc.GetField("DeckEntityId").cast<int>();
        orderedPoolEntries = orderedPool.GetField("Entries").cast<MonoList<Obj*>*>();
        poolEntryEntityId = entry.GetField("EntityId").cast<int>();
        getPlayerIdFromCard = playerUtils.GetMethod("GetPlayerIdFromCard", 1).cast<int>();
        orderedPoolClass = orderedPool.GetClass();
        deckAssociationClass = deckAssoc.GetClass();
        if (!descriptorMode.IsValid() || !descriptorPosition.IsValid() ||
            !runtimeDeckPosition.IsValid() || !effectTargetId.IsValid() ||
            !modelEntities.IsValid() || !entityComponents.IsValid() ||
            !entityId.IsValid() || !playerDeckId.IsValid() ||
            !orderedPoolEntries.IsValid() || !poolEntryEntityId.IsValid() ||
            !getPlayerIdFromCard.IsValid() || !orderedPoolClass || !deckAssociationClass) {
            LOG_ERR("CustomComponents/MoveCardInDeck: managed fields/methods missing");
            return false;
        }
        initialized = true;
        CC_DIAG("CustomComponents/MoveCardInDeck: ready (no new native hooks)");
        return true;
    }

} // namespace CustomComponents::Systems::MoveCardInDeckSystem
