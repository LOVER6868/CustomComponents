#include "diagnostics/DiagnosticLogging.hpp"
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

        // Read-only correlation for the deck-return history and triggered
        // presentation path. Report the input effect identity BEFORE mutation.
        const int traceEffectEntityId = effectEffectEntityId.IsValid()
            ? effectEffectEntityId[effect].Get() : -1;
        const int traceSecondaryId = effectSecondaryTargetId.IsValid()
            ? effectSecondaryTargetId[effect].Get() : -1;
        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: TRANSFER-BEFORE card=%d player=%d discard=%d deck=%d position=%d effect=%p effectEntity=%d secondary=%d model=%p history=%p cardObject=%p",
            cardId, ctx.playerId, ctx.discardPileId, deckId, deckPosition,
            effect, traceEffectEntityId, traceSecondaryId, model, history, ctx.card);

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
        // This records the actual stock-history object identity and the
        // resulting off-board zone, without querying/mutating frontend views.
        const bool inDeckAfter = ZoneTriggeredAbilityPresentationSystem::IsInMaterializedDeckPool(model, cardId);
        const auto zoneAfter = ZoneTriggeredAbilityPresentationSystem::GetOffBoardZone(model, cardId);
        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: RECORD-AFTER card=%d record=%p recordCard=%d deck=%d position=%d effectEntity=%d history=%p inDeck=%d zone=%s",
            cardId, record, record ? cardId : -1, deckId, deckPosition,
            traceEffectEntityId, history, int(inDeckAfter),
            ZoneTriggeredAbilityPresentationSystem::ZoneName(zoneAfter));


        CC_DIAG(
            "CustomComponents/ReturnFromDiscardPileToDeck: SUCCESS EntityId=%d PlayerId=%d DiscardPileId=%d DeckId=%d DeckPosition=%d (same live entity; no reset)",
            cardId,
            ctx.playerId,
            ctx.discardPileId,
            deckId,
            deckPosition
        );

        // Coordinate rapid discard -> deck return with pending CardView retirement.
        Core::CardViewLifecycleCoordinator::NoteReturn(cardId, deckId, ctx.discardPileId);
        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: RETURN-TRIGGER-ENTER card=%d effectEntity=%d record=%p", cardId, traceEffectEntityId, record);
        CustomTriggerSystem::FireReturnedFromDiscardPile(effect);
        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: RETURN-TRIGGER-EXIT card=%d effectEntity=%d", cardId, traceEffectEntityId);
    }

