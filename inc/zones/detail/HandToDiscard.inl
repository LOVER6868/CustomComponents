#include "diagnostics/DiagnosticLogging.hpp"
    // =====================================================================
    // 4. DiscardOutsidePlayTrigger
    // =====================================================================

    // Snapshot the target owner's hand/discard zones without mutation.
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
        if constexpr (false) CC_DIAG(
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

        // Custom descriptors only; preserve stock effect routing.
        // Stock destination resolution runs before this handler and replaces
        // the descriptor's -1 sentinel. Route by actual physical membership.
        const bool customHandRoute = effect && model && CardIsInAnyHand(model,cardId);

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

            // Remove from hand FIRST. Adding to the discard pile before
            // RemoveCardFromContainingHand can prevent that modifier from
            // locating the card in its containing hand.
            // The original card entity is never recreated or destroyed.
            Obj* originalCard = FindEntity(model, cardId);
            Obj* originalHand = FindEntity(model, before.handId);
            if (!originalCard || !originalHand) {
                LOG_ERR("CustomComponents/DiscardFromHand: REJECT card=%d missing recovery prerequisites", cardId);
                return;
            }

            if constexpr (false) CC_DIAG("CustomComponents/DiscardFromHand/LIFETIME: BEFORE HAND REMOVE model=%p handModifier=%p pileModifier=%p card=%d", model, handModifier, discardModifier, cardId);
            handRemoveContainingCard[handModifier](cardId);
            if constexpr (false) CC_DIAG("CustomComponents/DiscardFromHand/LIFETIME: AFTER HAND REMOVE card=%d", cardId);
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

            if constexpr (false) CC_DIAG("CustomComponents/DiscardFromHand/LIFETIME: BEFORE PILE ADD card=%d", cardId);
            discardAddCard[discardModifier](cardId, before.pileId);
            if constexpr (false) CC_DIAG("CustomComponents/DiscardFromHand/LIFETIME: AFTER PILE ADD card=%d", cardId);
            const auto after = SnapshotHandDiscard(model, cardId);
            LogHandDiscardSnapshot("AFTER-TRANSFER", cardId, after);
            const bool success = after.entityExists &&
                !after.anyHandContains && after.pileContains &&
                after.ownerId == before.ownerId &&
                after.pileId == before.pileId &&
                after.handCount == before.handCount - 1 &&
                after.pileCount == before.pileCount + 1;
            if constexpr (false) CC_DIAG("CustomComponents/DiscardFromHand/DIAG: TRANSFER card=%d success=%d hand=%d->%d pile=%d->%d sameEntity=%d",
                cardId, (int)success, before.handCount, after.handCount,
                before.pileCount, after.pileCount, (int)after.entityExists);
            if (!success) {
                LOG_ERR("CustomComponents/DiscardFromHand: TRANSFER FAILED/INCOMPLETE card=%d; suppress presentation and trigger", cardId);
                LOG_ERR("CustomComponents/DiscardFromHand: non-atomic state; no speculative rollback attempted");
                return;
            }
            if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
                LOG_INFO("CustomComponents/DiscardFromHand/TRANSFER: card=%d hand=%d pile=%d handCount=%d pileCount=%d success=1",cardId,before.handId,before.pileId,after.handCount,after.pileCount);
            if (reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
                Integration::ReplayEventBridge::Publish("discard_hand_to_pile", {
                    {"entityId",cardId},{"ownerId",before.ownerId},
                    {"handId",before.handId},{"pileId",before.pileId},
                    {"stockRoute",0},{"verifiedTransfer",1}});
            Zones::MarkedForDiscard::Mark(FindEntity(model,cardId));

            // Set origin on the SAME live entity while it is verifiably in pile.
            // This must happen before the custom discard trigger is fired.
            const bool markedHand = DiscardedSystem::Mark(
                model, cardId, DiscardedSystem::Source::Hand);
            const int actualSource = DiscardedSystem::GetSource(model, cardId);
            CC_DIAG("CustomComponents/DiscardFromHand/DISCARDED: card=%d mark=%d Source=%d expected=1",
                cardId, (int)markedHand, actualSource);
            if (!markedHand || actualSource != static_cast<int>(DiscardedSystem::Source::Hand)) {
                LOG_ERR("CustomComponents/DiscardFromHand/DISCARDED: HAND SOURCE MARK FAILED card=%d; suppress trigger", cardId);
                // Zone transfer already succeeded. Do not attempt to reverse it.
                // Presentation still needs the committed zone transfer record.
            }

            // The custom record below owns presentation of this committed transfer.
            CC_DIAG("CustomComponents/DiscardFromHand: MODEL SUCCESS card=%d owner=%d hand=%d pile=%d; recording frontend transfer",
                cardId, before.ownerId, before.handId, before.pileId);

            // Native custom record is the authoritative hand->pile mutation
            // evidence. Its frontend processor, not the simulation hook, queues
            // the card-bound discard animation once stock history reaches it.
            if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity()) {
                Engine::CustomRecords::Record r{};
                r.model=reinterpret_cast<std::uintptr_t>(model);
                r.kind=Engine::CustomRecords::Kind::DiscardOutsidePlay;
                r.stage=Engine::CustomRecords::Stage::Committed;
                r.from=Engine::CustomRecords::Origin::Hand;
                r.to=Engine::CustomRecords::Origin::DiscardPile;
                r.entityId=cardId;r.ownerId=before.ownerId;
                r.fromZoneId=before.handId;r.toZoneId=before.pileId;
                r.effectId=effect && effectEffectEntityId.IsValid()?effectEffectEntityId[effect].Get():-1;
                // Match::Provide can fail in some simulation injection scopes;
                // -1 is a fail-safe stock-history-caught-up presentation anchor.
                Obj* card=FindEntity(model,cardId);
                Obj* instances=card?Engine::Match::Instances(card):nullptr;
                Obj* history=instances?Engine::Match::Provide(instances,
                    BNM::Class("PvZCards.Engine","History").GetMonoType()):nullptr;
                static auto recs=BNM::Class("PvZCards.Engine","History").GetField("Records")
                    .cast<BNM::Structures::Mono::List<Obj*>*>();
                auto* list=history && recs.IsValid()?recs[history].Get():nullptr;
                if(list && list->size>=0)r.stockHistoryAnchor=list->size;
                r.history=reinterpret_cast<std::uintptr_t>(history);
                r.modelToken=Core::ModelRegistry::Identify(model,true);
                const auto serial=Engine::CustomRecords::Commit(r);
                if(!serial) {
                    LOG_ERR("CustomComponents/Records/HAND_DISCARD: required history commit failed card=%d",cardId);
                } else LOG_INFO("CustomComponents/Records/HAND_DISCARD: committed serial=%llu card=%d historyAnchor=%d",(unsigned long long)serial,cardId,r.stockHistoryAnchor);
            }

            if(markedHand && actualSource==static_cast<int>(DiscardedSystem::Source::Hand))
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
        CC_DIAG("CustomComponents/DiscardFromHand/DISCARDED: STOCK card=%d mark=%d Source=%d expected=1",
            cardId, (int)markedHand, actualSource);
        if (!markedHand || actualSource != static_cast<int>(DiscardedSystem::Source::Hand)) {
            LOG_ERR("CustomComponents/DiscardFromHand/DISCARDED: STOCK HAND SOURCE MARK FAILED card=%d; suppress trigger", cardId);
            return;
        }
        CC_DIAG("CustomComponents/DiscardOutsidePlayTrigger: HAND EntityId=%d PlayerId=%d DiscardPileId=%d",
            cardId, ctx.playerId, ctx.discardPileId);
        if (reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
            Integration::ReplayEventBridge::Publish("discard_hand_to_pile", {
                {"entityId",cardId},{"ownerId",ctx.playerId},
                {"pileId",ctx.discardPileId},{"stockRoute",1},{"verifiedTransfer",1}});
        CustomTriggerSystem::FireDiscardOutsidePlay(effect);
    }

