    inline bool GetAnimationModelAndSource(
        Obj* animation, Obj** modelOut, int* sourceIdOut
    ) {
        if (!animation ||
            !timelineEntityViewMapper.IsValid() ||
            !timelineSourceCardId.IsValid() ||
            !viewMapperEntityModel.IsValid())
            return false;

        int sourceId = timelineSourceCardId[animation].Get();
        Obj* mapper = timelineEntityViewMapper[animation].Get();
        Obj* model = mapper ? viewMapperEntityModel[mapper].Get() : nullptr;

        if (modelOut) *modelOut = model;
        if (sourceIdOut) *sourceIdOut = sourceId;

        return model && sourceId >= 0;
    }

    inline void RegisterOffBoardTriggerPresentation(
        Obj* model,
        int presentationSourceId,
        int eventCardId,
        OffBoardZone authoritativeZone = OffBoardZone::None
    ) {
        if (!model || presentationSourceId < 0 || eventCardId < 0)
            return;

        OffBoardZone zone =
            authoritativeZone != OffBoardZone::None
            ? authoritativeZone
            : GetOffBoardZone(model, eventCardId);

        if (zone == OffBoardZone::None) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation: bridge skipped presentationSource=%d eventCard=%d (zone not yet identifiable) model=%p",
                presentationSourceId, eventCardId, model
            );
            return;
        }

        if (presentationSourceId == eventCardId &&
            authoritativeZone == OffBoardZone::None)
            return;

        std::size_t slot = pendingPresentations.size();
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        std::size_t oldestSlot = 0;

        for (std::size_t i = 0; i < pendingPresentations.size(); ++i) {
            const auto& entry = pendingPresentations[i];

            if (!entry.model) {
                slot = i;
                break;
            }

            if (entry.sequence < oldest) {
                oldest = entry.sequence;
                oldestSlot = i;
            }
        }

        if (slot == pendingPresentations.size())
            slot = oldestSlot;

        pendingPresentations[slot] = {
            model,
            presentationSourceId,
            eventCardId,
            zone,
            nextPendingSequence++
        };

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation: queued bridge presentationSource=%d eventCard=%d zone=%s model=%p",
            presentationSourceId, eventCardId, ZoneName(zone), model
        );
    }

    inline bool TakePendingPresentation(
        Obj* model,
        int presentationSourceId,
        int* eventCardIdOut,
        OffBoardZone* zoneOut
    ) {
        std::size_t bestSlot = pendingPresentations.size();
        std::uint64_t bestSequence = std::numeric_limits<std::uint64_t>::max();

        for (std::size_t i = 0; i < pendingPresentations.size(); ++i) {
            const auto& entry = pendingPresentations[i];

            if (!entry.model ||
                entry.model != model ||
                entry.presentationSourceId != presentationSourceId)
                continue;

            if (entry.sequence < bestSequence) {
                bestSequence = entry.sequence;
                bestSlot = i;
            }
        }

        if (bestSlot == pendingPresentations.size()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: bridge MISS model=%p presentationSource=%d",
                model, presentationSourceId
            );
            return false;
        }

        PendingPresentation entry = pendingPresentations[bestSlot];
        pendingPresentations[bestSlot] = {};

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: bridge CONSUMED model=%p presentationSource=%d eventCard=%d zone=%s sequence=%llu",
            model, presentationSourceId, entry.eventCardId,
            ZoneName(entry.zone),
            static_cast<unsigned long long>(entry.sequence)
        );

        if (eventCardIdOut) *eventCardIdOut = entry.eventCardId;
        if (zoneOut) *zoneOut = entry.zone;

        return true;
    }

    inline bool IsDeckOrDiscardSource(
        Obj* animation,
        int* presentationSourceIdOut = nullptr,
        int* zoneCardIdOut = nullptr,
        OffBoardZone* zoneOut = nullptr
    ) {
        Obj* model = nullptr;
        int sourceId = -1;

        if (!GetAnimationModelAndSource(animation, &model, &sourceId)) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: animation source lookup FAILED animation=%p",
                animation
            );
            return false;
        }

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: animation source resolved animation=%p model=%p source=%d",
            animation, model, sourceId
        );

        if (presentationSourceIdOut)
            *presentationSourceIdOut = sourceId;

        if (traceAbilityGroupId.IsValid()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ONSTART animation=%p source=%d abilityGroup=%d model=%p",
                animation, sourceId, traceAbilityGroupId[animation].Get(), model);
        }
        TraceBridgeCandidates(model, sourceId, animation, "ONSTART");

        // BRIDGEONLY: deck/discard membership by itself is NOT proof of an
        // off-board custom trigger. Played Tricks routinely enter discard
        // before their own presentation starts and must keep their stock path.
        // Keep the zone lookup for diagnostics only, never classification.
        const OffBoardZone directZone = GetOffBoardZone(model, sourceId);
        int eventCardId = -1;
        OffBoardZone bridgedZone = OffBoardZone::None;

        // Existing explicit custom-trigger bridge takes precedence, notably
        // DiscardOutsidePlayTrigger, which intentionally carries an event-card hint.
        if (!TakePendingPresentation(model, sourceId, &eventCardId, &bridgedZone)) {
            OffBoardZone autoZone = OffBoardZone::None;
            if (TakeAutoOffBoardPresentation(animation, model, sourceId, autoZone)) {
                if (zoneCardIdOut) *zoneCardIdOut = sourceId;
                if (zoneOut) *zoneOut = autoZone;
                return true;
            }
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGEONLY: STOCK animation=%p source=%d currentZone=%s reason=no-bridge",
                animation, sourceId, ZoneName(directZone)
            );
            return false;
        }

        if (zoneCardIdOut) *zoneCardIdOut = eventCardId;
        if (zoneOut) *zoneOut = bridgedZone;

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGEONLY: MATCH animation=%p source=%d eventCard=%d registeredZone=%s currentZone=%s",
            animation, sourceId, eventCardId, ZoneName(bridgedZone), ZoneName(directZone)
        );
        return bridgedZone != OffBoardZone::None;
    }

    // Targeted off-board workaround: never ask EntityViewMapper for a
    // deck/discard source that has no playable CardView. All other queries
    // still go through the unmodified native implementation.
    static Obj* GetViewObjectGenericHook(
        void* self, int entityId, const void* method
    ) {
        const bool offBoard = IsZoneNullTimelineActive();
        const bool sourceMatches =
            offBoard &&
            expectedOffBoardMapper != nullptr &&
            self == expectedOffBoardMapper &&
            entityId == expectedOffBoardSourceId;

        if (sourceMatches) {
            // __builtin_return_address(0) identifies the native call site.
            // With a normal inline hook this is the return PC in its caller;
            // dladdr-style base subtraction makes the RVA stable across ASLR.
            const auto returnPc = reinterpret_cast<std::uintptr_t>(__builtin_extract_return_addr(__builtin_return_address(0)));
            const auto returnRva = nativeIl2cppBase && returnPc >= nativeIl2cppBase
                ? returnPc - nativeIl2cppBase : 0;
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/CALLERTRACE: source=%d mapper=%p returnPC=%p returnRVA=0x%llx base=%p",
                entityId, self, reinterpret_cast<void*>(returnPc),
                static_cast<unsigned long long>(returnRva), reinterpret_cast<void*>(nativeIl2cppBase)
            );
            // DISCARD -> HAND: A synthetic DrawCardRecord uses the ordinary
            // DrawAllCardsAnimation.AddCardToDraw() card-view lookup. That call
            // legitimately needs the *existing* live card's frontend view.
            // Our null-source guard otherwise steals it from the draw animation
            // and can leave its hand-entry animation unable to complete.
            // Only let the EXACT audited caller through when the entity is now
            // a hand card (and no longer in discard); every other off-board
            // caller retains the old null-source protection.
            if (returnRva == kDrawAddCardViewLookupReturnRva &&
                viewMapperEntityModel.IsValid() &&
                entityExists.IsValid() && getEntityById.IsValid() &&
                traceEntityComponents.IsValid()) {
                Obj* mapper = static_cast<Obj*>(self);
                Obj* model = viewMapperEntityModel[mapper].Get();
                Obj* entity = model && entityId >= 0 && entityExists[model](entityId)
                    ? getEntityById[model](entityId) : nullptr;
                const bool isLiveHandCard =
                    entity &&
                    TraceFindComponent(entity, "Card") &&
                    TraceFindComponent(entity, "HandElement") &&
                    !TraceFindComponent(entity, "DiscardPileElement");
                if (isLiveHandCard) {
                    Obj* view = originalGetViewObjectGeneric(self, entityId, method);
                    CUSTOMCOMPONENTS_ZTAP_TRACE(
                        "CustomComponents/ZoneTriggeredAbilityPresentation/DRAW-HAND-LOOKUP: ALLOW source=%d model=%p view=%p type=%s callerRVA=0x%llx",
                        entityId, model, view, TraceObjectType(view),
                        static_cast<unsigned long long>(returnRva)
                    );
                    return view;
                }
                CUSTOMCOMPONENTS_ZTAP_TRACE(
                    "CustomComponents/ZoneTriggeredAbilityPresentation/DRAW-HAND-LOOKUP: REJECT source=%d model=%p card=%d hand=%d discard=%d",
                    entityId, model,
                    int(entity && TraceFindComponent(entity, "Card")),
                    int(entity && TraceFindComponent(entity, "HandElement")),
                    int(entity && TraceFindComponent(entity, "DiscardPileElement"))
                );
            }
            // DISCARD -> LANE: do not steal the *legitimate* newly-loaded
            // CardView from CreateCardAnimation.OnStarted. This is a single
            // verified native callsite, and only applies to cards registered
            // by our discard->lane gameplay path which are now in LaneElement.
            if (returnRva == kCreateCardViewLookupReturnRva &&
                viewMapperEntityModel.IsValid() && entityExists.IsValid() &&
                getEntityById.IsValid() && traceEntityComponents.IsValid()) {
                Obj* mapper = static_cast<Obj*>(self);
                Obj* model = viewMapperEntityModel[mapper].Get();
                Obj* entity = model && entityId >= 0 && entityExists[model](entityId)
                    ? getEntityById[model](entityId) : nullptr;
                const bool inLaneFromDiscard = entity &&
                    TraceFindComponent(entity, "Card") &&
                    TraceFindComponent(entity, "LaneElement") &&
                    !TraceFindComponent(entity, "DiscardPileElement") &&
                    !TraceFindComponent(entity, "HandElement") &&
                    IsDiscardToLaneOrigin(model, entityId);
                if (inLaneFromDiscard) {
                    Obj* view = originalGetViewObjectGeneric(self, entityId, method);
                    CUSTOMCOMPONENTS_ZTAP_TRACE(
                        "CustomComponents/ZoneTriggeredAbilityPresentation/CREATE-LANE-LOOKUP: ALLOW source=%d model=%p view=%p type=%s callerRVA=0x%llx",
                        entityId, model, view, TraceObjectType(view),
                        static_cast<unsigned long long>(returnRva)
                    );
                    return view;
                }
                CUSTOMCOMPONENTS_ZTAP_TRACE(
                    "CustomComponents/ZoneTriggeredAbilityPresentation/CREATE-LANE-LOOKUP: REJECT source=%d model=%p lane=%d discard=%d origin=%d",
                    entityId, model,
                    int(entity && TraceFindComponent(entity, "LaneElement")),
                    int(entity && TraceFindComponent(entity, "DiscardPileElement")),
                    int(IsDiscardToLaneOrigin(model, entityId))
                );
            }
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/VIEWLOOKUP: BYPASS offboard source mapper=%p entityId=%d result=null",
                self, entityId
            );
            return nullptr;
        }

        if (offBoard) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/VIEWLOOKUP: PASS-THROUGH mapper=%p entityId=%d expectedMapper=%p expectedId=%d",
                self, entityId, expectedOffBoardMapper, expectedOffBoardSourceId
            );
        }

        Obj* result = originalGetViewObjectGeneric(self, entityId, method);

        if (offBoard) {
            CUSTOMCOMPONENTS_ZTAP_TRACE(
                "CustomComponents/ZoneTriggeredAbilityPresentation/VIEWLOOKUP: PASS-THROUGH EXIT mapper=%p entityId=%d result=%p",
                self, entityId, result
            );
        }
        return result;
    }

