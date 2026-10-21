#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <string>
#include <vector>

#include "effects/RemoveFromDiscardPileEffectDescriptor.hpp"
#include "effects/ReturnFromDiscardPileEffectDescriptor.hpp"
#include "zones/MoveCardInDeckSystem.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "triggers/CustomTriggerSystem.hpp"
#include "zones/DiscardProvenanceSystem.hpp"
#include "presentation/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "log.h"
#include "core/CardViewLifecycleCoordinator.hpp"

namespace CustomComponents::Systems::ReturnFromDiscardPileSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    constexpr int kReturnFromDiscardMarker =
        Components::ReturnFromDiscardPileEffectDescriptor::kReturnFromDiscardSentinel;

    // DIAGNOSTIC: restore the frontend draw-style record after isolating stale
    // discard-scoped presentation animations. The gameplay move still reuses
    // the original live card. Keep the stale self/target presentation guards
    // in ZoneTriggeredAbilityPresentationSystem.hpp for this test.
    inline constexpr bool kEmitSyntheticDrawRecord = true;

    inline BNM::Field<int> effectTargetId;
    inline BNM::Field<int> runtimeDeckPosition;

    inline BNM::Field<Obj*> systemModel;
    inline BNM::Field<Obj*> systemDiscardPileModifier;
    inline BNM::Field<Obj*> systemHandModifier;
    inline BNM::Field<Obj*> systemHistory;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;

    inline BNM::Field<int> ownerId;
    inline BNM::Field<int> handAssociationId;
    inline BNM::Field<int> discardAssociationId;
    inline BNM::Field<int> deckAssociationId;
    inline BNM::Field<MonoList<int>*> discardCardIds;
    inline BNM::Field<MonoList<int>*> handCardIds;

    inline BNM::Method<void> discardRemoveCard;
    inline BNM::Method<void> handAddCard;

    inline BNM::Method<Obj*> historyAddRecordByType;
    inline BNM::Method<Obj*> cardIdDataCreateFromEntity;
    inline BNM::Method<void> drawRecordSetHandId;
    inline BNM::Method<void> drawRecordSetDeckId;
    inline BNM::Method<void> drawRecordSetCard;
    inline BNM::Method<void> effectRecordSetDefaultIds;

    using DescriptorToEffectFn = Obj * (*)(void*, Obj*, Obj*, const void*);
    using ProcessEffectFn = void (*)(void*, Obj*, const void*);

    inline DescriptorToEffectFn originalDescriptorToEffect = nullptr;
    inline ProcessEffectFn originalProcessEffect = nullptr;

    template<typename T>
    inline std::vector<T> Items(MonoList<T>* list) {
        std::vector<T> out;
        if (!list || !list->items)
            return out;
        out.reserve(list->size);
        for (int i = 0; i < list->size; ++i)
            out.push_back(list->items->m_Items[i]);
        return out;
    }

    inline std::string TypeName(Obj* object) {
        if (!object)
            return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "?";
    }

    inline Obj* FindComponent(Obj* entity, const char* name) {
        if (!entity)
            return nullptr;
        for (auto* component : Items(entityComponents[entity].Get())) {
            if (component && TypeName(component) == name)
                return component;
        }
        return nullptr;
    }

    // EntityId 0 is valid in this game build. Only negative IDs are invalid.
    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;
        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

    inline int CountDiscard(Obj* pile) {
        auto* component = FindComponent(pile, "DiscardPile");
        auto* ids = component ? discardCardIds[component].Get() : nullptr;
        return ids ? ids->size : -1;
    }

    inline int CountHand(Obj* hand) {
        auto* component = FindComponent(hand, "Hand");
        auto* ids = component ? handCardIds[component].Get() : nullptr;
        return ids ? ids->size : -1;
    }

    inline bool ContainsId(MonoList<int>* ids, int cardId) {
        if (!ids || !ids->items)
            return false;
        for (int i = 0; i < ids->size; ++i) {
            if (ids->items->m_Items[i] == cardId)
                return true;
        }
        return false;
    }

    inline bool DiscardContains(Obj* pile, int cardId) {
        auto* component = FindComponent(pile, "DiscardPile");
        return component && ContainsId(discardCardIds[component].Get(), cardId);
    }

    inline bool HandContains(Obj* hand, int cardId) {
        auto* component = FindComponent(hand, "Hand");
        return component && ContainsId(handCardIds[component].Get(), cardId);
    }

    static Obj* DescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        auto* descriptor = static_cast<Obj*>(self);
        const std::string typeName = TypeName(descriptor);

        // Reuse this hook owner for deck reordering; never hook stock ToEffect twice.
        if (typeName == "MoveCardInDeckEffectDescriptor") {
            return MoveCardInDeckSystem::CreateRuntime(instances, descriptor);
        }

        if (typeName == "ReturnFromDiscardPileEffectDescriptor") {
            Obj* runtime = Components::ReturnFromDiscardPileEffectDescriptor::CreateRuntime(
                instances
            );

            CC_DIAG(
                "CustomComponents/ReturnFromDiscard: descriptor -> runtime=%p marker=%d",
                runtime,
                runtime ? runtimeDeckPosition[runtime].Get() : -1
            );
            return runtime;
        }

        if (typeName == "RemoveFromDiscardPileEffectDescriptor") {
            Obj* runtime = Components::RemoveFromDiscardPileEffectDescriptor::CreateRuntime(
                instances
            );

            CC_DIAG(
                "CustomComponents/RemoveFromDiscardPile: descriptor -> runtime=%p marker=%d",
                runtime,
                runtime ? runtimeDeckPosition[runtime].Get() : -1
            );
            return runtime;
        }

        return originalDescriptorToEffect(self, instances, effectEntity, method);
    }

    static void ProcessEffectHook(void* self, Obj* effect, const void* method) {
        if (!effect) {
            originalProcessEffect(self, effect, method);
            return;
        }

        const int route = runtimeDeckPosition[effect].Get();
        // Our encoded DeckPosition carrier cannot collide with stock 0/1/2
        // or with the existing return/remove markers 99 and 0x6F000001.
        if (MoveCardInDeckSystem::IsCarrier(route)) {
            Obj* model = systemModel[static_cast<Obj*>(self)].Get();
            MoveCardInDeckSystem::ProcessEffect(model, effect, route);
            return;
        }
        const bool isHandReturn =
            route == Components::ReturnFromDiscardPileEffectDescriptor::kReturnFromDiscardSentinel;
        const bool isRemoveOnly =
            route == Components::RemoveFromDiscardPileEffectDescriptor::kRemoveFromDiscardSentinel;

        if (!isHandReturn && !isRemoveOnly) {
            originalProcessEffect(self, effect, method);
            return;
        }

        auto* system = static_cast<Obj*>(self);
        Obj* model = systemModel[system].Get();
        Obj* discardModifier = systemDiscardPileModifier[system].Get();
        const int targetId = effectTargetId[effect].Get();

        Obj* card = FindEntity(model, targetId);
        if (!card) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: target EntityId=%d not found",
                targetId
            );
            return;
        }

        Obj* owner = FindComponent(card, "Owner");
        if (!owner) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: target %d has no Owner",
                targetId
            );
            return;
        }

        const int playerId = ownerId[owner].Get();
        Obj* player = FindEntity(model, playerId);
        if (!player) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: owner player EntityId=%d not found",
                playerId
            );
            return;
        }

        Obj* discardAssociation = FindComponent(player, "DiscardPileAssociation");
        if (!discardAssociation) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: player %d missing DiscardPileAssociation",
                playerId
            );
            return;
        }

        const int pileId = discardAssociationId[discardAssociation].Get();
        Obj* pile = FindEntity(model, pileId);
        if (!pile || !discardModifier || !DiscardContains(pile, targetId)) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: target %d is not in owner discard pile %d",
                targetId,
                pileId
            );
            return;
        }

        if (isRemoveOnly) {
            discardRemoveCard[discardModifier](targetId, pileId);

            CC_DIAG(
                "CustomComponents/RemoveFromDiscardPile: SUCCESS EntityId=%d PlayerId=%d DiscardPileId=%d (entity preserved; no destination)",
                targetId,
                playerId,
                pileId
            );
            return;
        }

        Obj* handModifier = systemHandModifier[system].Get();
        Obj* history = systemHistory[system].Get();
        Obj* handAssociation = FindComponent(player, "HandAssociation");
        Obj* deckAssociation = FindComponent(player, "DeckAssociation");

        if (!handModifier || !history || !handAssociation || !deckAssociation) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: missing hand return state handModifier=%p history=%p handAssoc=%p deckAssoc=%p",
                handModifier,
                history,
                handAssociation,
                deckAssociation
            );
            return;
        }

        const int handId = handAssociationId[handAssociation].Get();
        const int deckId = deckAssociationId[deckAssociation].Get();
        Obj* hand = FindEntity(model, handId);

        if (!hand) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: hand EntityId=%d not found",
                handId
            );
            return;
        }

        // Capture provenance BEFORE removing the Discarded marker or moving
        // the live entity. Lane-origin (Source=0) is always stock-only.
        const int statReturnSource = DiscardedSystem::GetSource(model, targetId);
        CC_DIAG("CustomComponents/ReturnFromDiscard/RETURN-STAT: origin id=%d source=%d (1=hand,2=deck,0=stock)",
                 targetId, statReturnSource);
        const int beforePile = CountDiscard(pile);
        const int beforeHand = CountHand(hand);
        const bool handContainedBefore = HandContains(hand, targetId);

        discardRemoveCard[discardModifier](targetId, pileId);
        handAddCard[handModifier](hand, card);

        const int afterPile = CountDiscard(pile);
        const int afterHand = CountHand(hand);
        const bool stillInDiscard = DiscardContains(pile, targetId);
        const bool handContainsAfter = HandContains(hand, targetId);

        CC_DIAG(
            "CustomComponents/ReturnFromDiscard: EXIT CardId=%d hand=%d->%d pile=%d->%d contains=%d->%d discardContains=%d",
            targetId,
            beforeHand,
            afterHand,
            beforePile,
            afterPile,
            handContainedBefore ? 1 : 0,
            handContainsAfter ? 1 : 0,
            stillInDiscard ? 1 : 0
        );

        if (stillInDiscard || !handContainsAfter) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: move verification failed EntityId=%d",
                targetId
            );
            return;
        }

        // Observational marker only: no Unity calls on this gameplay thread.
        // This includes returns of cards destroyed on the board or discarded from deck.
        CustomComponents::Core::CardViewLifecycleCoordinator::NoteReturn(targetId, handId, pileId);

        // Presentation bridge only. Gameplay already moved the SAME live entity.
        if (kEmitSyntheticDrawRecord) {
        CC_DIAG("CustomComponents/ReturnFromDiscard/DIAGNOSTIC: EMIT synthetic DrawCardRecord card=%d hand=%d deckAnchor=%d", targetId, handId, deckId);
        auto recordClass = BNM::Class(
            "PvZCards.Engine.Records",
            "DrawCardRecord"
        );
        Obj* drawRecord = historyAddRecordByType[history](
            reinterpret_cast<Obj*>(recordClass.GetMonoType())
            );

        if (drawRecord) {
            Obj* cardData =
                cardIdDataCreateFromEntity[static_cast<Obj*>(nullptr)](card);

            if (cardData) {
                drawRecordSetHandId[drawRecord](handId);
                drawRecordSetDeckId[drawRecord](deckId); // visual anchor only
                drawRecordSetCard[drawRecord](cardData);
                effectRecordSetDefaultIds[drawRecord](effect);
                // Cross-thread identity ticket only; frontend ShowInHand owns
                // the actual one-shot stat presentation correction.
                ZoneTriggeredAbilityPresentationSystem::RememberHandDeckReturn(
                    model, targetId, statReturnSource);

                CC_DIAG(
                    "CustomComponents/ReturnFromDiscard: FRONTEND DRAW-STYLE EntityId=%d HandId=%d VisualDeckId=%d record=%p cardData=%p",
                    targetId,
                    handId,
                    deckId,
                    drawRecord,
                    cardData
                );
            }
            else {
                LOG_ERR(
                    "CustomComponents/ReturnFromDiscard: CardIdData.CreateFromEntity failed for EntityId=%d",
                    targetId
                );
            }
        }
        else {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscard: failed to create DrawCardRecord for EntityId=%d",
                targetId
            );
        }

        } else {
            CC_DIAG("CustomComponents/ReturnFromDiscard/DIAGNOSTIC: SKIPPED synthetic DrawCardRecord card=%d hand=%d deckAnchor=%d; gameplay move and custom trigger preserved", targetId, handId, deckId);
        }

        CustomTriggerSystem::FireReturnedFromDiscardPile(effect);
    }

    inline bool RegisterType() {
        const char* names[] = {
            "ReturnFromDiscardPileEffectDescriptor",
            "RemoveFromDiscardPileEffectDescriptor"
        };

        for (const char* name : names) {
            if (CustomComponentRegistry::FindByName(
                "PvZCards.Engine.Components",
                name)) {
                continue;
            }

            if (!CustomComponentRegistry::Register({
                    name,
                    "PvZCards.Engine.Components",
                    "EngineLib.dll",
                    "ReturnCardToPlayersDeckEffectDescriptor",
                    "PvZCards.Engine.Components",
                    "EngineLib.dll"
                })) {
                return false;
            }
        }

        return true;
    }

    inline bool InstallHooks() {
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto runtime = BNM::Class(
            "PvZCards.Engine.Effects", "ReturnCardToPlayersDeckEffect"
        );
        auto descriptor = BNM::Class(
            "PvZCards.Engine.Components", "ReturnCardToPlayersDeckEffectDescriptor"
        );
        auto system = BNM::Class(
            "PvZCards.Core.Systems", "ReturnCardToPlayersDeckSystem"
        );
        auto discardModifierClass = BNM::Class(
            "PvZCards.Core.Systems.Utilities", "DiscardPileModifier"
        );
        auto handModifierClass = BNM::Class(
            "PvZCards.Core.Systems.Utilities", "HandModifier"
        );
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto owner = BNM::Class("PvZCards.Engine.Components", "Owner");
        auto handAssociation = BNM::Class(
            "PvZCards.Engine.Components", "HandAssociation"
        );
        auto discardAssociation = BNM::Class(
            "PvZCards.Engine.Components", "DiscardPileAssociation"
        );
        auto deckAssociation = BNM::Class(
            "PvZCards.Engine.Components", "DeckAssociation"
        );
        auto discardPile = BNM::Class(
            "PvZCards.Engine.Components", "DiscardPile"
        );
        auto hand = BNM::Class("PvZCards.Engine.Components", "Hand");

        auto history = BNM::Class("PvZCards.Engine", "History");
        auto systemType = BNM::Class("System", "Type");
        auto cardIdData = BNM::Class(
            "PvZCards.Engine.Records", "CardIdData"
        );
        auto drawCardRecord = BNM::Class(
            "PvZCards.Engine.Records", "DrawCardRecord"
        );
        auto effectRecord = BNM::Class(
            "PvZCards.Engine.Records", "EffectRecord"
        );

        if (!effect.IsValid() || !runtime.IsValid() || !descriptor.IsValid() ||
            !system.IsValid() || !discardModifierClass.IsValid() ||
            !handModifierClass.IsValid() || !model.IsValid() ||
            !entity.IsValid() || !owner.IsValid() ||
            !handAssociation.IsValid() || !discardAssociation.IsValid() ||
            !deckAssociation.IsValid() || !discardPile.IsValid() ||
            !hand.IsValid() || !history.IsValid() || !systemType.IsValid() ||
            !cardIdData.IsValid() || !drawCardRecord.IsValid() ||
            !effectRecord.IsValid()) {
            LOG_ERR("CustomComponents/ReturnFromDiscard: required classes missing");
            return false;
        }

        effectTargetId = effect.GetField("TargetId");
        runtimeDeckPosition = runtime.GetField("DeckPosition");
        systemModel = system.GetField("<Model>k__BackingField");
        systemDiscardPileModifier = system.GetField("<DiscardPileModifier>k__BackingField");
        systemHandModifier = system.GetField("<HandModifier>k__BackingField");
        systemHistory = system.GetField("<History>k__BackingField");
        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");
        ownerId = owner.GetField("OwnerId");
        handAssociationId = handAssociation.GetField("HandEntityId");
        discardAssociationId = discardAssociation.GetField("DiscardPileId");
        deckAssociationId = deckAssociation.GetField("DeckEntityId");
        discardCardIds = discardPile.GetField("CardIds");
        handCardIds = hand.GetField("CardIds");

        discardRemoveCard = discardModifierClass.GetMethod("RemoveCard", 2).cast<void>();
        handAddCard = handModifierClass.GetMethod("AddCard", 2).cast<void>();

        historyAddRecordByType = history.GetMethod(
            "AddRecord",
            { systemType.GetCompileTimeClass() }
        ).cast<Obj*>();
        cardIdDataCreateFromEntity =
            cardIdData.GetMethod("CreateFromEntity", 1).cast<Obj*>();
        drawRecordSetHandId =
            drawCardRecord.GetMethod("set_HandId", 1).cast<void>();
        drawRecordSetDeckId =
            drawCardRecord.GetMethod("set_DeckId", 1).cast<void>();
        drawRecordSetCard =
            drawCardRecord.GetMethod("set_Card", 1).cast<void>();
        effectRecordSetDefaultIds =
            effectRecord.GetMethod("SetDefaultIdsFromEffect", 1).cast<void>();

        auto descriptorToEffect = descriptor.GetMethod("ToEffect", 2);
        auto processEffect = system.GetMethod("ProcessEffect", 1);

        if (!effectTargetId.IsValid() || !runtimeDeckPosition.IsValid() ||
            !systemModel.IsValid() || !systemDiscardPileModifier.IsValid() ||
            !systemHandModifier.IsValid() || !systemHistory.IsValid() ||
            !modelEntities.IsValid() || !entityComponents.IsValid() ||
            !entityId.IsValid() || !ownerId.IsValid() ||
            !handAssociationId.IsValid() || !discardAssociationId.IsValid() ||
            !deckAssociationId.IsValid() || !discardCardIds.IsValid() ||
            !handCardIds.IsValid() || !discardRemoveCard.IsValid() ||
            !handAddCard.IsValid() || !historyAddRecordByType.IsValid() ||
            !cardIdDataCreateFromEntity.IsValid() ||
            !drawRecordSetHandId.IsValid() || !drawRecordSetDeckId.IsValid() ||
            !drawRecordSetCard.IsValid() || !effectRecordSetDefaultIds.IsValid() ||
            !descriptorToEffect.IsValid() || !processEffect.IsValid()) {
            LOG_ERR("CustomComponents/ReturnFromDiscard: required API missing");
            return false;
        }

        BNM::BasicHook(
            descriptorToEffect,
            DescriptorToEffectHook,
            originalDescriptorToEffect
        );
        BNM::BasicHook(
            processEffect,
            ProcessEffectHook,
            originalProcessEffect
        );

        if (!originalDescriptorToEffect || !originalProcessEffect) {
            LOG_ERR("CustomComponents/ReturnFromDiscard: hook installation failed");
            return false;
        }

        CC_DIAG(
            "CustomComponents/ReturnFromDiscard: ready (hand marker=%d; remove marker=%d; owns stock return hooks)",
            kReturnFromDiscardMarker,
            Components::RemoveFromDiscardPileEffectDescriptor::kRemoveFromDiscardSentinel
        );
        return true;
    }

    inline bool Initialize() {
        return RegisterType() && InstallHooks();
    }

} // namespace CustomComponents::Systems::ReturnFromDiscardPileSystem
