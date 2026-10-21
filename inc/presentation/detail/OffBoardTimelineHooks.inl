#include "diagnostics/DiagnosticLogging.hpp"
    static void TriggeredAbilityOnStartedHook(
        void* self, const void* method
    ) {
        Obj* animation = static_cast<Obj*>(self);
        ++presentationStartCaptureCount;

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: TriggeredAbility.OnStarted ENTER animation=%p count=%llu",
            animation,
            static_cast<unsigned long long>(presentationStartCaptureCount)
        );

        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: ONSTART-ENTER anim=%p count=%llu",
            animation, static_cast<unsigned long long>(presentationStartCaptureCount));
        // Pre-stock checkpoint for the exact native view lookup at OnStarted.
        // The actual view-map exception remains owned by stock code.
        Obj* diagnosticModel = nullptr;
        int diagnosticCard = -1;
        if (GetAnimationModelAndSource(animation, &diagnosticModel, &diagnosticCard)) {
            if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: ONSTART-BEFORE anim=%p group=%d card=%d model=%p deck=%d zone=%s mapper=%p",
                animation, traceAbilityGroupId.IsValid() ? traceAbilityGroupId[animation].Get() : -1,
                diagnosticCard, diagnosticModel,
                int(IsInMaterializedDeckPool(diagnosticModel, diagnosticCard)),
                ZoneName(GetOffBoardZone(diagnosticModel, diagnosticCard)),
                timelineEntityViewMapper.IsValid() ? timelineEntityViewMapper[animation].Get() : nullptr);
        }
        if (diagnosticCard >= 0)
            if constexpr (false) CC_DIAG("CustomComponents/TRICK-LIFECYCLE-DIAG: TRIGGERED-START anim=%p card=%d group=%d zone=%s mapper=%p",
                animation, diagnosticCard,
                traceAbilityGroupId.IsValid() ? traceAbilityGroupId[animation].Get() : -1,
                ZoneName(GetOffBoardZone(diagnosticModel, diagnosticCard)),
                timelineEntityViewMapper.IsValid() ? timelineEntityViewMapper[animation].Get() : nullptr);
        int presentationSourceId = -1;
        int zoneCardId = -1;
        OffBoardZone zone = OffBoardZone::None;

        // An off-board model alone is insufficient: normal played tricks
        // remain in discard with a mapped CardView across their play triggers.
        // Conversely, ordinary death/return triggers may start after a card's
        // CardView has been removed. Guard those even without a custom bridge.
        const bool explicitlyOffBoard = IsDeckOrDiscardSource(
            animation, &presentationSourceId, &zoneCardId, &zone);
        Obj* liveMapper = timelineEntityViewMapper.IsValid()
            ? timelineEntityViewMapper[animation].Get() : nullptr;
        Obj* mappedView = nullptr;
        const bool hasMappedView = liveMapper && diagnosticCard >= 0 &&
            TryResolveMappedCardView(liveMapper, diagnosticCard, &mappedView);
        const OffBoardZone currentZone = diagnosticModel && diagnosticCard >= 0
            ? GetOffBoardZone(diagnosticModel, diagnosticCard) : OffBoardZone::None;
        const bool confirmedOffBoard = (currentZone == OffBoardZone::Deck ||
            currentZone == OffBoardZone::DiscardPile ||
            (diagnosticModel && diagnosticCard >= 0 &&
             IsInMaterializedDeckPool(diagnosticModel, diagnosticCard)));
        const bool missingOffBoardView = confirmedOffBoard &&
            liveMapper && viewMapperViewMap.IsValid() && !hasMappedView;
        // Preserve existing bridge handling (including stale discard->hand),
        // but never suppress a legitimate currently mapped presentation.
        const bool shouldUseNullTimeline =
            (explicitlyOffBoard && !hasMappedView) || missingOffBoardView;
        if (missingOffBoardView && !explicitlyOffBoard) {
            presentationSourceId = diagnosticCard;
            zoneCardId = diagnosticCard;
            zone = currentZone == OffBoardZone::None ? OffBoardZone::Deck : currentZone;
            CC_DIAG("CustomComponents/UNIVERSAL-VIEW-GUARD: source-null card=%d zone=%s animation=%p mapper=%p",
                diagnosticCard, ZoneName(zone), animation, liveMapper);
        }
        if (!shouldUseNullTimeline)
        {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: NORMAL stock OnStarted ENTER animation=%p",
                animation
            );

            originalTriggeredAbilityOnStarted(self, method);

            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: NORMAL stock OnStarted EXIT animation=%p",
                animation
            );
            return;
        }

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: OFFBOARD base OnStarted ENTER animation=%p source=%d zoneCard=%d zone=%s",
            animation, presentationSourceId, zoneCardId, ZoneName(zone)
        );

        // The native base OnStarted performs GetViewObject<object> before
        // IsInLocalHand. Match only this animation's source mapper and ID.
        Obj* expectedMapper = timelineEntityViewMapper[animation].Get();
        if (!expectedMapper) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: offboard animation has null EntityViewMapper; cannot safely bypass lookup animation=%p",
                animation
            );
            return;
        }

        lifecycleTargetAnimation = animation;
        Obj* containerForLifecycle = timelineCommandContainer[animation].Get();
        lifecycleTargetQueue = (containerForLifecycle && commandAnimationQueue.IsValid())
            ? commandAnimationQueue[containerForLifecycle].Get() : nullptr;
        lifecycleQueueUpdates = 0;
        lifecycleQueueLastSignature = ~std::uint64_t{ 0 };
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: TRACK animation=%p container=%p queue=%p sourceId=%d",
            animation, containerForLifecycle, lifecycleTargetQueue, presentationSourceId);
        LogQueueLifecycle(lifecycleTargetQueue, "AT START", true);
        // Register before stock OnStarted in case the callback is synchronous.
        RememberLoadScope(animation, expectedMapper, presentationSourceId);
        ZoneNullTimelineScope scope(expectedMapper, presentationSourceId);
        LogWaitForLoad(animation, "BEFORE base OnStarted");
        timelineBaseOnStarted[animation]();
        LogWaitForLoad(animation, "AFTER base OnStarted");
        if (!timelineWaitForLoad[animation].Get()) {
            // No delayed callback is outstanding; avoid stale animation pointers.
            ForgetLoadScope(animation);
        } else {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DELAYEDSCOPE: retained animation=%p mapper=%p source=%d", animation, expectedMapper, presentationSourceId);
        }

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: OFFBOARD base OnStarted EXIT animation=%p",
            animation
        );

        LogCompletionState(animation);

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation: presentationSource=%d zoneCard=%d zone=%s used stock null timeline",
            presentationSourceId, zoneCardId, ZoneName(zone)
        );
    }

    static Obj* GetCardViewHook(void* self, const void* method) {
        bool offBoard = IsZoneNullTimelineActive();

        if (offBoard) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/CARDVIEW: get_CardView ENTER animation=%p",
                self
            );
        }

        Obj* result = originalGetCardView(self, method);

        if (offBoard) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/CARDVIEW: get_CardView EXIT animation=%p result=%p",
                self, result
            );
        }

        return result;
    }

    static bool IsInLocalHandHook(
        void* self, Obj* cardView, const void* method
    ) {
        if (IsZoneNullTimelineActive()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/CARDVIEW: IsInLocalHand REACHED cardView=%p forced=true",
                cardView
            );
            return true;
        }

        return originalIsInLocalHand(self, cardView, method);
    }

