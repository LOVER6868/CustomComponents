#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "components/RemoveFromDiscardPileEffectDescriptor.hpp"
#include "systems/DiscardFromHandSystem.hpp"
#include "systems/DiscardedSystem.hpp"
#include "systems/DiscardFromHandPresentationSystem.hpp"
#include "components/ReturnFromDiscardPileEffectDescriptor.hpp"
#include "components/ReturnFromDiscardPileToDeckEffectDescriptor.hpp"
#include "components/ReturnFromDiscardPileToLaneEffectDescriptor.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "systems/CustomTriggerSystem.hpp"
#include "systems/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "log.h"

namespace CustomComponents::Systems::DiscardPileZoneEffectsSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    // ---------------------------------------------------------------------
    // Shared Effect / entity fields
    // ---------------------------------------------------------------------
    inline BNM::Field<int> effectTargetId;
    inline BNM::Field<int> effectSecondaryTargetId;
    inline BNM::Field<int> effectEffectEntityId;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;

    inline BNM::Field<int> discardAssociationId;
    inline BNM::Field<int> deckAssociationId;
    inline BNM::Field<int> handAssociationId;
    inline BNM::Field<MonoList<int>*> discardCardIds;
    inline BNM::Field<MonoList<int>*> handCardIds;

    inline BNM::Method<int> getPlayerIdFromCard;
    inline BNM::Method<void> discardRemoveCard;
    inline BNM::Method<void> handAddCard;
    inline BNM::Method<void> handRemoveContainingCard;
    inline BNM::Method<void> discardAddCard;
    inline BNM::Field<Obj*> handModifierEntities;
    inline BNM::Class handModifierClass;


    // A scratch DiscardPileModifier lets systems that do not inject one still
    // use the game's canonical RemoveCard implementation. Its only dependency
    // is EntityModel, which we set from the active stock system instance.

    inline BNM::Field<Obj*> discardModifierEntities;
    inline BNM::Class discardModifierClass;

    // ---------------------------------------------------------------------
    // RemoveFromDiscardPile routing
    // ---------------------------------------------------------------------
    inline BNM::Field<int> returnDeckPosition;
    inline BNM::Field<Obj*> returnSystemModel;
    inline BNM::Field<Obj*> returnSystemDiscardModifier;
    inline BNM::Field<Obj*> returnSystemHandModifier;
    inline BNM::Field<Obj*> returnSystemHistory;

    // ---------------------------------------------------------------------
    // ReturnFromDiscardPileToDeck routing
    // ---------------------------------------------------------------------
    inline BNM::Field<int> createDeckDescriptorPosition;
    inline BNM::Field<int> createDeckRuntimeCardGuid;
    inline BNM::Field<int> createDeckRuntimePosition;
    inline BNM::Field<Obj*> createDeckSystemModel;
    inline BNM::Field<Obj*> createDeckSystemModifier;
    inline BNM::Field<Obj*> createDeckSystemHistory;
    inline BNM::Method<void> deckAddCardAt;

    // ---------------------------------------------------------------------
    // ReturnFromDiscardPileToLane routing
    // ---------------------------------------------------------------------
    inline BNM::Field<int> moveLaneRuntimeOrder;
    inline BNM::Field<Obj*> moveLaneSystemModel;
    inline BNM::Field<Obj*> moveLaneSystemModifier;
    inline BNM::Field<Obj*> moveLaneSystemHistory;
    inline BNM::Method<void> lanePlayCard;

    // Stage I.3 read-only dependencies (stock LaneModifier / Lane / LaneElement).
    inline BNM::Field<Obj*> laneModifierModel;
    inline BNM::Field<MonoList<int>*> laneElementIds;
    inline BNM::Method<int> laneGetNumFightersForPlayer;
    inline BNM::Method<int> laneGetNumTotalFighters;


    // ---------------------------------------------------------------------
    // DiscardOutsidePlayTrigger event sources
    // ---------------------------------------------------------------------
    inline BNM::Field<Obj*> moveToDiscardSystemModel;
    inline BNM::Field<int> moveToDiscardEffectPileId;

    // ---------------------------------------------------------------------
    // History / frontend bridge
    // ---------------------------------------------------------------------
    inline BNM::Method<Obj*> historyAddRecordByType;

    inline BNM::Method<void> returnedDeckRecordSetCardId;
    inline BNM::Method<void> returnedDeckRecordSetDeckId;

    inline BNM::Method<Obj*> cardIdDataCreateFromEntity;
    inline BNM::Method<void> drawRecordSetHandId;
    inline BNM::Method<void> drawRecordSetDeckId;
    inline BNM::Method<void> drawRecordSetCard;
    inline BNM::Method<void> createLaneRecordSetCard;
    inline BNM::Method<void> createLaneRecordSetOrder;
    inline BNM::Method<void> createLaneRecordSetFaceDown;
    inline BNM::Method<void> effectRecordSetDefaultIds;
    inline BNM::Method<void> effectRecordSetTargetId;

    using DescriptorToEffectFn = Obj* (*)(void*, Obj*, Obj*, const void*);
    using ProcessEffectFn = void (*)(void*, Obj*, const void*);

    inline DescriptorToEffectFn originalReturnDescriptorToEffect = nullptr;
    inline DescriptorToEffectFn originalCreateDeckDescriptorToEffect = nullptr;
    inline DescriptorToEffectFn originalMoveLaneDescriptorToEffect = nullptr;

    inline ProcessEffectFn originalReturnProcessEffect = nullptr;
    inline ProcessEffectFn originalCreateDeckProcessEffect = nullptr;
    inline ProcessEffectFn originalMoveLaneProcessEffect = nullptr;
    inline ProcessEffectFn originalMoveToDiscardProcessEffect = nullptr;

    // Stage I diagnostic. Set to 0 only after lookup is confirmed safe.
    // When 1, matched custom lane effects are intentionally not executed.
    inline constexpr bool kStageIBypassCustomLaneTransfer = true;


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
            if (TypeName(component) == name)
                return component;
        }
        return nullptr;
    }

    // EntityId 0 is valid (notably the local player).
    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;
        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

    inline bool DiscardContains(Obj* discardPileEntity, int cardId) {
        auto* discard = FindComponent(discardPileEntity, "DiscardPile");
        if (!discard)
            return false;
        auto* ids = discardCardIds[discard].Get();
        if (!ids || !ids->items)
            return false;
        for (int i = 0; i < ids->size; ++i) {
            if (ids->items->m_Items[i] == cardId)
                return true;
        }
        return false;
    }

    inline Obj* ScratchDiscardModifier(Obj* model) {
        if (!model || !discardModifierClass.IsValid() || !discardModifierEntities.IsValid())
            return nullptr;

        // Never retain a raw IL2CPP pointer across matches or GC cycles.
        // The newly allocated modifier is used synchronously by the caller.
        Obj* modifier = discardModifierClass.CreateNewInstance();
        if (modifier)
            discardModifierEntities[modifier] = model;
        return modifier;
    }

    inline Obj* ScratchHandModifier(Obj* model) {
        if (!model || !handModifierClass.IsValid() || !handModifierEntities.IsValid())
            return nullptr;
        // Fresh temporary per operation; no unrooted process-global managed pointer.
        Obj* modifier = handModifierClass.CreateNewInstance();
        if (modifier)
            handModifierEntities[modifier] = model;
        return modifier;
    }

    struct DiscardContext {
        Obj* card = nullptr;
        Obj* player = nullptr;
        Obj* discardPile = nullptr;
        int cardId = -1;
        int playerId = -1;
        int discardPileId = -1;
    };

    inline bool ResolveDiscardContext(Obj* model, int cardId, DiscardContext& out) {
        out = {};
        out.cardId = cardId;

        Obj* card = FindEntity(model, cardId);
        if (!card)
            return false;

        const int playerId = getPlayerIdFromCard[static_cast<Obj*>(nullptr)](card);
        Obj* player = FindEntity(model, playerId);
        if (!player)
            return false;

        Obj* discardAssociation = FindComponent(player, "DiscardPileAssociation");
        if (!discardAssociation)
            return false;

        const int discardPileId = discardAssociationId[discardAssociation].Get();
        Obj* discardPile = FindEntity(model, discardPileId);
        if (!discardPile || !DiscardContains(discardPile, cardId))
            return false;

        out.card = card;
        out.player = player;
        out.discardPile = discardPile;
        out.playerId = playerId;
        out.discardPileId = discardPileId;
        return true;
    }

    // =====================================================================
    // Descriptor hooks
    // =====================================================================

    static Obj* ReturnDescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        auto* descriptor = static_cast<Obj*>(self);
        const std::string typeName = TypeName(descriptor);

        if (typeName == "ReturnFromDiscardPileEffectDescriptor") {
            Obj* runtime = Components::ReturnFromDiscardPileEffectDescriptor::CreateRuntime(instances);
            LOG_INFO(
                "CustomComponents/ReturnFromDiscardPile: descriptor -> runtime=%p",
                runtime
            );
            return runtime;
        }

        if (typeName == "RemoveFromDiscardPileEffectDescriptor") {
            Obj* runtime = Components::RemoveFromDiscardPileEffectDescriptor::CreateRuntime(instances);
            LOG_INFO(
                "CustomComponents/RemoveFromDiscardPile: descriptor -> runtime=%p",
                runtime
            );
            return runtime;
        }

        return originalReturnDescriptorToEffect(self, instances, effectEntity, method);
    }

    static Obj* CreateDeckDescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        auto* descriptor = static_cast<Obj*>(self);
        if (TypeName(descriptor) != "ReturnFromDiscardPileToDeckEffectDescriptor") {
            return originalCreateDeckDescriptorToEffect(self, instances, effectEntity, method);
        }

        const int position = createDeckDescriptorPosition[descriptor].Get();
        Obj* runtime = Components::ReturnFromDiscardPileToDeckEffectDescriptor::CreateRuntime(
            instances,
            position
        );
        LOG_INFO(
            "CustomComponents/ReturnFromDiscardPileToDeck: descriptor -> runtime=%p DeckPosition=%d",
            runtime,
            position
        );
        return runtime;
    }

    static Obj* MoveLaneDescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        LOG_INFO("CustomComponents/LANE-DIAG: DTE ENTER self=%p instances=%p effectEntity=%p method=%p", self, instances, effectEntity, method);
        auto* descriptor = static_cast<Obj*>(self);
        LOG_INFO("CustomComponents/LANE-DIAG: DTE before TypeName descriptor=%p", descriptor);
        const std::string descriptorType = TypeName(descriptor);
        LOG_INFO("CustomComponents/LANE-DIAG: DTE type=%s", descriptorType.c_str());
        if (descriptorType != "ReturnFromDiscardPileToLaneEffectDescriptor") {
            LOG_INFO("CustomComponents/LANE-DIAG: DTE stock delegate ENTER");
            Obj* stock = originalMoveLaneDescriptorToEffect(self, instances, effectEntity, method);
            LOG_INFO("CustomComponents/LANE-DIAG: DTE stock delegate EXIT runtime=%p", stock);
            return stock;
        }
        LOG_INFO("CustomComponents/LANE-DIAG: DTE custom CreateRuntime ENTER");

        Obj* runtime = Components::ReturnFromDiscardPileToLaneEffectDescriptor::CreateRuntime(instances);
        LOG_INFO("CustomComponents/LANE-DIAG: DTE custom CreateRuntime EXIT runtime=%p", runtime);
        LOG_INFO(
            "CustomComponents/ReturnFromDiscardPileToLane: descriptor -> runtime=%p (lane supplied by SecondaryTargetId)",
            runtime
        );
        return runtime;
    }

    // =====================================================================
    // 1. RemoveFromDiscardPile
    // =====================================================================

    inline bool HandContains(Obj* handEntity, int cardId) {
        auto* hand = FindComponent(handEntity, "Hand");
        if (!hand)
            return false;
        auto* ids = handCardIds[hand].Get();
        if (!ids || !ids->items)
            return false;
        for (int i = 0; i < ids->size; ++i) {
            if (ids->items->m_Items[i] == cardId)
                return true;
        }
        return false;
    }


    inline bool CardIsInAnyHand(Obj* model, int cardId) {
        if (!model || cardId < 0)
            return false;

        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && HandContains(entity, cardId))
                return true;
        }
        return false;
    }

    static void ReturnProcessEffectHook(void* self, Obj* effect, const void* method) {
        if (!effect) {
            originalReturnProcessEffect(self, effect, method);
            return;
        }

        const int route = returnDeckPosition[effect].Get();
        const bool isHandReturn =
            route == Components::ReturnFromDiscardPileEffectDescriptor::kReturnFromDiscardSentinel;
        const bool isRemoveOnly =
            route == Components::RemoveFromDiscardPileEffectDescriptor::kRemoveFromDiscardSentinel;

        if (!isHandReturn && !isRemoveOnly) {
            originalReturnProcessEffect(self, effect, method);
            return;
        }

        auto* system = static_cast<Obj*>(self);
        Obj* model = returnSystemModel[system].Get();
        Obj* discardModifier = returnSystemDiscardModifier[system].Get();
        const int cardId = effectTargetId[effect].Get();

        if (!model || !discardModifier || cardId < 0) {
            LOG_ERR(
                "CustomComponents/DiscardPileZoneEffects: return-router missing state model=%p discardModifier=%p TargetId=%d route=%d",
                model, discardModifier, cardId, route
            );
            return;
        }

        DiscardContext ctx;
        if (!ResolveDiscardContext(model, cardId, ctx)) {
            LOG_ERR(
                "CustomComponents/DiscardPileZoneEffects: TargetId=%d is not a live card in its owner's discard pile (route=%d)",
                cardId, route
            );
            return;
        }

        if (isRemoveOnly) {
            discardRemoveCard[discardModifier](cardId, ctx.discardPileId);

            LOG_INFO(
                "CustomComponents/RemoveFromDiscardPile: SUCCESS EntityId=%d PlayerId=%d DiscardPileId=%d (entity preserved; no destination)",
                cardId, ctx.playerId, ctx.discardPileId
            );
            return;
        }

        // ReturnFromDiscardPile -> Hand.
        // Gameplay is still discard -> hand on the SAME live entity.
        // DrawCardRecord is frontend materialization only.
        Obj* handModifier = returnSystemHandModifier[system].Get();
        Obj* history = returnSystemHistory[system].Get();

        Obj* handAssociation = FindComponent(ctx.player, "HandAssociation");
        Obj* deckAssociation = FindComponent(ctx.player, "DeckAssociation");

        if (!handModifier || !history || !handAssociation || !deckAssociation) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPile: missing state handModifier=%p history=%p handAssoc=%p deckAssoc=%p",
                handModifier, history, handAssociation, deckAssociation
            );
            return;
        }

        const int handId = handAssociationId[handAssociation].Get();
        const int deckId = deckAssociationId[deckAssociation].Get();
        Obj* handEntity = FindEntity(model, handId);

        if (!handEntity) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPile: unresolved HandId=%d for PlayerId=%d",
                handId, ctx.playerId
            );
            return;
        }

        auto* discardBefore = FindComponent(ctx.discardPile, "DiscardPile");
        auto* handBefore = FindComponent(handEntity, "Hand");
        auto* discardBeforeIds = discardBefore ? discardCardIds[discardBefore].Get() : nullptr;
        auto* handBeforeIds = handBefore ? handCardIds[handBefore].Get() : nullptr;
        const int beforeDiscardCount = discardBeforeIds ? discardBeforeIds->size : -1;
        const int beforeHandCount = handBeforeIds ? handBeforeIds->size : -1;
        const bool handContainedBefore = HandContains(handEntity, cardId);

        discardRemoveCard[discardModifier](cardId, ctx.discardPileId);
        handAddCard[handModifier](handEntity, ctx.card);

        auto recordClass = BNM::Class(
            "PvZCards.Engine.Records",
            "DrawCardRecord"
        );
        Obj* drawRecord = historyAddRecordByType[history](
            reinterpret_cast<Obj*>(recordClass.GetMonoType())
        );

        if (drawRecord) {
            Obj* cardData =
                cardIdDataCreateFromEntity[static_cast<Obj*>(nullptr)](ctx.card);

            if (cardData) {
                drawRecordSetHandId[drawRecord](handId);
                drawRecordSetDeckId[drawRecord](deckId); // visual anchor only
                drawRecordSetCard[drawRecord](cardData);
                effectRecordSetDefaultIds[drawRecord](effect);

                LOG_INFO(
                    "CustomComponents/ReturnFromDiscardPile: FRONTEND DRAW-STYLE EntityId=%d HandId=%d VisualDeckId=%d record=%p cardData=%p (presentation only; no draw/deck mutation; no reset)",
                    cardId, handId, deckId, drawRecord, cardData
                );
            } else {
                LOG_ERR(
                    "CustomComponents/ReturnFromDiscardPile: CardIdData.CreateFromEntity failed for EntityId=%d",
                    cardId
                );
            }
        } else {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPile: failed to create DrawCardRecord for EntityId=%d",
                cardId
            );
        }

        auto* discardAfter = FindComponent(ctx.discardPile, "DiscardPile");
        auto* handAfter = FindComponent(handEntity, "Hand");
        auto* discardAfterIds = discardAfter ? discardCardIds[discardAfter].Get() : nullptr;
        auto* handAfterIds = handAfter ? handCardIds[handAfter].Get() : nullptr;
        const int afterDiscardCount = discardAfterIds ? discardAfterIds->size : -1;
        const int afterHandCount = handAfterIds ? handAfterIds->size : -1;
        const bool handContainsAfter = HandContains(handEntity, cardId);

        LOG_INFO(
            "CustomComponents/ReturnFromDiscardPile: SUCCESS EntityId=%d PlayerId=%d DiscardPileId=%d HandId=%d discard=%d->%d hand=%d->%d contains=%d->%d (same live entity)",
            cardId, ctx.playerId, ctx.discardPileId, handId,
            beforeDiscardCount, afterDiscardCount,
            beforeHandCount, afterHandCount,
            handContainedBefore ? 1 : 0,
            handContainsAfter ? 1 : 0
        );

        CustomTriggerSystem::FireReturnedFromDiscardPile(effect);
    }

    // =====================================================================
    // 2. ReturnFromDiscardPileToDeck
    // =====================================================================

    static void CreateDeckProcessEffectHook(void* self, Obj* effect, const void* method) {
        if (!effect ||
            createDeckRuntimeCardGuid[effect].Get() !=
                Components::ReturnFromDiscardPileToDeckEffectDescriptor::kExistingEntityDeckSentinel) {
            originalCreateDeckProcessEffect(self, effect, method);
            return;
        }

        auto* system = static_cast<Obj*>(self);
        Obj* model = createDeckSystemModel[system].Get();
        Obj* deckModifier = createDeckSystemModifier[system].Get();
        Obj* history = createDeckSystemHistory[system].Get();
        const int cardId = effectTargetId[effect].Get();
        const int deckPosition = createDeckRuntimePosition[effect].Get();

        if (!model || !deckModifier || !history || cardId < 0) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPileToDeck: missing state model=%p deckModifier=%p history=%p TargetId=%d",
                model,
                deckModifier,
                history,
                cardId
            );
            return;
        }

        DiscardContext ctx;
        if (!ResolveDiscardContext(model, cardId, ctx)) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPileToDeck: TargetId=%d is not a live card in its owner's discard pile",
                cardId
            );
            return;
        }

        Obj* deckAssociation = FindComponent(ctx.player, "DeckAssociation");
        if (!deckAssociation) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPileToDeck: PlayerId=%d missing DeckAssociation",
                ctx.playerId
            );
            return;
        }

        const int deckId = deckAssociationId[deckAssociation].Get();
        Obj* deckEntity = FindEntity(model, deckId);
        Obj* orderedPool = FindComponent(deckEntity, "OrderedCardPool");
        Obj* discardModifier = ScratchDiscardModifier(model);

        if (!deckEntity || !orderedPool || !discardModifier) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPileToDeck: unresolved deck/pool/modifier DeckId=%d deck=%p pool=%p discardModifier=%p",
                deckId,
                deckEntity,
                orderedPool,
                discardModifier
            );
            return;
        }

        // Match stock zone-transfer ordering: add the SAME live entity to the
        // destination first, then remove its old discard membership.
        deckAddCardAt[deckModifier](ctx.card, orderedPool, deckPosition);
        discardRemoveCard[discardModifier](cardId, ctx.discardPileId);

        // Deck is an information-only zone. This stock record is bookkeeping /
        // frontend notification only; no card is created and no reset occurs.
        auto recordClass = BNM::Class(
            "PvZCards.Engine.Records",
            "CardReturnedToPlayersDeckRecord"
        );
        Obj* record = historyAddRecordByType[history](
            reinterpret_cast<Obj*>(recordClass.GetMonoType())
        );
        if (record) {
            returnedDeckRecordSetCardId[record](cardId);
            returnedDeckRecordSetDeckId[record](deckId);
        }

        LOG_INFO(
            "CustomComponents/ReturnFromDiscardPileToDeck: SUCCESS EntityId=%d PlayerId=%d DiscardPileId=%d DeckId=%d DeckPosition=%d (same live entity; no reset)",
            cardId,
            ctx.playerId,
            ctx.discardPileId,
            deckId,
            deckPosition
        );

        CustomTriggerSystem::FireReturnedFromDiscardPile(effect);
    }

    // =====================================================================
    // 3. ReturnFromDiscardPileToLane
    // =====================================================================

    static void MoveLaneProcessEffectHook(void* self, Obj* effect, const void* method) {
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS ENTER self=%p effect=%p method=%p", self, effect, method);
        if (!effect || !self) {
            LOG_INFO("CustomComponents/LANE-DIAG: PROCESS null argument; stock delegation ENTER");
            originalMoveLaneProcessEffect(self, effect, method);
            LOG_INFO("CustomComponents/LANE-DIAG: PROCESS null argument; stock delegation EXIT");
            return;
        }

        auto* system = static_cast<Obj*>(self);
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS before model field read system=%p", system);
        Obj* model = moveLaneSystemModel[system].Get();
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS model=%p", model);

        // Do NOT use MoveCardToLanesEffect.OrderInLane as the routing tag here.
        // The secondary-target pipeline can rebuild/deep-copy the runtime effect,
        // so a borrowed stock field is not a stable identity marker. Instead,
        // identify the effect from its originating EffectEntity and the custom
        // descriptor component that produced it.
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS before EffectEntityId read effect=%p", effect);
        const int sourceEffectEntityId = effectEffectEntityId[effect].Get();
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS EffectEntityId=%d before FindEntity model=%p", sourceEffectEntityId, model);
        if (!model || sourceEffectEntityId < 0) {
            LOG_INFO("CustomComponents/LANE-DIAG: PROCESS invalid model/id; stock delegation ENTER");
            originalMoveLaneProcessEffect(self, effect, method);
            LOG_INFO("CustomComponents/LANE-DIAG: PROCESS invalid model/id; stock delegation EXIT");
            return;
        }
        Obj* sourceEffectEntity = FindEntity(model, sourceEffectEntityId);
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS FindEntity EXIT entity=%p before FindComponent", sourceEffectEntity);
        Obj* customLaneDescriptor = FindComponent(
            sourceEffectEntity,
            "ReturnFromDiscardPileToLaneEffectDescriptor"
        );
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS FindComponent EXIT descriptor=%p", customLaneDescriptor);

        if (!customLaneDescriptor) {
            LOG_INFO("CustomComponents/LANE-DIAG: PROCESS stock delegate ENTER");
            originalMoveLaneProcessEffect(self, effect, method);
            LOG_INFO("CustomComponents/LANE-DIAG: PROCESS stock delegate EXIT");
            return;
        }

        // Stage I.2: read-only preflight. NO zone mutations, record creation,
        // scratch modifier allocation, or custom trigger dispatch in this block.
        // All checkpoints are deliberately separated to locate a native fault.
        LOG_INFO("CustomComponents/LANE-I2: PREFLIGHT ENTER effect=%p model=%p system=%p", effect, model, system);
        const int preflightCardId = effectTargetId[effect].Get();
        const int preflightLaneId = effectSecondaryTargetId[effect].Get();
        LOG_INFO("CustomComponents/LANE-I2: TARGETS cardId=%d laneId=%d effectEntityId=%d", preflightCardId, preflightLaneId, sourceEffectEntityId);

        LOG_INFO("CustomComponents/LANE-I2: SYSTEM before modifier/history reads");
        Obj* preflightLaneModifier = moveLaneSystemModifier[system].Get();
        Obj* preflightHistory = moveLaneSystemHistory[system].Get();
        LOG_INFO("CustomComponents/LANE-I2: SYSTEM laneModifier=%p history=%p playCardValid=%d discardRemoveValid=%d historyAddValid=%d scratchClassValid=%d scratchFieldValid=%d",
                 preflightLaneModifier, preflightHistory, lanePlayCard.IsValid() ? 1 : 0,
                 discardRemoveCard.IsValid() ? 1 : 0, historyAddRecordByType.IsValid() ? 1 : 0,
                 discardModifierClass.IsValid() ? 1 : 0, discardModifierEntities.IsValid() ? 1 : 0);

        DiscardContext preflightCtx;
        bool preflightDiscardResolved = false;
        if (preflightCardId >= 0) {
            LOG_INFO("CustomComponents/LANE-I2: DISCARD before ResolveDiscardContext cardId=%d", preflightCardId);
            preflightDiscardResolved = ResolveDiscardContext(model, preflightCardId, preflightCtx);
            LOG_INFO("CustomComponents/LANE-I2: DISCARD after ResolveDiscardContext ok=%d card=%p player=%p pile=%p playerId=%d pileId=%d",
                     preflightDiscardResolved ? 1 : 0, preflightCtx.card, preflightCtx.player,
                     preflightCtx.discardPile, preflightCtx.playerId, preflightCtx.discardPileId);
        } else {
            LOG_INFO("CustomComponents/LANE-I2: DISCARD skipped invalid target id=%d", preflightCardId);
        }

        Obj* preflightLaneEntity = nullptr;
        Obj* preflightLaneComponent = nullptr;
        if (preflightLaneId >= 0) {
            LOG_INFO("CustomComponents/LANE-I2: LANE before FindEntity laneId=%d", preflightLaneId);
            preflightLaneEntity = FindEntity(model, preflightLaneId);
            LOG_INFO("CustomComponents/LANE-I2: LANE after FindEntity entity=%p", preflightLaneEntity);
            if (preflightLaneEntity) {
                LOG_INFO("CustomComponents/LANE-I2: LANE before FindComponent");
                preflightLaneComponent = FindComponent(preflightLaneEntity, "Lane");
                LOG_INFO("CustomComponents/LANE-I2: LANE after FindComponent component=%p", preflightLaneComponent);
            }
        } else {
            LOG_INFO("CustomComponents/LANE-I2: LANE skipped invalid lane id=%d", preflightLaneId);
        }
        // Stage I.3: inspect stock placement dependencies without invoking any
        // mutating LaneModifier methods or touching Lane's dictionary storage.
        Obj* i3ModifierModel = nullptr;
        if (preflightLaneModifier && laneModifierModel.IsValid()) {
            LOG_INFO("CustomComponents/LANE-I3: MODIFIER before Entities read modifier=%p", preflightLaneModifier);
            i3ModifierModel = laneModifierModel[preflightLaneModifier].Get();
            LOG_INFO("CustomComponents/LANE-I3: MODIFIER Entities=%p System.Entities=%p match=%d",
                     i3ModifierModel, model, i3ModifierModel == model ? 1 : 0);
        } else {
            LOG_INFO("CustomComponents/LANE-I3: MODIFIER unavailable modifier=%p fieldValid=%d",
                     preflightLaneModifier, laneModifierModel.IsValid() ? 1 : 0);
        }

        Obj* i3LaneElement = preflightDiscardResolved
            ? FindComponent(preflightCtx.card, "LaneElement") : nullptr;
        LOG_INFO("CustomComponents/LANE-I3: CARD LaneElement=%p fieldValid=%d",
                 i3LaneElement, laneElementIds.IsValid() ? 1 : 0);
        int i3LaneIdCount = -1;
        bool i3CardAlreadyInTarget = false;
        if (i3LaneElement && laneElementIds.IsValid()) {
            LOG_INFO("CustomComponents/LANE-I3: CARD before LaneIds field read");
            auto* ids = laneElementIds[i3LaneElement].Get();
            LOG_INFO("CustomComponents/LANE-I3: CARD LaneIds list=%p", ids);
            if (ids && ids->items && ids->size >= 0 && ids->size <= 32) {
                i3LaneIdCount = ids->size;
                for (int i = 0; i < i3LaneIdCount; ++i) {
                    const int oldLaneId = ids->items->m_Items[i];
                    if (oldLaneId == preflightLaneId) i3CardAlreadyInTarget = true;
                    LOG_INFO("CustomComponents/LANE-I3: CARD LaneIds[%d]=%d", i, oldLaneId);
                }
            } else {
                LOG_INFO("CustomComponents/LANE-I3: CARD LaneIds null or unexpected count (no iteration)");
            }
        }
        LOG_INFO("CustomComponents/LANE-I3: CARD RESULT laneIdCount=%d alreadyInTarget=%d",
                 i3LaneIdCount, i3CardAlreadyInTarget ? 1 : 0);

        int i3OwnerFighters = -1;
        int i3TotalFighters = -1;
        if (preflightLaneComponent && preflightDiscardResolved &&
            laneGetNumFightersForPlayer.IsValid() && laneGetNumTotalFighters.IsValid()) {
            LOG_INFO("CustomComponents/LANE-I3: OCCUPANCY before GetNumFightersForPlayer playerId=%d",
                     preflightCtx.playerId);
            i3OwnerFighters = laneGetNumFightersForPlayer[preflightLaneComponent](preflightCtx.playerId);
            LOG_INFO("CustomComponents/LANE-I3: OCCUPANCY ownerFighters=%d before GetNumTotalFighters", i3OwnerFighters);
            i3TotalFighters = laneGetNumTotalFighters[preflightLaneComponent]();
            LOG_INFO("CustomComponents/LANE-I3: OCCUPANCY totalFighters=%d", i3TotalFighters);
        } else {
            LOG_INFO("CustomComponents/LANE-I3: OCCUPANCY unavailable lane=%p playerResolved=%d ownerMethod=%d totalMethod=%d",
                     preflightLaneComponent, preflightDiscardResolved ? 1 : 0,
                     laneGetNumFightersForPlayer.IsValid() ? 1 : 0,
                     laneGetNumTotalFighters.IsValid() ? 1 : 0);
        }
        LOG_INFO("CustomComponents/LANE-I3: SUMMARY modelMatch=%d laneElement=%p cardLaneCount=%d alreadyTarget=%d ownerFighters=%d totalFighters=%d (observational only)",
                 i3ModifierModel && i3ModifierModel == model ? 1 : 0, i3LaneElement,
                 i3LaneIdCount, i3CardAlreadyInTarget ? 1 : 0, i3OwnerFighters, i3TotalFighters);

        // Stage I.4: read-only zone-component and frontend provenance inspection.
        // No mutation, no provenance registration and no record creation.
        if (preflightDiscardResolved) {
            const char* const zoneNames[] = {
                "Card", "Owner", "DiscardPileElement", "HandElement",
                "DeckElement", "LaneElement", "Attack", "Health",
                "SunCost", "Plant", "Zombie", "Fighter"
            };
            LOG_INFO("CustomComponents/LANE-I4: CARD-COMPONENTS BEGIN card=%d entity=%p",
                     preflightCardId, preflightCtx.card);
            for (const char* name : zoneNames) {
                Obj* found = FindComponent(preflightCtx.card, name);
                LOG_INFO("CustomComponents/LANE-I4: CARD-COMPONENT name=%s present=%d ptr=%p",
                         name, found ? 1 : 0, found);
            }
            Obj* discardElement = FindComponent(preflightCtx.card, "DiscardPileElement");
            Obj* handElement = FindComponent(preflightCtx.card, "HandElement");
            Obj* deckElement = FindComponent(preflightCtx.card, "DeckElement");
            const bool actualDiscardMembership = DiscardContains(preflightCtx.discardPile, preflightCardId);
            LOG_INFO("CustomComponents/LANE-I4: ZONE-SUMMARY card=%d pileId=%d pileContains=%d discardElement=%d handElement=%d deckElement=%d laneElement=%d",
                     preflightCardId, preflightCtx.discardPileId, actualDiscardMembership ? 1 : 0,
                     discardElement ? 1 : 0, handElement ? 1 : 0,
                     deckElement ? 1 : 0, i3LaneElement ? 1 : 0);
            const bool exactOrigin =
                ZoneTriggeredAbilityPresentationSystem::IsDiscardToLaneOrigin(
                    model, preflightCardId, sourceEffectEntityId);
            const bool anyOrigin =
                ZoneTriggeredAbilityPresentationSystem::IsDiscardToLaneOrigin(
                    model, preflightCardId);
            LOG_INFO("CustomComponents/LANE-I4: PRESENTATION-ORIGIN card=%d effectEntityId=%d exactRegistered=%d anyRegistered=%d (peek only, no registration)",
                     preflightCardId, sourceEffectEntityId,
                     exactOrigin ? 1 : 0, anyOrigin ? 1 : 0);
        } else {
            LOG_INFO("CustomComponents/LANE-I4: SKIP no valid discard context card=%d", preflightCardId);
        }

        const bool preflightReady = preflightDiscardResolved && preflightLaneComponent &&
            preflightLaneModifier && preflightHistory && lanePlayCard.IsValid() &&
            discardRemoveCard.IsValid() && historyAddRecordByType.IsValid() &&
            discardModifierClass.IsValid() && discardModifierEntities.IsValid();
        LOG_INFO("CustomComponents/LANE-I2: PREFLIGHT RESULT ready=%d cardId=%d laneId=%d playerId=%d (read-only)",
                 preflightReady ? 1 : 0, preflightCardId, preflightLaneId, preflightCtx.playerId);

        if (kStageIBypassCustomLaneTransfer) {
            LOG_INFO("CustomComponents/LANE-DIAG: PROCESS CUSTOM BYPASS effect=%p sourceEffectEntityId=%d descriptor=%p (Stage I.4 read-only; NO lane transfer)", effect, sourceEffectEntityId, customLaneDescriptor);
            return;
        }
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS custom transfer ENTER");

        Obj* laneModifier = moveLaneSystemModifier[system].Get();
        Obj* history = moveLaneSystemHistory[system].Get();

        LOG_INFO(
            "CustomComponents/ReturnFromDiscardPileToLane: PROCESS matched EffectEntityId=%d effect=%p descriptor=%p",
            sourceEffectEntityId,
            effect,
            customLaneDescriptor
        );

        const int cardId = effectTargetId[effect].Get();
        const int laneId = effectSecondaryTargetId[effect].Get();

        if (!model || !laneModifier || !history || cardId < 0 || laneId < 0) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPileToLane: missing state model=%p laneModifier=%p history=%p TargetId=%d SecondaryTargetId=%d",
                model,
                laneModifier,
                history,
                cardId,
                laneId
            );
            return;
        }

        DiscardContext ctx;
        if (!ResolveDiscardContext(model, cardId, ctx)) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPileToLane: TargetId=%d is not a live card in its owner's discard pile",
                cardId
            );
            return;
        }

        Obj* laneEntity = FindEntity(model, laneId);
        Obj* laneComponent = FindComponent(laneEntity, "Lane");
        Obj* discardModifier = ScratchDiscardModifier(model);
        if (!laneEntity || !laneComponent || !discardModifier) {
            LOG_ERR(
                "CustomComponents/ReturnFromDiscardPileToLane: invalid secondary lane LaneId=%d lane=%p component=%p discardModifier=%p",
                laneId,
                laneEntity,
                laneComponent,
                discardModifier
            );
            return;
        }

        // Stock overload PlayCardInLane(cardId, laneId, playerId) creates a
        // LanePlacementData with OrderInLane=0 (front). This changes only the
        // existing entity's lane membership; it does not manufacture a card.
        constexpr int kOrderInLane = 0;
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS before lanePlayCard card=%d lane=%d player=%d", cardId, laneId, ctx.playerId);
        lanePlayCard[laneModifier](cardId, laneId, ctx.playerId);
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS after lanePlayCard before discardRemoveCard");
        discardRemoveCard[discardModifier](cardId, ctx.discardPileId);
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS after discardRemoveCard before history record");

        // FRONTEND ONLY: a discard pile has no active CardView. Reuse the stock
        // CreateCardInLaneRecord presentation path because it materializes a
        // CardView "from nothing". CardIdData still points at the SAME existing
        // live entity. No CreateCardSystem / CardEntityFactory is invoked.
        auto recordClass = BNM::Class(
            "PvZCards.Engine.Records",
            "CreateCardInLaneRecord"
        );
        Obj* record = historyAddRecordByType[history](
            reinterpret_cast<Obj*>(recordClass.GetMonoType())
        );

        if (record) {
            Obj* cardData = cardIdDataCreateFromEntity[static_cast<Obj*>(nullptr)](ctx.card);
            if (cardData) {
                createLaneRecordSetCard[record](cardData);
                createLaneRecordSetOrder[record](kOrderInLane);
                createLaneRecordSetFaceDown[record](false);

                // Preserve source/effect/group/player IDs, including the native
                // SecondaryTargetId selected by the targeting system. Then make
                // TargetId the lane for CreateCardInLane's frontend processor.
                effectRecordSetDefaultIds[record](effect);
                effectRecordSetTargetId[record](laneId);
            }
        }

        LOG_INFO(
            "CustomComponents/ReturnFromDiscardPileToLane: SUCCESS EntityId=%d PlayerId=%d DiscardPileId=%d LaneId=%d Order=%d (same live entity; SecondaryTargetId lane; create-in-lane frontend only; no reset)",
            cardId,
            ctx.playerId,
            ctx.discardPileId,
            laneId,
            kOrderInLane
        );


        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS before return trigger");
        CustomTriggerSystem::FireReturnedFromDiscardPile(effect);
        LOG_INFO("CustomComponents/LANE-DIAG: PROCESS EXIT");
    }


    // =====================================================================
    // 4. DiscardOutsidePlayTrigger
    // =====================================================================

    // Stage 2.1: observational snapshot of the target owner's hand/discard zones.
    // Do not retain managed pointers beyond the hook invocation.
    struct HandDiscardSnapshot {
        bool entityExists = false;
        bool anyHandContains = false;
        bool ownerHandContains = false;
        bool pileContains = false;
        int ownerId = -1;
        int handId = -1;
        int pileId = -1;
        int handCount = -1;
        int pileCount = -1;
    };

    inline HandDiscardSnapshot SnapshotHandDiscard(Obj* model, int cardId) {
        HandDiscardSnapshot state;
        if (!model || cardId < 0) return state;
        Obj* card = FindEntity(model, cardId);
        state.entityExists = card != nullptr;
        state.anyHandContains = CardIsInAnyHand(model, cardId);
        if (!card) return state;

        state.ownerId = getPlayerIdFromCard[static_cast<Obj*>(nullptr)](card);
        Obj* owner = FindEntity(model, state.ownerId);
        Obj* handAssociation = FindComponent(owner, "HandAssociation");
        Obj* pileAssociation = FindComponent(owner, "DiscardPileAssociation");
        if (handAssociation) {
            state.handId = handAssociationId[handAssociation].Get();
            Obj* handEntity = FindEntity(model, state.handId);
            Obj* handComponent = FindComponent(handEntity, "Hand");
            auto* ids = handComponent ? handCardIds[handComponent].Get() : nullptr;
            state.handCount = ids ? ids->size : -1;
            state.ownerHandContains = HandContains(handEntity, cardId);
        }
        if (pileAssociation) {
            state.pileId = discardAssociationId[pileAssociation].Get();
            Obj* pileEntity = FindEntity(model, state.pileId);
            Obj* pileComponent = FindComponent(pileEntity, "DiscardPile");
            auto* ids = pileComponent ? discardCardIds[pileComponent].Get() : nullptr;
            state.pileCount = ids ? ids->size : -1;
            state.pileContains = DiscardContains(pileEntity, cardId);
        }
        return state;
    }

    inline void LogHandDiscardSnapshot(const char* phase, int cardId,
                                       const HandDiscardSnapshot& s) {
        LOG_INFO(
            "CustomComponents/DiscardFromHand/DIAG: %s card=%d entityExists=%d owner=%d "
            "handId=%d handCount=%d inOwnerHand=%d inAnyHand=%d "
            "pileId=%d pileCount=%d inPile=%d",
            phase, cardId, (int)s.entityExists, s.ownerId,
            s.handId, s.handCount, (int)s.ownerHandContains,
            (int)s.anyHandContains, s.pileId, s.pileCount, (int)s.pileContains
        );
    }

    static void MoveToDiscardProcessEffectHook(
        void* self,
        Obj* effect,
        const void* method
    ) {
        auto* system = static_cast<Obj*>(self);
        Obj* model =
            system && moveToDiscardSystemModel.IsValid()
                ? moveToDiscardSystemModel[system].Get()
                : nullptr;
        const int cardId =
            effect && effectTargetId.IsValid()
                ? effectTargetId[effect].Get()
                : -1;

        // Stage 2.2: custom descriptor only. Do not modify stock effect routing.
        const bool customHandRoute = effect && moveToDiscardEffectPileId.IsValid() &&
            moveToDiscardEffectPileId[effect].Get() == -1;

        if (customHandRoute) {
            const auto before = SnapshotHandDiscard(model, cardId);
            LogHandDiscardSnapshot("BEFORE-TRANSFER", cardId, before);

            // Restrict operation to the target's *own* hand, with valid associations.
            // This avoids cross-owner moves and double-discarding.
            if (!before.entityExists || !before.ownerHandContains ||
                !before.anyHandContains || before.pileContains ||
                before.handId < 0 || before.pileId < 0 ||
                before.handCount < 0 || before.pileCount < 0 ||
                !FindEntity(model, before.pileId)) {
                LOG_ERR("CustomComponents/DiscardFromHand: REJECT card=%d invalid origin/destination", cardId);
                return;
            }
            Obj* handModifier = ScratchHandModifier(model);
            Obj* discardModifier = ScratchDiscardModifier(model);
            if (!handModifier || !discardModifier ||
                !handRemoveContainingCard.IsValid() || !discardAddCard.IsValid()) {
                LOG_ERR("CustomComponents/DiscardFromHand: REJECT card=%d modifier/API unavailable hand=%p pile=%p", cardId, handModifier, discardModifier);
                return;
            }

            // Stage 2.2C: hand FIRST. Adding to the discard pile before
            // RemoveCardFromContainingHand can prevent that modifier from
            // locating the card in its containing hand.
            // The original card entity is never recreated or destroyed.
            Obj* originalCard = FindEntity(model, cardId);
            Obj* originalHand = FindEntity(model, before.handId);
            if (!originalCard || !originalHand) {
                LOG_ERR("CustomComponents/DiscardFromHand: REJECT card=%d missing recovery prerequisites", cardId);
                return;
            }

            LOG_INFO("CustomComponents/DiscardFromHand/LIFETIME: BEFORE HAND REMOVE model=%p handModifier=%p pileModifier=%p card=%d", model, handModifier, discardModifier, cardId);
            handRemoveContainingCard[handModifier](cardId);
            LOG_INFO("CustomComponents/DiscardFromHand/LIFETIME: AFTER HAND REMOVE card=%d", cardId);
            const auto afterHandRemoval = SnapshotHandDiscard(model, cardId);
            LogHandDiscardSnapshot("AFTER-HAND-REMOVE", cardId, afterHandRemoval);
            const bool handRemoved = afterHandRemoval.entityExists &&
                afterHandRemoval.ownerId == before.ownerId &&
                afterHandRemoval.handId == before.handId &&
                !afterHandRemoval.anyHandContains &&
                !afterHandRemoval.ownerHandContains &&
                !afterHandRemoval.pileContains &&
                afterHandRemoval.handCount == before.handCount - 1 &&
                afterHandRemoval.pileCount == before.pileCount;
            if (!handRemoved) {
                LOG_ERR("CustomComponents/DiscardFromHand: HAND REMOVE INCOMPLETE card=%d; discard addition skipped", cardId);
                LOG_ERR("CustomComponents/DiscardFromHand: non-atomic state; no speculative rollback attempted");
                return;
            }

            LOG_INFO("CustomComponents/DiscardFromHand/LIFETIME: BEFORE PILE ADD card=%d", cardId);
            discardAddCard[discardModifier](cardId, before.pileId);
            LOG_INFO("CustomComponents/DiscardFromHand/LIFETIME: AFTER PILE ADD card=%d", cardId);
            const auto after = SnapshotHandDiscard(model, cardId);
            LogHandDiscardSnapshot("AFTER-TRANSFER", cardId, after);
            const bool success = after.entityExists &&
                !after.anyHandContains && after.pileContains &&
                after.ownerId == before.ownerId &&
                after.pileId == before.pileId &&
                after.handCount == before.handCount - 1 &&
                after.pileCount == before.pileCount + 1;
            LOG_INFO("CustomComponents/DiscardFromHand/DIAG: TRANSFER card=%d success=%d hand=%d->%d pile=%d->%d sameEntity=%d",
                cardId, (int)success, before.handCount, after.handCount,
                before.pileCount, after.pileCount, (int)after.entityExists);
            if (!success) {
                LOG_ERR("CustomComponents/DiscardFromHand: TRANSFER FAILED/INCOMPLETE card=%d; suppress presentation and trigger", cardId);
                LOG_ERR("CustomComponents/DiscardFromHand: non-atomic state; no speculative rollback attempted");
                return;
            }

            // Set origin on the SAME live entity while it is verifiably in pile.
            // This must happen before the custom discard trigger is fired.
            const bool markedHand = DiscardedSystem::Mark(
                model, cardId, DiscardedSystem::Source::Hand);
            const int actualSource = DiscardedSystem::GetSource(model, cardId);
            LOG_INFO("CustomComponents/DiscardFromHand/DISCARDED: card=%d mark=%d Source=%d expected=1",
                cardId, (int)markedHand, actualSource);
            if (!markedHand || actualSource != static_cast<int>(DiscardedSystem::Source::Hand)) {
                LOG_ERR("CustomComponents/DiscardFromHand/DISCARDED: HAND SOURCE MARK FAILED card=%d; suppress trigger", cardId);
                // Zone transfer already succeeded. Do not attempt to reverse it.
                DiscardFromHandPresentationSystem::Queue(before.handId, cardId, reinterpret_cast<std::uintptr_t>(model));
                return;
            }

            // Presentation is deliberately NOT synthesized: no generic stock
            // Hand->Discard record was verified in dump.cs. The frontend can
            // remain visually stale until a dedicated record/animation exists.
            LOG_INFO("CustomComponents/DiscardFromHand: MODEL SUCCESS card=%d owner=%d hand=%d pile=%d; FRONTEND RECORD NOT YET IMPLEMENTED",
                cardId, before.ownerId, before.handId, before.pileId);
            DiscardFromHandPresentationSystem::Queue(before.handId, cardId, reinterpret_cast<std::uintptr_t>(model));
            CustomTriggerSystem::FireDiscardOutsidePlay(effect);
            return;
        }

        // All ordinary stock play/death discards retain the existing behavior.
        const bool wasInHand =
            model && cardId >= 0 && CardIsInAnyHand(model, cardId);
        originalMoveToDiscardProcessEffect(self, effect, method);
        if (!wasInHand || !effect || !model || cardId < 0)
            return;
        DiscardContext ctx;
        if (!ResolveDiscardContext(model, cardId, ctx))
            return;
        const bool markedHand = DiscardedSystem::Mark(
            model, cardId, DiscardedSystem::Source::Hand);
        const int actualSource = DiscardedSystem::GetSource(model, cardId);
        LOG_INFO("CustomComponents/DiscardFromHand/DISCARDED: STOCK card=%d mark=%d Source=%d expected=1",
            cardId, (int)markedHand, actualSource);
        if (!markedHand || actualSource != static_cast<int>(DiscardedSystem::Source::Hand)) {
            LOG_ERR("CustomComponents/DiscardFromHand/DISCARDED: STOCK HAND SOURCE MARK FAILED card=%d; suppress trigger", cardId);
            return;
        }
        LOG_INFO("CustomComponents/DiscardOutsidePlayTrigger: HAND EntityId=%d PlayerId=%d DiscardPileId=%d",
            cardId, ctx.playerId, ctx.discardPileId);
        CustomTriggerSystem::FireDiscardOutsidePlay(effect);
    }

    // =====================================================================
    // Registration / hooks
    // =====================================================================

    inline bool RegisterTypes() {
        struct Registration {
            const char* name;
            const char* baseName;
        };

        const Registration registrations[] = {
            {
                "ReturnFromDiscardPileEffectDescriptor",
                "ReturnCardToPlayersDeckEffectDescriptor"
            },
            {
                "RemoveFromDiscardPileEffectDescriptor",
                "ReturnCardToPlayersDeckEffectDescriptor"
            },
            {
                "ReturnFromDiscardPileToDeckEffectDescriptor",
                "CreateCardInDeckEffectDescriptor"
            },
            {
                "ReturnFromDiscardPileToLaneEffectDescriptor",
                "MoveCardToLanesEffectDescriptor"
            }
        };

        for (const auto& registration : registrations) {
            if (CustomComponentRegistry::FindByName(
                    "PvZCards.Engine.Components",
                    registration.name)) {
                continue;
            }

            const std::size_t registryCountBefore = CustomComponentRegistry::Count();
            BNM::Class pendingClass(
                "PvZCards.Engine.Components",
                registration.name
            );
            const bool pendingClassValid = pendingClass.IsValid();
            auto* pendingMonoType =
                pendingClassValid ? pendingClass.GetMonoType() : nullptr;

            if (!CustomComponentRegistry::Register({
                    registration.name,
                    "PvZCards.Engine.Components",
                    "EngineLib.dll",
                    registration.baseName,
                    "PvZCards.Engine.Components",
                    "EngineLib.dll"
                })) {
                LOG_ERR(
                    "CustomComponents/DiscardPileZoneEffects: failed to register %s "
                    "(registryCount=%zu max=%zu classValid=%d monoType=%p duplicate=%d base=%s)",
                    registration.name,
                    registryCountBefore,
                    CustomComponentRegistry::MaxComponents,
                    pendingClassValid ? 1 : 0,
                    pendingMonoType,
                    CustomComponentRegistry::FindByName(
                        "PvZCards.Engine.Components",
                        registration.name
                    ) ? 1 : 0,
                    registration.baseName
                );
                return false;
            }

            LOG_INFO(
                "CustomComponents/DiscardPileZoneEffects: registered %s "
                "(registryCount=%zu->%zu classValid=%d monoType=%p base=%s)",
                registration.name,
                registryCountBefore,
                CustomComponentRegistry::Count(),
                pendingClassValid ? 1 : 0,
                pendingMonoType,
                registration.baseName
            );
        }

        return true;
    }

    inline bool InstallHooks() {
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto playerUtils = BNM::Class("PvZCards.Core.Utilities", "PlayerUtils");
        auto moveToDiscardSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "MoveToDiscardPileSystem"
        );

        auto discardAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "DiscardPileAssociation"
        );
        auto deckAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "DeckAssociation"
        );
        auto handAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "HandAssociation"
        );
        auto discardPile = BNM::Class(
            "PvZCards.Engine.Components",
            "DiscardPile"
        );

        discardModifierClass = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "DiscardPileModifier"
        );
        handModifierClass = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "HandModifier"
        );
        auto deckModifier = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "DeckModifier"
        );
        auto laneModifier = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "LaneModifier"
        );

        auto returnDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "ReturnCardToPlayersDeckEffectDescriptor"
        );
        auto returnRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "ReturnCardToPlayersDeckEffect"
        );
        auto returnSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "ReturnCardToPlayersDeckSystem"
        );

        auto createDeckDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "CreateCardInDeckEffectDescriptor"
        );
        auto createDeckRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "CreateCardInDeckEffect"
        );
        auto createDeckSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "CreateCardInDeckSystem"
        );

        auto moveLaneDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "MoveCardToLanesEffectDescriptor"
        );
        auto moveLaneRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "MoveCardToLanesEffect"
        );
        auto moveLaneSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "MoveCardToLanesSystem"
        );

        auto laneComponentClass = BNM::Class(
            "PvZCards.Engine.Components", "Lane"
        );
        auto laneElementClass = BNM::Class(
            "PvZCards.Engine.Components", "LaneElement"
        );

        auto hand = BNM::Class(
            "PvZCards.Engine.Components",
            "Hand"
        );

        auto history = BNM::Class("PvZCards.Engine", "History");
        auto systemType = BNM::Class("System", "Type");

        auto returnedDeckRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "CardReturnedToPlayersDeckRecord"
        );
        auto cardIdData = BNM::Class(
            "PvZCards.Engine.Records",
            "CardIdData"
        );
        auto drawCardRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "DrawCardRecord"
        );
        auto createLaneRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "CreateCardInLaneRecord"
        );
        auto effectRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "EffectRecord"
        );

        if (!effect.IsValid() || !entity.IsValid() || !model.IsValid() ||
            !playerUtils.IsValid() || !moveToDiscardSystem.IsValid() ||
            !discardAssociation.IsValid() ||
            !deckAssociation.IsValid() || !handAssociation.IsValid() ||
            !discardPile.IsValid() || !hand.IsValid() ||
            !discardModifierClass.IsValid() || !handModifierClass.IsValid() ||
            !deckModifier.IsValid() ||
            !laneModifier.IsValid() || !laneComponentClass.IsValid() ||
            !laneElementClass.IsValid() || !returnDescriptor.IsValid() ||
            !returnRuntime.IsValid() || !returnSystem.IsValid() ||
            !createDeckDescriptor.IsValid() || !createDeckRuntime.IsValid() ||
            !createDeckSystem.IsValid() || !moveLaneDescriptor.IsValid() ||
            !moveLaneRuntime.IsValid() || !moveLaneSystem.IsValid() ||
            !history.IsValid() || !systemType.IsValid() ||
            !returnedDeckRecord.IsValid() || !cardIdData.IsValid() ||
            !drawCardRecord.IsValid() || !createLaneRecord.IsValid() ||
            !effectRecord.IsValid()) {
            LOG_ERR("CustomComponents/DiscardPileZoneEffects: required classes missing");
            return false;
        }

        effectTargetId = effect.GetField("TargetId");
        effectSecondaryTargetId = effect.GetField("SecondaryTargetId");
        effectEffectEntityId = effect.GetField("EffectEntityId");

        moveToDiscardSystemModel =
            moveToDiscardSystem.GetField("<Model>k__BackingField");
        moveToDiscardEffectPileId = BNM::Class("PvZCards.Engine.Effects", "MoveToDiscardPileEffect").GetField("DiscardPileId");

        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");

        discardAssociationId = discardAssociation.GetField("DiscardPileId");
        deckAssociationId = deckAssociation.GetField("DeckEntityId");
        handAssociationId = handAssociation.GetField("HandEntityId");
        discardCardIds = discardPile.GetField("CardIds");
        handCardIds = hand.GetField("CardIds");

        getPlayerIdFromCard = playerUtils.GetMethod("GetPlayerIdFromCard", 1).cast<int>();
        discardRemoveCard = discardModifierClass.GetMethod("RemoveCard", 2).cast<void>();
        handAddCard = handModifierClass.GetMethod("AddCard", 2).cast<void>();
        handRemoveContainingCard = handModifierClass.GetMethod("RemoveCardFromContainingHand", 1).cast<void>();
        discardAddCard = discardModifierClass.GetMethod("AddCard", 2).cast<void>();
        handModifierEntities = handModifierClass.GetField("<Entities>k__BackingField");
        discardModifierEntities = discardModifierClass.GetField("<Entities>k__BackingField");

        returnDeckPosition = returnRuntime.GetField("DeckPosition");
        returnSystemModel = returnSystem.GetField("<Model>k__BackingField");
        returnSystemDiscardModifier = returnSystem.GetField("<DiscardPileModifier>k__BackingField");
        returnSystemHandModifier = returnSystem.GetField("<HandModifier>k__BackingField");
        returnSystemHistory = returnSystem.GetField("<History>k__BackingField");

        createDeckDescriptorPosition = createDeckDescriptor.GetField("DeckPosition");
        createDeckRuntimeCardGuid = createDeckRuntime.GetField("CardGuid");
        createDeckRuntimePosition = createDeckRuntime.GetField("DeckPosition");
        createDeckSystemModel = createDeckSystem.GetField("<Model>k__BackingField");
        createDeckSystemModifier = createDeckSystem.GetField("<DeckModifier>k__BackingField");
        createDeckSystemHistory = createDeckSystem.GetField("<History>k__BackingField");
        deckAddCardAt = deckModifier.GetMethod("AddCardAt", 3).cast<void>();

        moveLaneRuntimeOrder = moveLaneRuntime.GetField("OrderInLane");
        moveLaneSystemModel = moveLaneSystem.GetField("<Entities>k__BackingField");
        moveLaneSystemModifier = moveLaneSystem.GetField("<LaneModifier>k__BackingField");
        moveLaneSystemHistory = moveLaneSystem.GetField("<History>k__BackingField");
        laneModifierModel = laneModifier.GetField("<Entities>k__BackingField");
        laneElementIds = laneElementClass.GetField("LaneIds");
        laneGetNumFightersForPlayer = laneComponentClass.GetMethod("GetNumFightersForPlayer", 1).cast<int>();
        laneGetNumTotalFighters = laneComponentClass.GetMethod("GetNumTotalFighters", 0).cast<int>();
        lanePlayCard = laneModifier.GetMethod(
            "PlayCardInLane",
            {BNM::Class("System", "Int32").GetCompileTimeClass(),
             BNM::Class("System", "Int32").GetCompileTimeClass(),
             BNM::Class("System", "Int32").GetCompileTimeClass()}
        ).cast<void>();

        historyAddRecordByType = history.GetMethod(
            "AddRecord",
            {systemType.GetCompileTimeClass()}
        ).cast<Obj*>();

        returnedDeckRecordSetCardId = returnedDeckRecord.GetMethod("set_CardId", 1).cast<void>();
        returnedDeckRecordSetDeckId = returnedDeckRecord.GetMethod("set_DeckId", 1).cast<void>();

        cardIdDataCreateFromEntity = cardIdData.GetMethod("CreateFromEntity", 1).cast<Obj*>();
        drawRecordSetHandId = drawCardRecord.GetMethod("set_HandId", 1).cast<void>();
        drawRecordSetDeckId = drawCardRecord.GetMethod("set_DeckId", 1).cast<void>();
        drawRecordSetCard = drawCardRecord.GetMethod("set_Card", 1).cast<void>();
        createLaneRecordSetCard = createLaneRecord.GetMethod("set_Card", 1).cast<void>();
        createLaneRecordSetOrder = createLaneRecord.GetMethod("set_OrderInLane", 1).cast<void>();
        createLaneRecordSetFaceDown = createLaneRecord.GetMethod("set_IsFaceDown", 1).cast<void>();
        effectRecordSetDefaultIds = effectRecord.GetMethod("SetDefaultIdsFromEffect", 1).cast<void>();
        effectRecordSetTargetId = effectRecord.GetMethod("set_TargetId", 1).cast<void>();

        auto returnToEffect = returnDescriptor.GetMethod("ToEffect", 2);
        auto createDeckToEffect = createDeckDescriptor.GetMethod("ToEffect", 2);
        auto moveLaneToEffect = moveLaneDescriptor.GetMethod("ToEffect", 2);

        auto returnProcess = returnSystem.GetMethod("ProcessEffect", 1);
        auto createDeckProcess = createDeckSystem.GetMethod("ProcessEffect", 1);
        auto moveLaneProcess = moveLaneSystem.GetMethod("ProcessEffect", 1);
        auto moveToDiscardProcess =
            moveToDiscardSystem.GetMethod("ProcessEffect", 1);

        LOG_INFO(
            "CustomComponents/DiscardPileZoneEffects: DiscardOutsidePlay HAND API "
            "moveToDiscardClass=%d model=%d process=%d",
            moveToDiscardSystem.IsValid() ? 1 : 0,
            moveToDiscardSystemModel.IsValid() ? 1 : 0,
            moveToDiscardProcess.IsValid() ? 1 : 0
        );

        LOG_INFO(
            "CustomComponents/DiscardPileZoneEffects: stock return hooks owned by ReturnFromDiscardPileSystem; not re-hooking"
        );

        if (!effectTargetId.IsValid() || !effectSecondaryTargetId.IsValid() ||
            !effectEffectEntityId.IsValid() ||
            !moveToDiscardSystemModel.IsValid() ||
            !modelEntities.IsValid() || !entityComponents.IsValid() ||
            !entityId.IsValid() || !discardAssociationId.IsValid() ||
            !deckAssociationId.IsValid() || !handAssociationId.IsValid() ||
            !discardCardIds.IsValid() || !handCardIds.IsValid() ||
            !getPlayerIdFromCard.IsValid() || !discardRemoveCard.IsValid() ||
            !handAddCard.IsValid() || !discardModifierEntities.IsValid() ||
            !handModifierEntities.IsValid() || !handRemoveContainingCard.IsValid() ||
            !discardAddCard.IsValid() ||
            !returnDeckPosition.IsValid() || !returnSystemModel.IsValid() ||
            !returnSystemDiscardModifier.IsValid() ||
            !returnSystemHandModifier.IsValid() || !returnSystemHistory.IsValid() ||
            !createDeckDescriptorPosition.IsValid() ||
            !createDeckRuntimeCardGuid.IsValid() || !createDeckRuntimePosition.IsValid() ||
            !createDeckSystemModel.IsValid() || !createDeckSystemModifier.IsValid() ||
            !createDeckSystemHistory.IsValid() || !deckAddCardAt.IsValid() ||
            !moveLaneRuntimeOrder.IsValid() || !moveLaneSystemModel.IsValid() ||
            !moveLaneSystemModifier.IsValid() || !moveLaneSystemHistory.IsValid() ||
            !lanePlayCard.IsValid() || !laneModifierModel.IsValid() ||
            !laneElementIds.IsValid() || !laneGetNumFightersForPlayer.IsValid() ||
            !laneGetNumTotalFighters.IsValid() || !historyAddRecordByType.IsValid() ||
            !returnedDeckRecordSetCardId.IsValid() ||
            !returnedDeckRecordSetDeckId.IsValid() ||
            !cardIdDataCreateFromEntity.IsValid() ||
            !drawRecordSetHandId.IsValid() || !drawRecordSetDeckId.IsValid() ||
            !drawRecordSetCard.IsValid() || !createLaneRecordSetCard.IsValid() ||
            !createLaneRecordSetOrder.IsValid() || !createLaneRecordSetFaceDown.IsValid() ||
            !effectRecordSetDefaultIds.IsValid() || !effectRecordSetTargetId.IsValid() ||
            !returnToEffect.IsValid() ||
            !createDeckToEffect.IsValid() ||
            !moveLaneToEffect.IsValid() || !returnProcess.IsValid() ||
            !createDeckProcess.IsValid() || !moveLaneProcess.IsValid() ||
            !moveToDiscardProcess.IsValid()) {
            LOG_ERR("CustomComponents/DiscardPileZoneEffects: required API missing");
            return false;
        }

        BNM::BasicHook(
            createDeckToEffect,
            CreateDeckDescriptorToEffectHook,
            originalCreateDeckDescriptorToEffect
        );
        BNM::BasicHook(
            moveLaneToEffect,
            MoveLaneDescriptorToEffectHook,
            originalMoveLaneDescriptorToEffect
        );

        BNM::BasicHook(
            createDeckProcess,
            CreateDeckProcessEffectHook,
            originalCreateDeckProcessEffect
        );
        BNM::BasicHook(
            moveLaneProcess,
            MoveLaneProcessEffectHook,
            originalMoveLaneProcessEffect
        );

        BNM::BasicHook(
            moveToDiscardProcess,
            MoveToDiscardProcessEffectHook,
            originalMoveToDiscardProcessEffect
        );

        bool hookInstallationFailed = false;

