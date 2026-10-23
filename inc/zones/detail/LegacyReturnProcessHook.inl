#include "diagnostics/DiagnosticLogging.hpp"
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
        // Compare against Pick3/TARGET_TRACE for Unearth/return-from-discard
        // issues. The native effect must receive the chosen physical Entity.Id;
        // a Card.Guid cannot be looked up as a discarded live entity.
        LOG_INFO("CustomComponents/DiscardPile/TARGET_TRACE: route=%d effectTargetEntityId=%d "
                 "effectGroupEntityId=%d",
                 route, cardId, effectEffectEntityId.IsValid()
                    ? effectEffectEntityId[effect].Get() : -1);

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

            CC_DIAG(
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

                CC_DIAG(
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

        CC_DIAG(
            "CustomComponents/ReturnFromDiscardPile: SUCCESS EntityId=%d PlayerId=%d DiscardPileId=%d HandId=%d discard=%d->%d hand=%d->%d contains=%d->%d (same live entity)",
            cardId, ctx.playerId, ctx.discardPileId, handId,
            beforeDiscardCount, afterDiscardCount,
            beforeHandCount, afterHandCount,
            handContainedBefore ? 1 : 0,
            handContainsAfter ? 1 : 0
        );

        CustomTriggerSystem::FireReturnedFromDiscardPile(effect);
    }

