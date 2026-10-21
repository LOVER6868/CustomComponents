// Internal implementation fragment for ZoneTriggeredAbilityPresentationSystem.
// Stock lane-creation and board-entry presentation hooks.
// Included at its original declaration position inside the owning namespace.
// Not a standalone public header; hook installation remains in the parent header.

    // DISCARD -> LANE / LANEENTRY: stock board-entry trace, READ ONLY.
    // This uses the original CreateCardInLaneRecord and never creates a view,
    // changes animation completion, or corrects gameplay/frame values.
    // Dump / ARM64 RVAs are checked again before installing each optional hook.
    // =====================================================================
    using LaneEntryVoidFn = void (*)(void*, const void*);
    using LaneEntryCardStateFn = void (*)(void*, Obj*, const void*);
    using LaneEntryAddToLaneFn = void (*)(void*, Obj*, int, const void*);
    using LaneEntryCreateEventFn = Obj* (*)(void*, Obj*, const void*);

    inline LaneEntryCreateEventFn originalLaneCreateCardEvent = nullptr;
    inline LaneEntryVoidFn originalLanePreload = nullptr;
    inline LaneEntryVoidFn originalLaneCreateStarted = nullptr;
    inline LaneEntryVoidFn originalLaneShowCard = nullptr;
    inline LaneEntryVoidFn originalLaneCompleted = nullptr;
    inline LaneEntryAddToLaneFn originalLaneAddToLane = nullptr;
    inline LaneEntryCardStateFn originalLanePlayedToBoard = nullptr;

    inline BNM::Field<int> laneAnimationCardId;
    inline BNM::Field<int> laneAnimationLaneId;
    inline BNM::Field<int> laneAnimationOrder;
    inline BNM::Field<bool> laneAnimationFaceDown;
    inline BNM::Field<bool> laneAnimationComplete;
    inline BNM::Field<Obj*> laneAnimationCardView;
    inline BNM::Field<Obj*> laneAnimationViewMapper;
    inline BNM::Field<int> laneEventCardId;
    inline BNM::Field<int> laneEventOrder;
    inline BNM::Field<bool> laneEventFaceDown;
    inline BNM::Field<int> laneRecordTargetId;
    inline BNM::Method<Obj*> laneRecordGetCard;
    inline BNM::Field<int> laneCardDataEntityId;
    inline std::uint64_t laneTraceStartCount = 0;
    inline std::uint64_t laneTraceShowCount = 0;
    inline std::uint64_t laneTraceAddCount = 0;
    inline std::uint64_t laneTraceBoardCount = 0;
    inline std::uint64_t laneTraceCompleteCount = 0;
    inline std::uint64_t laneTraceEventCount = 0;
    inline std::uint64_t laneTracePreloadCount = 0;

    inline int LaneReadAnimationCard(Obj* animation) {
        return animation && laneAnimationCardId.IsValid()
            ? laneAnimationCardId[animation].Get() : -1;
    }

    inline void LaneTraceAnimation(Obj* animation, const char* stage) {
        if (!animation) return;
        const int cardId = LaneReadAnimationCard(animation);
        Obj* view = laneAnimationCardView.IsValid()
            ? laneAnimationCardView[animation].Get() : nullptr;
        CUSTOMCOMPONENTS_LANE_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: %s animation=%p card=%d lane=%d order=%d faceDown=%d complete=%d cardView=%p viewType=%s",
            stage, animation, cardId,
            laneAnimationLaneId.IsValid() ? laneAnimationLaneId[animation].Get() : -1,
            laneAnimationOrder.IsValid() ? laneAnimationOrder[animation].Get() : -1,
            laneAnimationFaceDown.IsValid() ? int(laneAnimationFaceDown[animation].Get()) : -1,
            laneAnimationComplete.IsValid() ? int(laneAnimationComplete[animation].Get()) : -1,
            view, TraceObjectType(view)
        );
    }

    inline void LaneTraceCardFrame(Obj* view, const char* stage) {
        if (!view || !cardViewEntityId.IsValid() || !cardViewFrame.IsValid() ||
            std::strcmp(TraceObjectType(view), "CardViewComponent") != 0) return;
        const int id = cardViewEntityId[view].Get();
        Obj* model = nullptr;
        for (const auto& bound : frameViewBindings) {
            if (bound.view == view && bound.id == id) { model = bound.model; break; }
        }
        Obj* frame = cardViewFrame[view].Get();
        int gameplayAttack = -99999;
        if (model && id >= 0 && entityExists.IsValid() && getEntityById.IsValid() &&
            entityExists[model](id) && componentCalculateAttack.IsValid()) {
            Obj* entity = getEntityById[model](id);
            Obj* attack = TraceFindComponent(entity, "Attack");
            if (attack) gameplayAttack = componentCalculateAttack[attack]();
        }
        CUSTOMCOMPONENTS_LANE_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: %s view=%p card=%d model=%p frame=%p attackFrame=%d attackGameplay=%d healthMax=%d healthCurrent=%d sunCost=%d (observed only)",
            stage, view, id, model, frame,
            frame && frameCurrentAttack.IsValid() ? frameCurrentAttack[frame].Get() : -99999,
            gameplayAttack,
            frame && frameMaxHealth.IsValid() ? frameMaxHealth[frame].Get() : -99999,
            frame && frameCurrentHealth.IsValid() ? frameCurrentHealth[frame].Get() : -99999,
            frame && frameCurrentSunCost.IsValid() ? frameCurrentSunCost[frame].Get() : -99999
        );
    }

    // DISCARD -> LANE: the original entity is already in the lane when the
    // stock create-card animation runs, but its new frame is populated from
    // printed prefab values. Reuse the proven Fix3 reconciliation only after
    // the stock ShowCard board-entry initialization has finished.
    // The provenance check excludes ordinary hand plays, stock-created cards,
    // and other off-board movement; gameplay entities remain untouched.
    inline void ReconcileReturnedLaneView(Obj* view) {
        if (!view || !cardViewEntityId.IsValid() ||
            std::strcmp(TraceObjectType(view), "CardViewComponent") != 0)
            return;
        const int id = cardViewEntityId[view].Get();
        if (id < 0) return;
        Obj* model = nullptr;
        for (const auto& binding : frameViewBindings) {
            if (binding.view == view && binding.id == id) {
                model = binding.model;
                break;
            }
        }
        if (!model ||
            (!IsDiscardToLaneOrigin(model, id) && !HasLaneReturnSnapshot(model, id)) ||
            !entityExists.IsValid() || !getEntityById.IsValid() ||
            !entityExists[model](id)) return;
        Obj* entity = getEntityById[model](id);
        if (!entity || !TraceFindComponent(entity, "LaneElement") ||
            TraceFindComponent(entity, "DiscardPileElement")) {
            CUSTOMCOMPONENTS_LANE_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-SYNC: SKIP card=%d model=%p reason=not-in-lane", id, model);
            return;
        }
        CUSTOMCOMPONENTS_LANE_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-SYNC: BEGIN card=%d model=%p view=%p", id, model, view);
        RememberFrameSync(model, id);
        ReconcileVisibleAttack(view, "RETURN_TO_LANE");
        LaneTraceCardFrame(view, "RETURN-LANE FRAME AFTER SYNC");
        CUSTOMCOMPONENTS_LANE_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-SYNC: END card=%d view=%p", id, view);
    }

    static Obj* LaneCreateCardEventTraceHook(void* self, Obj* record, const void* method) {
        const auto n = ++laneTraceEventCount;
        int recordCardId = -1;
        if (record && laneRecordGetCard.IsValid() && laneCardDataEntityId.IsValid()) {
            Obj* data = laneRecordGetCard[record]();
            if (data) recordCardId = laneCardDataEntityId[data].Get();
        }
        if (n <= 200) CUSTOMCOMPONENTS_LANE_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: CREATE_EVENT ENTER seq=%llu record=%p card=%d laneTarget=%d",
            (unsigned long long)n, record, recordCardId,
            record && laneRecordTargetId.IsValid() ? laneRecordTargetId[record].Get() : -1);
        Obj* action = originalLaneCreateCardEvent(self, record, method);
        if (n <= 200) CUSTOMCOMPONENTS_LANE_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: CREATE_EVENT EXIT seq=%llu action=%p type=%s actionCard=%d order=%d faceDown=%d",
            (unsigned long long)n, action, TraceObjectType(action),
            action && laneEventCardId.IsValid() ? laneEventCardId[action].Get() : -1,
            action && laneEventOrder.IsValid() ? laneEventOrder[action].Get() : -1,
            action && laneEventFaceDown.IsValid() ? int(laneEventFaceDown[action].Get()) : -1);
        return action;
    }

    static void LanePreloadTraceHook(void* self, const void* method) {
        const auto n = ++laneTracePreloadCount;
        if (n <= 200) CUSTOMCOMPONENTS_LANE_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: PRELOAD ENTER seq=%llu action=%p card=%d",
            (unsigned long long)n, self,
            self && laneEventCardId.IsValid() ? laneEventCardId[static_cast<Obj*>(self)].Get() : -1);
        originalLanePreload(self, method);
        if (n <= 200) CUSTOMCOMPONENTS_LANE_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: PRELOAD EXIT seq=%llu action=%p",
            (unsigned long long)n, self);
    }

    static void LaneCreateStartedTraceHook(void* self, const void* method) {
        const auto n = ++laneTraceStartCount;
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "CREATE_ON_STARTED BEFORE");
        // I.6 frontend capture, rooted for the current scene. No model/view mutation.
        Obj* i6Animation = static_cast<Obj*>(self);
        Obj* i6Mapper = laneAnimationViewMapper.IsValid()
            ? laneAnimationViewMapper[i6Animation].Get() : nullptr;
        Obj* i6Model = i6Mapper && viewMapperEntityModel.IsValid()
            ? viewMapperEntityModel[i6Mapper].Get() : nullptr;
        Core::SceneViewContext::Capture(i6Mapper, i6Model);
        originalLaneCreateStarted(self, method);
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "CREATE_ON_STARTED AFTER");
    }

    static void LaneShowCardTraceHook(void* self, const void* method) {
        const auto n = ++laneTraceShowCount;
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "SHOW_CARD BEFORE");
        originalLaneShowCard(self, method);
        // Do not depend on EntityViewMapper.AddViewObject having observed this
        // exact view. The stock lane animation itself has the current mapper,
        // model, ID and newly initialized CardView on BOTH client viewpoints.
        Obj* animation = static_cast<Obj*>(self);
        Obj* laneView = laneAnimationCardView.IsValid()
            ? laneAnimationCardView[animation].Get() : nullptr;
        Obj* mapper = laneAnimationViewMapper.IsValid()
            ? laneAnimationViewMapper[animation].Get() : nullptr;
        Obj* model = mapper && viewMapperEntityModel.IsValid()
            ? viewMapperEntityModel[mapper].Get() : nullptr;
        const int id = laneAnimationCardId.IsValid()
            ? laneAnimationCardId[animation].Get() : -1;
        if (laneView && model && id >= 0 && cardViewEntityId.IsValid() &&
            cardViewEntityId[laneView].Get() == id &&
            std::strcmp(TraceObjectType(laneView), "CardViewComponent") == 0) {
            BindFrameView(laneView, model, id);
            // Preserve the existing pre-trigger snapshot on the initiating
            // client. Opponent clients have no such ticket; the deferred idle
            // reconciliation below works directly from their live model.
            if (IsDiscardToLaneOrigin(model, id) || HasLaneReturnSnapshot(model, id))
                ReconcileReturnedLaneView(laneView);
            // Do not expose unrevealed face-down cards through stat changes.
            if (laneAnimationFaceDown.IsValid() &&
                !laneAnimationFaceDown[animation].Get())
                RememberIdleLaneFrame(animation, mapper, model, laneView, id);
        }
        if (n <= 200) {
            LaneTraceAnimation(static_cast<Obj*>(self), "SHOW_CARD AFTER");
            if (laneAnimationCardView.IsValid())
                LaneTraceCardFrame(laneAnimationCardView[static_cast<Obj*>(self)].Get(), "SHOW_CARD FRAME");
        }
    }

    static void LaneCompletedTraceHook(void* self, const void* method) {
        const auto n = ++laneTraceCompleteCount;
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "CREATE_COMPLETED BEFORE");
        originalLaneCompleted(self, method);
        if (laneAnimationCardView.IsValid())
            RepairResetLaneFrame(laneAnimationCardView[static_cast<Obj*>(self)].Get(), "CREATE_COMPLETED");
        // Stock creation completed. Actual model-to-frame correction waits
        // until this animation's owning queue is empty (after buff visuals).
        CompleteIdleLaneFrame(static_cast<Obj*>(self));
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "CREATE_COMPLETED AFTER");
    }

    static void LaneAddToLaneTraceHook(void* self, Obj* card, int order, const void* method) {
        const auto n = ++laneTraceAddCount;
        if (n <= 200) {
            CUSTOMCOMPONENTS_LANE_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: ADD_TO_LANE ENTER seq=%llu laneView=%p cardView=%p type=%s order=%d",
                (unsigned long long)n, self, card, TraceObjectType(card), order);
            LaneTraceCardFrame(card, "ADD_TO_LANE FRAME BEFORE");
        }
        originalLaneAddToLane(self, card, order, method);
        if (n <= 200) {
            CUSTOMCOMPONENTS_LANE_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: ADD_TO_LANE EXIT seq=%llu laneView=%p cardView=%p order=%d",
                (unsigned long long)n, self, card, order);
            LaneTraceCardFrame(card, "ADD_TO_LANE FRAME AFTER");
        }
    }

    static void LanePlayedToBoardTraceHook(void* self, Obj* state, const void* method) {
        const auto n = ++laneTraceBoardCount;
        if (n <= 200) LaneTraceCardFrame(static_cast<Obj*>(self), "PLAYED_TO_BOARD BEFORE");
        originalLanePlayedToBoard(self, state, method);
        // Late binding fallback, then repair only a reset to printed stats.
        // The one-shot pre-trigger snapshot is never consumed twice.
        ReconcileReturnedLaneView(static_cast<Obj*>(self));
        RepairResetLaneFrame(static_cast<Obj*>(self), "PLAYED_TO_BOARD");
        if (n <= 200) LaneTraceCardFrame(static_cast<Obj*>(self), "PLAYED_TO_BOARD AFTER");
    }