#define CHECK_DISCARD_ZONE_HOOK(name, value) \
        do { \
            if (!(value)) { \
                LOG_ERR( \
                    "CustomComponents/DiscardPileZoneEffects: HOOK FAILED: %s", \
                    name \
                ); \
                hookInstallationFailed = true; \
            } \
        } while (0)

        CHECK_DISCARD_ZONE_HOOK(
            "CreateCardInDeckEffectDescriptor.ToEffect",
            originalCreateDeckDescriptorToEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "MoveCardToLanesEffectDescriptor.ToEffect",
            originalMoveLaneDescriptorToEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "CreateCardInDeckSystem.ProcessEffect",
            originalCreateDeckProcessEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "MoveCardToLanesSystem.ProcessEffect",
            originalMoveLaneProcessEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "MoveToDiscardPileSystem.ProcessEffect",
            originalMoveToDiscardProcessEffect
        );

#undef CHECK_DISCARD_ZONE_HOOK

        if (hookInstallationFailed) {
            LOG_ERR(
                "CustomComponents/DiscardPileZoneEffects: hook installation failed; see HOOK FAILED line(s) above"
            );
            return false;
        }

        LOG_INFO(
            "CustomComponents/DiscardPileZoneEffects: ready (deck/lane returns + DiscardOutsidePlayTrigger hand path; hand/remove owned by ReturnFromDiscardPileSystem; deck-discard owned by DiscardFromDeckSystem)"
        );
        return true;
    }

    inline bool Initialize() {
        return RegisterTypes() &&
               DiscardFromHandSystem::Initialize() &&
               DiscardFromHandPresentationSystem::Initialize() &&
               InstallHooks() && moveToDiscardEffectPileId.IsValid();
    }

} // namespace CustomComponents::Systems::DiscardPileZoneEffectsSystem
