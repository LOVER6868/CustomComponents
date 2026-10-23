#include "diagnostics/DiagnosticLogging.hpp"
    // =====================================================================
    // 3. ReturnFromDiscardPileToLane
    // =====================================================================

    static void MoveLaneProcessEffectHook(void* self, Obj* effect, const void* method) {
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS ENTER self=%p effect=%p method=%p", self, effect, method);
        if (!effect || !self) {
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS null argument; stock delegation ENTER");
            originalMoveLaneProcessEffect(self, effect, method);
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS null argument; stock delegation EXIT");
            return;
        }

        auto* system = static_cast<Obj*>(self);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS before model field read system=%p", system);
        Obj* model = moveLaneSystemModel[system].Get();
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS model=%p", model);

        // Do NOT use MoveCardToLanesEffect.OrderInLane as the routing tag here.
        // The secondary-target pipeline can rebuild/deep-copy the runtime effect,
        // so a borrowed stock field is not a stable identity marker. Instead,
        // identify the effect from its originating EffectEntity and the custom
        // descriptor component that produced it.
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS before EffectEntityId read effect=%p", effect);
        const int sourceEffectEntityId = effectEffectEntityId[effect].Get();
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS EffectEntityId=%d before FindEntity model=%p", sourceEffectEntityId, model);
        if (!model || sourceEffectEntityId < 0) {
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS invalid model/id; stock delegation ENTER");
            originalMoveLaneProcessEffect(self, effect, method);
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS invalid model/id; stock delegation EXIT");
            return;
        }
        Obj* sourceEffectEntity = FindEntity(model, sourceEffectEntityId);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS FindEntity EXIT entity=%p before FindComponent", sourceEffectEntity);
        Obj* customLaneDescriptor = FindComponent(
            sourceEffectEntity,
            "ReturnFromDiscardPileToLaneEffectDescriptor"
        );
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS FindComponent EXIT descriptor=%p", customLaneDescriptor);

        if (!customLaneDescriptor) {
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS stock delegate ENTER");
            originalMoveLaneProcessEffect(self, effect, method);
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: PROCESS stock delegate EXIT");
            return;
        }

        // Read-only preflight: NO zone mutations, record creation,
        // scratch modifier allocation, or custom trigger dispatch in this block.
        // All checkpoints are deliberately separated to locate a native fault.
        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: PREFLIGHT ENTER effect=%p model=%p system=%p", effect, model, system);
        const int preflightCardId = effectTargetId[effect].Get();
        const int preflightLaneId = effectSecondaryTargetId[effect].Get();
        LOG_INFO("CustomComponents/DiscardPile/LANE_TARGET_TRACE: "
                 "effect=%d cardTargetEntityId=%d laneTargetEntityId=%d",
                 sourceEffectEntityId, preflightCardId, preflightLaneId);
        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: TARGETS cardId=%d laneId=%d effectEntityId=%d", preflightCardId, preflightLaneId, sourceEffectEntityId);

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: SYSTEM before modifier/history reads");
        Obj* preflightLaneModifier = moveLaneSystemModifier[system].Get();
        Obj* preflightHistory = moveLaneSystemHistory[system].Get();
        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: SYSTEM laneModifier=%p history=%p playCardValid=%d discardRemoveValid=%d historyAddValid=%d scratchClassValid=%d scratchFieldValid=%d",
                 preflightLaneModifier, preflightHistory, lanePlayCard.IsValid() ? 1 : 0,
                 discardRemoveCard.IsValid() ? 1 : 0, historyAddRecordByType.IsValid() ? 1 : 0,
                 discardModifierClass.IsValid() ? 1 : 0, discardModifierEntities.IsValid() ? 1 : 0);

        DiscardContext preflightCtx;
        bool preflightDiscardResolved = false;
        if (preflightCardId >= 0) {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: DISCARD before ResolveDiscardContext cardId=%d", preflightCardId);
            preflightDiscardResolved = ResolveDiscardContext(model, preflightCardId, preflightCtx);
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: DISCARD after ResolveDiscardContext ok=%d card=%p player=%p pile=%p playerId=%d pileId=%d",
                     preflightDiscardResolved ? 1 : 0, preflightCtx.card, preflightCtx.player,
                     preflightCtx.discardPile, preflightCtx.playerId, preflightCtx.discardPileId);
        } else {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: DISCARD skipped invalid target id=%d", preflightCardId);
        }

        Obj* preflightLaneEntity = nullptr;
        Obj* preflightLaneComponent = nullptr;
        if (preflightLaneId >= 0) {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: LANE before FindEntity laneId=%d", preflightLaneId);
            preflightLaneEntity = FindEntity(model, preflightLaneId);
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: LANE after FindEntity entity=%p", preflightLaneEntity);
            if (preflightLaneEntity) {
                if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: LANE before FindComponent");
                preflightLaneComponent = FindComponent(preflightLaneEntity, "Lane");
                if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: LANE after FindComponent component=%p", preflightLaneComponent);
            }
        } else {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I2: LANE skipped invalid lane id=%d", preflightLaneId);
        }
        // Inspect stock placement dependencies without invoking any
        // mutating LaneModifier methods or touching Lane's dictionary storage.
        Obj* i3ModifierModel = nullptr;
        if (preflightLaneModifier && laneModifierModel.IsValid()) {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: MODIFIER before Entities read modifier=%p", preflightLaneModifier);
            i3ModifierModel = laneModifierModel[preflightLaneModifier].Get();
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: MODIFIER Entities=%p System.Entities=%p match=%d",
                     i3ModifierModel, model, i3ModifierModel == model ? 1 : 0);
        } else {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: MODIFIER unavailable modifier=%p fieldValid=%d",
                     preflightLaneModifier, laneModifierModel.IsValid() ? 1 : 0);
        }

        Obj* i3LaneElement = preflightDiscardResolved
            ? FindComponent(preflightCtx.card, "LaneElement") : nullptr;
        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: CARD LaneElement=%p fieldValid=%d",
                 i3LaneElement, laneElementIds.IsValid() ? 1 : 0);
        int i3LaneIdCount = -1;
        bool i3CardAlreadyInTarget = false;
        if (i3LaneElement && laneElementIds.IsValid()) {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: CARD before LaneIds field read");
            auto* ids = laneElementIds[i3LaneElement].Get();
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: CARD LaneIds list=%p", ids);
            if (ids && ids->items && ids->size >= 0 && ids->size <= 32) {
                i3LaneIdCount = ids->size;
                for (int i = 0; i < i3LaneIdCount; ++i) {
                    const int oldLaneId = ids->items->m_Items[i];
                    if (oldLaneId == preflightLaneId) i3CardAlreadyInTarget = true;
                    if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: CARD LaneIds[%d]=%d", i, oldLaneId);
                }
            } else {
                if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: CARD LaneIds null or unexpected count (no iteration)");
            }
        }
        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: CARD RESULT laneIdCount=%d alreadyInTarget=%d",
                 i3LaneIdCount, i3CardAlreadyInTarget ? 1 : 0);

        int i3OwnerFighters = -1;
        int i3TotalFighters = -1;
        if (preflightLaneComponent && preflightDiscardResolved &&
            laneGetNumFightersForPlayer.IsValid() && laneGetNumTotalFighters.IsValid()) {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: OCCUPANCY before GetNumFightersForPlayer playerId=%d",
                     preflightCtx.playerId);
            i3OwnerFighters = laneGetNumFightersForPlayer[preflightLaneComponent](preflightCtx.playerId);
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: OCCUPANCY ownerFighters=%d before GetNumTotalFighters", i3OwnerFighters);
            i3TotalFighters = laneGetNumTotalFighters[preflightLaneComponent]();
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: OCCUPANCY totalFighters=%d", i3TotalFighters);
        } else {
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: OCCUPANCY unavailable lane=%p playerResolved=%d ownerMethod=%d totalMethod=%d",
                     preflightLaneComponent, preflightDiscardResolved ? 1 : 0,
                     laneGetNumFightersForPlayer.IsValid() ? 1 : 0,
                     laneGetNumTotalFighters.IsValid() ? 1 : 0);
        }
        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I3: SUMMARY modelMatch=%d laneElement=%p cardLaneCount=%d alreadyTarget=%d ownerFighters=%d totalFighters=%d (observational only)",
                 i3ModifierModel && i3ModifierModel == model ? 1 : 0, i3LaneElement,
                 i3LaneIdCount, i3CardAlreadyInTarget ? 1 : 0, i3OwnerFighters, i3TotalFighters);

        // Read-only zone-component and frontend provenance inspection.
        // No mutation, no provenance registration and no record creation.
        if constexpr (CustomComponents::Diagnostics::kVerbose) {
            if (preflightDiscardResolved) {
                const char* const zoneNames[] = {
                "Card", "Owner", "DiscardPileElement", "HandElement",
                "DeckElement", "LaneElement", "Attack", "Health",
                "SunCost", "Plant", "Zombie", "Fighter"
            };
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I4: CARD-COMPONENTS BEGIN card=%d entity=%p",
                     preflightCardId, preflightCtx.card);
            for (const char* name : zoneNames) {
                Obj* found = FindComponent(preflightCtx.card, name);
                if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I4: CARD-COMPONENT name=%s present=%d ptr=%p",
                         name, found ? 1 : 0, found);
            }
            Obj* discardElement = FindComponent(preflightCtx.card, "DiscardPileElement");
            Obj* handElement = FindComponent(preflightCtx.card, "HandElement");
            Obj* deckElement = FindComponent(preflightCtx.card, "DeckElement");
            const bool actualDiscardMembership = DiscardContains(preflightCtx.discardPile, preflightCardId);
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I4: ZONE-SUMMARY card=%d pileId=%d pileContains=%d discardElement=%d handElement=%d deckElement=%d laneElement=%d",
                     preflightCardId, preflightCtx.discardPileId, actualDiscardMembership ? 1 : 0,
                     discardElement ? 1 : 0, handElement ? 1 : 0,
                     deckElement ? 1 : 0, i3LaneElement ? 1 : 0);
            const bool exactOrigin =
                ZoneTriggeredAbilityPresentationSystem::IsDiscardToLaneOrigin(
                    model, preflightCardId, sourceEffectEntityId);
            const bool anyOrigin =
                ZoneTriggeredAbilityPresentationSystem::IsDiscardToLaneOrigin(
                    model, preflightCardId);
            if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/LANE-I4: PRESENTATION-ORIGIN card=%d effectEntityId=%d exactRegistered=%d anyRegistered=%d (peek only, no registration)",
                     preflightCardId, sourceEffectEntityId,
                     exactOrigin ? 1 : 0, anyOrigin ? 1 : 0);
            } else {
                CC_DIAG("CustomComponents/LANE-I4: SKIP no valid discard context card=%d", preflightCardId);
            }

            // Historical frontend map/history inspection is observational only.
            if (preflightDiscardResolved)
                Core::CardViewLifecycleCoordinator::InspectLaneReturnMapping(model, preflightCardId);
        }

        const bool preflightReady = preflightDiscardResolved && preflightLaneComponent &&
            preflightLaneModifier && preflightHistory && lanePlayCard.IsValid() &&
            discardRemoveCard.IsValid() && historyAddRecordByType.IsValid() &&
            discardModifierClass.IsValid() && discardModifierEntities.IsValid();
        CC_DIAG("CustomComponents/ReturnFromDiscardPileToLane: preflight card=%d lane=%d player=%d ready=%d",
                 preflightCardId, preflightLaneId, preflightCtx.playerId, preflightReady ? 1 : 0);

        // Allow repeated discard-to-lane cycles for the same
        // entity while retaining all existing model/frontend safety gates.
        const bool inPileBefore = preflightDiscardResolved &&
            DiscardContains(preflightCtx.discardPile, preflightCardId);
        const bool discardBefore = preflightDiscardResolved &&
            FindComponent(preflightCtx.card, "DiscardPileElement") != nullptr;
        const bool laneBefore = preflightDiscardResolved &&
            FindComponent(preflightCtx.card, "LaneElement") != nullptr;
        const bool handBefore = preflightDiscardResolved &&
            FindComponent(preflightCtx.card, "HandElement") != nullptr;
        const bool deckBefore = preflightDiscardResolved &&
            FindComponent(preflightCtx.card, "DeckElement") != nullptr;
        const bool sameModel = i3ModifierModel && i3ModifierModel == model;
        const bool cleanSource = inPileBefore && discardBefore && !laneBefore &&
            !handBefore && !deckBefore;
        const bool candidate = preflightReady && sameModel && cleanSource &&
            preflightLaneComponent && i3OwnerFighters >= 0 && i3TotalFighters >= 0;

        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: PREFLIGHT card=%d lane=%d pile=%d inPile=%d discard=%d lane=%d hand=%d deck=%d modelMatch=%d ownerFighters=%d totalFighters=%d ready=%d candidate=%d",
                 preflightCardId, preflightLaneId, preflightCtx.discardPileId,
                 inPileBefore ? 1 : 0, discardBefore ? 1 : 0, laneBefore ? 1 : 0,
                 handBefore ? 1 : 0, deckBefore ? 1 : 0, sameModel ? 1 : 0,
                 i3OwnerFighters, i3TotalFighters, preflightReady ? 1 : 0,
                 candidate ? 1 : 0);

        // This guard tracks only transfers currently executing in this hook.
        // A completed transfer is no longer blocked forever: on a later cycle,
        // the ordinary discard-zone and cleared-view checks must pass again.
        // Use the model address for identity only; never dereference it later.
        static uintptr_t sI10DModelIdentity = 0;
        static std::vector<int> sI10DActiveCards;
        const uintptr_t currentModelIdentity = reinterpret_cast<uintptr_t>(model);
        if (currentModelIdentity != sI10DModelIdentity) {
            sI10DModelIdentity = currentModelIdentity;
            sI10DActiveCards.clear();
            if constexpr (false) if constexpr (false) CC_DIAG("CustomComponents/LANE-I10D: MODEL identity=%p active-transfer guard reset", model);
        }

        if (!kBypassCustomLaneTransferPendingVerification) {
            LOG_ERR("CustomComponents/LANE-I9: unexpected bypass-disabled configuration; skipping custom transfer");
            return;
        }
        // A stale CardView mapping must never be silently reused as a lane
        // card. Guard before the first zone mutation. Stock mapper owns cleanup.
        Obj* i10Mapper = Core::SceneViewContext::RootedMapper();
        const bool i10MapperMatches = i10Mapper &&
            Core::SceneViewContext::ModelIdentity() == currentModelIdentity;
        Obj* i10PriorView = i10MapperMatches && Core::CardViewLifecycleCoordinator::getView.IsValid()
            ? Core::CardViewLifecycleCoordinator::getView[i10Mapper](preflightCardId) : nullptr;
        const bool i10LookupReady = Core::CardViewLifecycleCoordinator::getView.IsValid();
        if constexpr (false) if constexpr (false) CC_DIAG("CustomComponents/LANE-I10B: MAPPING-PREFLIGHT card=%d mapper=%p modelMatch=%d lookupReady=%d priorView=%p", preflightCardId, i10Mapper, int(i10MapperMatches), int(i10LookupReady), i10PriorView);
        if (!i10MapperMatches || !i10LookupReady) {
            LOG_ERR("CustomComponents/LANE-I10B: BYPASS card=%d mapper unavailable or different model (NO mutation)", preflightCardId);
            return;
        }
        if (i10PriorView) {
            LOG_ERR("CustomComponents/LANE-I10B: BYPASS card=%d existing CardView mapping=%p (NO mutation; never reuse stale view)", preflightCardId, i10PriorView);
            return;
        }
        if constexpr (false) if constexpr (false) CC_DIAG("CustomComponents/LANE-I10D: MAPPING-CLEAR card=%d; stock lane creation eligible", preflightCardId);
        bool alreadyActive = false;
        for (int activeId : sI10DActiveCards) {
            if (activeId == preflightCardId) {
                alreadyActive = true;
                break;
            }
        }
        if (!candidate || alreadyActive) {
            if constexpr (false) if constexpr (false) CC_DIAG("CustomComponents/LANE-I10D: BYPASS card=%d candidate=%d alreadyActive=%d (no mutation)",
                     preflightCardId, candidate ? 1 : 0, alreadyActive ? 1 : 0);
            return;
        }

        // The guard is released on every exit (including failure paths), not
        // just on successful animation submission. Reentrant calls while this
        // transfer is executing cannot remove/place the same card twice.
        sI10DActiveCards.push_back(preflightCardId);
        struct ActiveTransferScope {
            std::vector<int>& active;
            int cardId;
            ~ActiveTransferScope() {
                for (auto it = active.begin(); it != active.end(); ++it) {
                    if (*it == cardId) {
                        active.erase(it);
                        break;
                    }
                }
            }
        } activeTransferScope{sI10DActiveCards, preflightCardId};
        if constexpr (false) if constexpr (false) CC_DIAG("CustomComponents/LANE-I10D: BEGIN card=%d (future discard-return cycles permitted)", preflightCardId);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: ATTEMPT-BEGIN card=%d lane=%d pile=%d player=%d effectEntity=%d",
                 preflightCardId, preflightLaneId, preflightCtx.discardPileId,
                 preflightCtx.playerId, sourceEffectEntityId);

        Obj* discardModifier = ScratchDiscardModifier(model);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: CHECKPOINT-0 scratchDiscardModifier=%p laneModifier=%p",
                 discardModifier, preflightLaneModifier);
        if (!discardModifier || !preflightLaneModifier) {
            LOG_ERR("CustomComponents/LANE-I9: ABORT before mutation: modifier unavailable");
            return;
        }

        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: REMOVE-ENTER card=%d pile=%d",
                 preflightCardId, preflightCtx.discardPileId);
        discardRemoveCard[discardModifier](preflightCardId, preflightCtx.discardPileId);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: REMOVE-RETURN card=%d", preflightCardId);

        Obj* cardAfterRemove = FindEntity(model, preflightCardId);
        const bool pileAfterRemove = DiscardContains(preflightCtx.discardPile, preflightCardId);
        const bool discardAfterRemove = cardAfterRemove &&
            FindComponent(cardAfterRemove, "DiscardPileElement") != nullptr;
        const bool laneAfterRemove = cardAfterRemove &&
            FindComponent(cardAfterRemove, "LaneElement") != nullptr;
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: CHECKPOINT-1 card=%p pileContains=%d discard=%d lane=%d",
                 cardAfterRemove, pileAfterRemove ? 1 : 0,
                 discardAfterRemove ? 1 : 0, laneAfterRemove ? 1 : 0);

        // Do not attempt placement if removal failed or the live entity vanished.
        if (!cardAfterRemove || pileAfterRemove || discardAfterRemove || laneAfterRemove) {
            LOG_ERR("CustomComponents/LANE-I9: STOP after removal: unexpected zone state; no placement/history/trigger");
            return;
        }

        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: PLACE-ENTER card=%d lane=%d player=%d overload=3-int",
                 preflightCardId, preflightLaneId, preflightCtx.playerId);
        // Mark before stock lane placement can enqueue asynchronous view creation.
        Core::CardViewLifecycleCoordinator::NoteReturn(preflightCardId, preflightLaneId, preflightCtx.discardPileId);
        lanePlayCard[preflightLaneModifier](preflightCardId, preflightLaneId, preflightCtx.playerId);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: PLACE-RETURN card=%d", preflightCardId);

        Obj* cardAfterPlace = FindEntity(model, preflightCardId);
        Obj* laneElementAfter = cardAfterPlace
            ? FindComponent(cardAfterPlace, "LaneElement") : nullptr;
        bool targetLanePresent = false;
        int laneCount = -1;
        if (laneElementAfter && laneElementIds.IsValid()) {
            auto* ids = laneElementIds[laneElementAfter].Get();
            if (ids && ids->items && ids->size >= 0 && ids->size <= 32) {
                laneCount = ids->size;
                for (int i = 0; i < laneCount; ++i)
                    if (ids->items->m_Items[i] == preflightLaneId)
                        targetLanePresent = true;
            }
        }
        const bool pileAfterPlace = DiscardContains(preflightCtx.discardPile, preflightCardId);
        const bool discardAfterPlace = cardAfterPlace &&
            FindComponent(cardAfterPlace, "DiscardPileElement") != nullptr;
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I9: CHECKPOINT-2 card=%p pileContains=%d discard=%d laneElement=%p laneCount=%d targetLanePresent=%d",
                 cardAfterPlace, pileAfterPlace ? 1 : 0, discardAfterPlace ? 1 : 0,
                 laneElementAfter, laneCount, targetLanePresent ? 1 : 0);
        const bool transitionOkay = cardAfterPlace && !pileAfterPlace &&
            !discardAfterPlace && targetLanePresent;
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I10: MODEL-TRANSITION success=%d", int(transitionOkay));
        if (!transitionOkay) return;

        // Record pre-trigger stats/traits from the *live* entity, after the
        // zone transfer but before any ReturnedFromDiscardPileTrigger. Source
        // 3 is only consumed by the lane-specific visual reconciliation path.
        ZoneTriggeredAbilityPresentationSystem::RememberDiscardToLaneMove(
            model, preflightCardId, sourceEffectEntityId, preflightLaneId);
        ZoneTriggeredAbilityPresentationSystem::RememberHandDeckReturn(
            model, preflightCardId, 3);

        // Reuse stock creation animation; no Unity objects made by this hook.
        // Entity remains the original entity, not a duplicate or reset copy.
        auto recordClass = BNM::Class("PvZCards.Engine.Records", "CreateCardInLaneRecord");
        Obj* cardData = cardIdDataCreateFromEntity[static_cast<Obj*>(nullptr)](cardAfterPlace);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I10: CREATE-RECORD-PREP card=%d cardData=%p typeValid=%d",
                 preflightCardId, cardData, int(recordClass.IsValid()));
        if (!cardData || !recordClass.IsValid()) {
            LOG_ERR("CustomComponents/LANE-I10: NO RECORD card=%d; match not safe for combat", preflightCardId);
            return;
        }
        Obj* record = historyAddRecordByType[preflightHistory](
            reinterpret_cast<Obj*>(recordClass.GetMonoType()));
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I10: CREATE-RECORD-RETURN card=%d record=%p", preflightCardId, record);
        if (!record) return;
        createLaneRecordSetCard[record](cardData);
        createLaneRecordSetOrder[record](0); // three-int stock overload inserts at front
        createLaneRecordSetFaceDown[record](false);
        effectRecordSetDefaultIds[record](effect);
        effectRecordSetTargetId[record](preflightLaneId);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I11: RECORD-READY card=%d lane=%d order=0 history=%p trigger=PENDING", preflightCardId, preflightLaneId, preflightHistory);
        Core::CardViewLifecycleCoordinator::InspectLaneReturnMapping(model, preflightCardId);

        // Same sequencing as the proven discard-to-hand path: the ORIGINAL
        // entity is already in its destination, the pre-trigger gameplay
        // stat/trait snapshot is saved, and the stock creation record is
        // prepared before dispatching the custom return trigger.  The record
        // targets the LANE for presentation, but the firing effect must retain
        // its CARD TargetId for TriggerData/EntityQueryContext and listener
        // condition evaluation.  Never rewrite effectTargetId to the lane.
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I11: TRIGGER-ENTER card=%d lane=%d effect=%p effectEntity=%d (snapshot already saved)",
                 preflightCardId, preflightLaneId, effect, sourceEffectEntityId);
        const bool returnTriggerDispatched =
            CustomTriggerSystem::FireReturnedFromDiscardPile(effect);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-I11: TRIGGER-EXIT card=%d dispatched=%d (queued effects remain stock-owned)",
                 preflightCardId, int(returnTriggerDispatched));

    }


