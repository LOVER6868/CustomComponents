    // Preserve stock event animation construction; log the precise boundary.
    static Obj* CreateRequiredAnimationHook(void* self, const void* method) {
        if (!IsZoneNullTimelineActive())
            return originalCreateRequiredAnimation(self, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEONLY: Required ENTER action=%p source=%d", self, expectedOffBoardSourceId);
        Obj* result = originalCreateRequiredAnimation(self, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEONLY: Required EXIT action=%p animation=%p", self, result);
        return result;
    }

    // ARM64: VisualEffectEventAction.CreateTargetReactionAnimation dispatches
    // via [klass+0x238] (common target) and [klass+0x258] (source particle).
    // Log function pointers without mutating class metadata/vtables.
    inline void TraceTargetDispatch(void* action) {
        if (!action) return;
        void* klass = *reinterpret_cast<void**>(action);
        if (!klass) return;
        void* common = nullptr;
        void* particle = nullptr;
        std::memcpy(&common, static_cast<const std::uint8_t*>(klass) + 0x238, sizeof(common));
        std::memcpy(&particle, static_cast<const std::uint8_t*>(klass) + 0x258, sizeof(particle));
        int source = -1;
        void* mapper = nullptr;
        std::memcpy(&source, static_cast<const std::uint8_t*>(action) + 0x30, sizeof(source));
        std::memcpy(&mapper, static_cast<const std::uint8_t*>(action) + 0x20, sizeof(mapper));
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DISPATCH: action=%p klass=%p common=%p particle=%p source=%d mapper=%p expectedSource=%d expectedMapper=%p matched=%d",
            action, klass, common, particle, source, mapper,
            expectedOffBoardSourceId, expectedOffBoardMapper,
            static_cast<int>(IsZoneNullTimelineActive() && source == expectedOffBoardSourceId && mapper == expectedOffBoardMapper));
    }

    static Obj* DrawCommonTargetHook(void* self, const void* method) {
        if (IsZoneNullTimelineActive())
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMMON: ENTER action=%p", self);
        Obj* result = originalDrawCommonTarget(self, method);
        if (IsZoneNullTimelineActive())
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMMON: EXIT action=%p result=%p", self, result);
        return result;
    }

    // These are substantive native implementations (unlike the one-instruction
    // CreateCommonTargetReactionAnimation forwarding thunk at 0x1EDDB2C).
    // Trace both boundaries but do not change any returned animation.
    static Obj* CreateDrawAnimationTraceHook(void* self, const void* method) {
        if (!IsZoneNullTimelineActive())
            return originalCreateDrawAnimation(self, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DRAWTRACE: CreateDrawAnimation ENTER action=%p source=%d", self, expectedOffBoardSourceId);
        Obj* result = originalCreateDrawAnimation(self, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DRAWTRACE: CreateDrawAnimation EXIT action=%p result=%p", self, result);
        return result;
    }

    static Obj* BuildRecordAnimationTraceHook(void* self, Obj* records, int drawType, const void* method) {
        if (!IsZoneNullTimelineActive())
            return originalBuildFromRecords(self, records, drawType, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DRAWTRACE: BuildAnimationFromRecordList ENTER factory=%p records=%p drawType=%d", self, records, drawType);
        Obj* result = originalBuildFromRecords(self, records, drawType, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DRAWTRACE: BuildAnimationFromRecordList EXIT factory=%p result=%p", self, result);
        return result;
    }

    // First behavior test: only skip the *target reaction* when every target
    // is a confirmed deck/discard card. Fail closed for malformed/empty lists,
    // missing metadata, non-card entities, and mixed visible/offboard targets.
    // This never changes effect resolution or mutates TargetIds.
    struct TargetVisualDecision {
        bool skipAll = false;
        int count = 0;
        int deck = 0;
        int discard = 0;
        // Diagnostic: stale discard-scoped visual for a card already in hand.
        int staleReturnHand = 0;
    };

    inline TargetVisualDecision EvaluateTargetVisualPolicy(Obj* action) {
        TargetVisualDecision result{};
        if (!action || !traceVisualMapper.IsValid() ||
            !traceVisualTargetIds.IsValid() || !viewMapperEntityModel.IsValid() ||
            !entityExists.IsValid() || !getEntityById.IsValid() ||
            !traceEntityComponents.IsValid() || !traceOrderedPoolEntries.IsValid() ||
            !tracePoolEntryEntityId.IsValid() || !viewMapperViewMap.IsValid()) return result;
        Obj* mapper = traceVisualMapper[action].Get();
        Obj* model = mapper ? viewMapperEntityModel[mapper].Get() : nullptr;
        Obj* targetsObject = traceVisualTargetIds[action].Get();
        if (!model || !targetsObject) return result;
        auto* targets = reinterpret_cast<BNM::Structures::Mono::List<int>*>(targetsObject);
        const int count = targets->GetSize();
        if (count <= 0 || count > 128 || !targets->items) return result;
        const int* ids = targets->GetData();
        if (!ids) return result;
        // The stale discard-start bridge can begin after the real card has
        // acquired HandElement, before the new hand view exists. Suppress its
        // own outdated target visual exactly like a still-offboard target.
        // Normal hand actions never enter this matched offboard scope.
        const bool scopedSource = IsZoneNullTimelineActive() &&
            IsCurrentSourceAction(action);
        for (int i = 0; i < count; ++i) {
            const int id = ids[i];
            if (id < 0 || !entityExists[model](id)) return result;
            Obj* entity = getEntityById[model](id);
            // Require a real card. Decks may lack DeckElement.
            if (!entity || !TraceFindComponent(entity, "Card")) return result;
            const bool inHand = TraceFindComponent(entity, "HandElement") != nullptr;
            const bool discarded = TraceFindComponent(entity, "DiscardPileElement") != nullptr;
            const bool inDeck = IsInMaterializedDeckPool(model, id);
            // Retained stock trick / transition views are legitimate even
            // when the model has already entered discard.
            if ((discarded || inDeck) && viewMapperViewMap.IsValid() &&
                TryResolveMappedCardView(mapper, id, nullptr)) return result;
            if (inHand) {
                // Only an exact source==target match under the offboard
                // source scope qualifies; never suppress generic hand buffs.
                if (!scopedSource || id != expectedOffBoardSourceId ||
                    discarded || inDeck) return result;
                ++result.staleReturnHand;
                continue;
            }
            // Ambiguous memberships should not be suppressed.
            if (discarded == inDeck) return result;
            if (discarded) ++result.discard;
            else ++result.deck;
        }
        // A mixed hand/offboard target list must not be hidden wholesale.
        if (result.staleReturnHand && result.staleReturnHand != count)
            return TargetVisualDecision{};
        result.count = count;
        result.skipAll = true;
        return result;
    }

    inline Obj* MakeTargetVisualNoOp() {
        if (!nullEffectAnimationClass.IsValid() || !nullEffectAnimationCtor.IsValid())
            return nullptr;
        Obj* replacement = nullEffectAnimationClass.CreateNewInstance();
        if (!replacement) return nullptr;
        nullEffectAnimationCtor[replacement]();
        return replacement;
    }

    // Shared target boundary: suppress only validated all-offboard target visuals.
    static Obj* CreateTargetReactionAnimationHook(void* self, Obj* data, const void* method) {
        const auto n = ++targetBoundaryTraceCount;
        if (n <= 300) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: SHARED ENTER seq=%llu action=%p data=%p", (unsigned long long)n, self, data);
            TraceVisualActionTargets(static_cast<Obj*>(self), "SHARED ENTER");
        }
        // Never suppress damage feedback: the victim can already be in discard
        // while the floating damage/heart particle still needs its CardView.
        const char* actionType = TraceObjectType(static_cast<Obj*>(self));
        const bool damageAction = std::strcmp(actionType, "DamageEventAction") == 0;
        const bool ownedByOffBoardAbility = IsCurrentSourceAction(self);
        // All-offboard targets without mapped views are safe to suppress,
        // regardless of which gameplay effect produced the animation. Keep
        // stock damage/transition visuals and mixed-target reactions intact.
        const TargetVisualDecision policy = !damageAction
            ? EvaluateTargetVisualPolicy(static_cast<Obj*>(self)) : TargetVisualDecision{};
        if (policy.skipAll) {
            Obj* noop = MakeTargetVisualNoOp();
            if (noop) {
                // Capture exact cards for later UI synchronization. Do not touch
                // gameplay components or mutate the effect's TargetIds.
                if (std::strcmp(actionType, "BuffEventAction") == 0 ||
                    std::strcmp(actionType, "ModifySunCostEventAction") == 0) {
                    Obj* mapper = traceVisualMapper[static_cast<Obj*>(self)].Get();
                    Obj* model = mapper ? viewMapperEntityModel[mapper].Get() : nullptr;
                    Obj* idsObject = traceVisualTargetIds[static_cast<Obj*>(self)].Get();
                    if (model && idsObject) {
                        auto* ids = reinterpret_cast<BNM::Structures::Mono::List<int>*>(idsObject);
                        if (ids->GetSize() == policy.count && ids->items) {
                            const int* values = ids->GetData();
                            if (values) for (int i = 0; i < policy.count; ++i)
                                RememberFrameSync(model, values[i]);
                        }
                    }
                }
                if (policy.staleReturnHand) {
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-HAND-TARGET: suppressed stale source-matched hand target action=%p type=%s source=%d targets=%d animation=%p",
                             self, actionType, expectedOffBoardSourceId, policy.staleReturnHand, noop);
                }
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETPOLICY: SKIP_ALL action=%p type=%s targets=%d deck=%d discard=%d animation=%p scoped=%d",
                         self, TraceObjectType(static_cast<Obj*>(self)), policy.count,
                         policy.deck, policy.discard, noop, int(IsZoneNullTimelineActive()));
                return noop;
            }
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETPOLICY: NOOP_FAILED action=%p; stock retained", self);
        } else if (n <= 300) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETPOLICY: STOCK action=%p type=%s (non-offboard/mixed/unresolved)",
                     self, TraceObjectType(static_cast<Obj*>(self)));
        }
        if (IsZoneNullTimelineActive()) TraceTargetDispatch(self);
        Obj* result = originalCreateTargetReactionAnimation(self, data, method);
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: SHARED EXIT seq=%llu action=%p animation=%p type=%s", (unsigned long long)n, self, result, TraceObjectType(result));
        return result;
    }

    // The concrete buff method is 0x1EDC9B0; the observed lookup
    // 0x1EDCA34 is inside it. This hook is read-only and passes through.
    static Obj* BuffCommonTargetTraceHook(void* self, const void* method) {
        const auto n = ++buffBoundaryTraceCount;
        if (n <= 300) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: BUFF ENTER seq=%llu action=%p", (unsigned long long)n, self);
            TraceVisualActionTargets(static_cast<Obj*>(self), "BUFF ENTER");
        }
        Obj* result = originalBuffCommonTarget(self, method);
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: BUFF EXIT seq=%llu action=%p animation=%p type=%s", (unsigned long long)n, self, result, TraceObjectType(result));
        return result;
    }

    // Source-only animation substitution. The common target reaction and any
    // draw-record-driven hand presentation are constructed by stock code.
    // This handler is intentionally reusable by other concrete event actions.
    inline Obj* MakeSourceParticleNoOp(void* action, const char* type) {
        if (!nullEffectAnimationClass.IsValid() || !nullEffectAnimationCtor.IsValid()) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEPARTICLE: metadata missing type=%s", type);
            return nullptr;
        }
        Obj* replacement = nullEffectAnimationClass.CreateNewInstance();
        if (!replacement) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEPARTICLE: allocation failed type=%s", type);
            return nullptr;
        }
        nullEffectAnimationCtor[replacement]();
        ++sourceParticleSubstitutions;
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEPARTICLE: NOOP type=%s action=%p animation=%p source=%d count=%llu",
            type, action, replacement, expectedOffBoardSourceId,
            static_cast<unsigned long long>(sourceParticleSubstitutions));
        return replacement;
    }

    // The dump places SourceId at +0x30 and the injected mapper at +0x20.
    // Check both against the currently executing off-board timeline so a
    // nested unrelated event cannot have its source effect suppressed.
    inline bool IsCurrentSourceAction(void* action) {
        if (!IsZoneNullTimelineActive() || !action) return false;
        const auto* bytes = static_cast<const std::uint8_t*>(action);
        int sourceId = -1;
        void* mapper = nullptr;
        std::memcpy(&sourceId, bytes + 0x30, sizeof(sourceId));
        std::memcpy(&mapper, bytes + 0x20, sizeof(mapper));
        return sourceId == expectedOffBoardSourceId && mapper == expectedOffBoardMapper;
    }

    static Obj* DrawSourceParticleHook(
        void* self, Obj* data, const void* method
    ) {
        CUSTOMCOMPONENTS_ZTAP_TRACE("SOURCEPARTICLE: HOOK ENTER action=%p", self);

        bool matched = IsCurrentSourceAction(self);

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "SOURCEPARTICLE: CHECK matched=%d active=%d",
            static_cast<int>(matched),
            static_cast<int>(IsZoneNullTimelineActive())
        );

        if (matched) {
            Obj* noop = MakeSourceParticleNoOp(
                self, "DrawAllCardsEventAction"
            );
            if (noop) return noop;
        }

        CUSTOMCOMPONENTS_ZTAP_TRACE("SOURCEPARTICLE: STOCK FORWARD ENTER action=%p original=%p method=%p", self,
            reinterpret_cast<void*>(originalDrawSourceParticle), method);
        Obj* stockResult = originalDrawSourceParticle(self, data, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("SOURCEPARTICLE: STOCK FORWARD EXIT action=%p result=%p", self, stockResult);
        return stockResult;
    }

    static Obj* BaseSourceParticleHook(void* self, Obj* data, const void* method) {
        if (IsZoneNullTimelineActive()) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEPARTICLE: BASE ENTER action=%p matched=%d", self, static_cast<int>(IsCurrentSourceAction(self)));
        if (IsCurrentSourceAction(self)) {
            Obj* noop = MakeSourceParticleNoOp(self, "VisualEffectEventAction");
            if (noop) return noop;
        }
        return originalBaseSourceParticle(self, data, method);
    }

    // Shared off-board source-visual guard. VisualEffectEventAction's slot 18
    // CreatePersistentVisuals() requires a source CardView. For a known deck /
    // discard source, preserve the event's target animation but use the game's
    // own NullEffectAnimation for this *source-dependent* persistent stage.
    // Do not affect unrelated actions, including valid on-board sources.
    static Obj* CreatePersistentVisualsHook(void* self, Obj* data, const void* method) {
        if (!IsZoneNullTimelineActive())
            return originalCreatePersistentVisuals(self, data, method);

        const bool matched = IsCurrentSourceAction(self);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/PERSISTENT: ENTER action=%p data=%p matched=%d source=%d",
            self, data, static_cast<int>(matched), expectedOffBoardSourceId);

        if (matched) {
            Obj* replacement = MakeSourceParticleNoOp(self, "PersistentVisuals");
            if (replacement) {
                ++persistentVisualSubstitutions;
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/PERSISTENT: NOOP action=%p animation=%p source=%d count=%llu",
                    self, replacement, expectedOffBoardSourceId,
                    static_cast<unsigned long long>(persistentVisualSubstitutions));
                return replacement;
            }
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/PERSISTENT: NOOP allocation failed; forwarding stock action=%p", self);
        }

        Obj* result = originalCreatePersistentVisuals(self, data, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/PERSISTENT: STOCK EXIT action=%p result=%p", self, result);
        return result;
    }

