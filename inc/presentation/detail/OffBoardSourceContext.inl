    inline thread_local int zoneNullTimelineDepth = 0;

    // Only the source lookup belonging to the currently starting off-board
    // animation may be bypassed. Keep this context thread-local to avoid
    // affecting unrelated animations on another thread.
    inline thread_local void* expectedOffBoardMapper = nullptr;
    inline thread_local int expectedOffBoardSourceId = -1;

    struct ZoneNullTimelineScope {
        void* previousMapper;
        int previousSourceId;

        ZoneNullTimelineScope(void* mapper, int sourceId)
            : previousMapper(expectedOffBoardMapper),
            previousSourceId(expectedOffBoardSourceId) {
            expectedOffBoardMapper = mapper;
            expectedOffBoardSourceId = sourceId;
            ++zoneNullTimelineDepth;
        }
        ~ZoneNullTimelineScope() {
            --zoneNullTimelineDepth;
            expectedOffBoardMapper = previousMapper;
            expectedOffBoardSourceId = previousSourceId;
        }
        ZoneNullTimelineScope(const ZoneNullTimelineScope&) = delete;
        ZoneNullTimelineScope& operator=(const ZoneNullTimelineScope&) = delete;
    };

    inline bool IsZoneNullTimelineActive() {
        return zoneNullTimelineDepth > 0;
    }

    // Carry the off-board source context across asynchronous command loading.
    // The callback consumes its entry before forwarding to stock code.
    struct PendingLoadScope {
        Obj* animation = nullptr;
        void* mapper = nullptr;
        int sourceId = -1;
    };
    inline std::array<PendingLoadScope, 128> pendingLoadScopes{};

    inline void RememberLoadScope(Obj* animation, void* mapper, int sourceId) {
        if (!animation || !mapper || sourceId < 0) return;
        for (auto& entry : pendingLoadScopes) {
            if (entry.animation == animation || !entry.animation) {
                entry = {animation, mapper, sourceId};
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/DELAYEDSCOPE: registration full animation=%p", animation);
    }

    inline bool ConsumeLoadScope(Obj* animation, PendingLoadScope& result) {
        for (auto& entry : pendingLoadScopes) {
            if (entry.animation == animation && animation) {
                result = entry;
                entry = {};
                return true;
            }
        }
        return false;
    }

    inline void ForgetLoadScope(Obj* animation) {
        for (auto& entry : pendingLoadScopes) {
            if (entry.animation == animation && animation) entry = {};
        }
    }

    enum class OffBoardZone : std::uint8_t {
        None = 0,
        Deck = 1,
        DiscardPile = 2
    };

    inline constexpr std::size_t kMaxPendingPresentations = 128;

    struct PendingPresentation {
        Obj* model = nullptr;
        int presentationSourceId = -1;
        int eventCardId = -1;
        OffBoardZone zone = OffBoardZone::None;
        std::uint64_t sequence = 0;
    };

    inline std::array<PendingPresentation, kMaxPendingPresentations>
        pendingPresentations{};

    inline std::uint64_t nextPendingSequence = 1;
    inline std::uint64_t presentationStartCaptureCount = 0;

    // First-test stock off-board registration. Key by the exact animation,
    // not by AbilityGroupId or just the card ID (both are non-unique).
    struct AutoOffBoardPresentation {
        Obj* animation = nullptr;
        Obj* model = nullptr;
        int sourceId = -1;
        int effectEntityId = -1;
        OffBoardZone zone = OffBoardZone::None;
    };
    inline std::array<AutoOffBoardPresentation, 128> autoOffBoardPresentations{};

    // DISCARD -> LANE DIAGNOSTIC provenance. The engine can process a
    // discard-scoped triggered start AFTER our custom effect has moved its
    // ORIGINAL card into a lane. Record the exact originating EffectEntityId
    // rather than treating all lane cards with an InDiscardPileQuery as stale.
    // This table changes presentation classification only, not gameplay.
    struct DiscardToLaneStartOrigin {
        Obj* model = nullptr;
        int cardId = -1;
        int effectEntityId = -1;
        int laneId = -1;
    };
    inline std::array<DiscardToLaneStartOrigin, 64> discardToLaneStartOrigins{};

    inline void RememberDiscardToLaneMove(Obj* model, int cardId,
                                          int effectEntityId, int laneId) {
        if (!model || cardId < 0 || effectEntityId < 0 || laneId < 0) return;
        for (auto& entry : discardToLaneStartOrigins) {
            if ((entry.model == model && entry.cardId == cardId &&
                 entry.effectEntityId == effectEntityId) || !entry.model) {
                entry = {model, cardId, effectEntityId, laneId};
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-ORIGIN: registered model=%p card=%d effect=%d lane=%d",
                         model, cardId, effectEntityId, laneId);
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-ORIGIN: table full, stock presentation retained card=%d effect=%d",
                cardId, effectEntityId);
    }

    inline bool IsDiscardToLaneOrigin(Obj* model, int cardId,
                                      int effectEntityId = -1) {
        if (!model || cardId < 0) return false;
        for (const auto& entry : discardToLaneStartOrigins) {
            if (entry.model == model && entry.cardId == cardId &&
                (effectEntityId < 0 || entry.effectEntityId == effectEntityId))
                return true;
        }
        return false;
    }


