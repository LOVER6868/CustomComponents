
#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <cstring>
#include <dlfcn.h>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "log.h"

// Development traces are opt-in. Keep error reporting and the final
// initialization summary visible in normal builds.
#ifndef CUSTOMCOMPONENTS_ZTAP_VERBOSE
#define CUSTOMCOMPONENTS_ZTAP_VERBOSE 0
#endif
#if CUSTOMCOMPONENTS_ZTAP_VERBOSE
#define CUSTOMCOMPONENTS_ZTAP_TRACE(...) LOG_INFO(__VA_ARGS__)
#else
#define CUSTOMCOMPONENTS_ZTAP_TRACE(...) ((void)0)
#endif

namespace CustomComponents::Systems::ZoneTriggeredAbilityPresentationSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;

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
    inline std::uint64_t presentationStartTraceCount = 0;

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


    inline BNM::Field<Obj*> timelineEntityViewMapper;
    inline BNM::Field<int> timelineSourceCardId;
    inline BNM::Field<Obj*> viewMapperEntityModel;

    inline BNM::Method<bool> entityExists;
    inline BNM::Method<Obj*> getEntityById;
    inline BNM::Method<bool> entityHasComponent;

    inline Obj* deckElementType = nullptr;
    inline Obj* discardPileElementType = nullptr;

    inline BNM::Method<void> timelineBaseOnStarted;

    inline BNM::Class nullEffectAnimationClass;
    inline BNM::Method<void> nullEffectAnimationCtor;
    inline std::uint64_t sourceParticleSubstitutions = 0;
    inline std::uint64_t persistentVisualSubstitutions = 0;

    // Diagnostic-only completion state: no flags or commands are mutated.
    inline BNM::Field<Obj*> timelineCommandContainer;
    inline BNM::Field<bool> timelineIsCardTimelineComplete;
    inline BNM::Field<bool> timelineWaitForLoad;
    inline BNM::Method<bool> commandHasAllCommandsBeenActivated;
    inline BNM::Method<bool> commandIsChildAnimationsComplete;
    inline BNM::Method<bool> timelineIsCompleted;
    inline BNM::Field<Obj*> commandEventCommands;

    // Diagnostic-only lifecycle target: most recent off-board presentation.
    inline Obj* lifecycleTargetAnimation = nullptr;
    inline Obj* lifecycleTargetQueue = nullptr;
    inline BNM::Field<Obj*> commandAnimationQueue;
    inline BNM::Field<Obj*> queueAnimations;
    inline BNM::Field<Obj*> queueFrontAnimations;
    inline BNM::Field<bool> effectIsStarted;
    inline BNM::Field<bool> effectIsCancelled;
    inline std::uint64_t lifecycleQueueUpdates = 0;
    inline std::uint64_t lifecycleQueueLastSignature = ~std::uint64_t{ 0 };


    // Preserve stock particle registration, including Trick animations.
    // Source-only and persistent no-ops are handled by their dedicated hooks.

    inline void LogWaitForLoad(Obj* animation, const char* phase) {
        if (!animation || !timelineWaitForLoad.IsValid()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/WAITLOAD: %s metadata unavailable animation=%p", phase, animation);
            return;
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/WAITLOAD: %s animation=%p waitForLoad=%d",
            phase, animation,
            static_cast<int>(timelineWaitForLoad[animation].Get())
        );
    }

    inline void LogCompletionState(Obj* animation) {
        if (!animation || !timelineCommandContainer.IsValid() ||
            !timelineIsCardTimelineComplete.IsValid()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: metadata unavailable animation=%p", animation);
            return;
        }

        Obj* container = timelineCommandContainer[animation].Get();
        const bool timelineComplete = timelineIsCardTimelineComplete[animation].Get();
        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: BEGIN animation=%p container=%p timelineComplete=%d",
            animation, container, static_cast<int>(timelineComplete)
        );

        if (!container) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: no command container; skipping checks");
            return;
        }

        // The individual checks make it clear which call does not return,
        // and avoid drawing conclusions from a single IsCompleted result.
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: HasAllCommandsBeenActivated ENTER container=%p", container);
        const bool allActivated = commandHasAllCommandsBeenActivated[container]();
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: HasAllCommandsBeenActivated EXIT result=%d", static_cast<int>(allActivated));

        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: IsChildAnimationsComplete ENTER container=%p", container);
        const bool childrenComplete = commandIsChildAnimationsComplete[container]();
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: IsChildAnimationsComplete EXIT result=%d", static_cast<int>(childrenComplete));

        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: IsCompleted ENTER animation=%p", animation);
        const bool completed = timelineIsCompleted[animation]();
        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/COMPLETION: IsCompleted EXIT result=%d (timeline=%d activated=%d children=%d)",
            static_cast<int>(completed), static_cast<int>(timelineComplete),
            static_cast<int>(allActivated), static_cast<int>(childrenComplete)
        );
    }

    // Exact generic specialization found in the uploaded IL2CPP dump.
    // ARM64 RVA: 0x238E7B0
    inline constexpr std::uintptr_t kGenericGetViewObjectRva = 0x238E7B0;
    // DrawAllCardsAnimation.AddCardToDraw calls the generic lookup at
    // 0x1F0D288, returning to 0x1F0D28C (verified on this APK's ARM64 binary).
    // Match the exact call site, not the whole draw animation or RVA range.
    inline constexpr std::uintptr_t kDrawAddCardViewLookupReturnRva = 0x1F0D28C;
    // CreateCardAnimation.OnStarted: BL GetViewObject at 0x1F0206C;
    // exact ARM64 return PC relative to libil2cpp.so base is 0x1F02070.
    inline constexpr std::uintptr_t kCreateCardViewLookupReturnRva = 0x1F02070;
    inline std::uintptr_t nativeIl2cppBase = 0;

    using GetViewObjectGenericFn = Obj * (*)(void*, int, const void*);
    using GetCardViewFn = Obj * (*)(void*, const void*);
    using TriggeredAbilityOnStartedFn = void (*)(void*, const void*);
    using IsInLocalHandFn = bool (*)(void*, Obj*, const void*);
    using AddParticleEffectAnimationFn =
        void (*)(void*, Obj*, Obj*, const void*);
    using ExecuteRemainingCommandsFn = void (*)(void*, const void*);
    using HandleCommandLoadCompleteFn = void (*)(void*, const void*);
    using CreateRequiredAnimationFn = Obj * (*)(void*, const void*);
    using CreateTargetReactionAnimationFn = Obj * (*)(void*, Obj*, const void*);
    using SourceParticleFn = Obj * (*)(void*, Obj*, const void*);
    using PersistentVisualsFn = Obj * (*)(void*, Obj*, const void*);
    using CommonTargetFn = Obj * (*)(void*, const void*);
    using DrawAnimationFn = Obj * (*)(void*, const void*);
    using BuildRecordAnimationFn = Obj * (*)(void*, Obj*, int, const void*);
    using LifecycleVoidFn = void (*)(void*, const void*);
    using LifecycleBoolFn = bool (*)(void*, const void*);


    inline GetViewObjectGenericFn originalGetViewObjectGeneric = nullptr;
    inline GetCardViewFn originalGetCardView = nullptr;
    inline TriggeredAbilityOnStartedFn originalTriggeredAbilityOnStarted = nullptr;
    // BRIDGETRACE originals and counters are defined with their hooks below.

    inline IsInLocalHandFn originalIsInLocalHand = nullptr;
    inline AddParticleEffectAnimationFn originalAddParticleEffectAnimation = nullptr;
    inline ExecuteRemainingCommandsFn originalExecuteRemainingCommands = nullptr;
    inline HandleCommandLoadCompleteFn originalHandleCommandLoadComplete = nullptr;
    inline CreateRequiredAnimationFn originalCreateRequiredAnimation = nullptr;
    inline CreateTargetReactionAnimationFn originalCreateTargetReactionAnimation = nullptr;
    inline CommonTargetFn originalDrawCommonTarget = nullptr;
    inline CommonTargetFn originalBuffCommonTarget = nullptr;
    inline DrawAnimationFn originalCreateDrawAnimation = nullptr;
    inline BuildRecordAnimationFn originalBuildFromRecords = nullptr;
    inline SourceParticleFn originalDrawSourceParticle = nullptr;
    inline SourceParticleFn originalBaseSourceParticle = nullptr;
    inline PersistentVisualsFn originalCreatePersistentVisuals = nullptr;
    inline LifecycleVoidFn originalCheckForCompletion = nullptr;
    inline LifecycleVoidFn originalQueueUpdate = nullptr;
    inline LifecycleBoolFn originalQueueIsEmpty = nullptr;
    inline LifecycleBoolFn originalTriggeredCanYield = nullptr;


    inline const char* ZoneName(OffBoardZone zone) {
        switch (zone) {
        case OffBoardZone::Deck: return "deck";
        case OffBoardZone::DiscardPile: return "discard";
        default: return "none";
        }
    }

    // BRIDGETRACE: read-only history / Trick / triggered-ability correlations.
    using OneTimeRecordFn = void (*)(void*, Obj*, const void*);
    using TriggerRecordGroupFn = void (*)(void*, Obj*, int, Obj*, const void*);
    using CreateAbilityCoreFn = Obj* (*)(void*, int, int, Obj*, bool, const void*);
    using AddAbilityRecordFn = Obj* (*)(void*, Obj*, int, Obj*, const void*);
    inline OneTimeRecordFn originalOneTimeRecord = nullptr;
    inline TriggerRecordGroupFn originalTriggerRecordGroup = nullptr;
    inline CreateAbilityCoreFn originalCreateAbilityCore = nullptr;
    inline AddAbilityRecordFn originalAddAbilityRecord = nullptr;
    inline BNM::Method<int> oneTimeCardId;
    inline BNM::Method<int> oneTimeHandId;
    inline BNM::Method<int> oneTimeLocationId;
    inline BNM::Field<int> traceAbilityGroupId;
    // Diagnostic-only: read start-record keys only after Dictionary.ContainsKey.
    inline BNM::Method<Obj*> traceRecordGetDictionary;
    inline BNM::Method<int> traceRecordGetInt;
    inline BNM::Field<Obj*> traceProcessorEntityModel;
    inline BNM::Method<int> traceEffectEntityGetSource;
    // RECORDZONE diagnostic metadata. No gameplay/query results are changed.
    inline BNM::Field<Obj*> traceModelEntities;
    inline BNM::Field<Obj*> traceEntityComponents;
    inline BNM::Field<int> traceCardGuid;
    inline BNM::Field<Obj*> traceSelfFilterQuery;
    inline BNM::Field<Obj*> traceCompositeQueries;
    inline BNM::Field<Obj*> traceOrderedPoolEntries;
    inline BNM::Field<int> tracePoolEntryEntityId;
    inline BNM::Field<int> tracePoolEntryGuid;
    inline BNM::Field<int> traceVisualSourceId;
    inline BNM::Field<Obj*> traceVisualTargetIds;
    inline BNM::Field<Obj*> traceVisualMapper;
    inline std::uint64_t targetBoundaryTraceCount = 0;
    inline std::uint64_t buffBoundaryTraceCount = 0;

    inline const char* TraceObjectType(Obj* object);
    inline Obj* TraceFindComponent(Obj* entity, const char* name);
    inline bool IsCurrentSourceAction(void* action);

    // Only cards with a *suppressed* off-board buff are eligible for UI repair.
    struct PendingFrameSync { Obj* model = nullptr; int entityId = -1; };
    inline std::array<PendingFrameSync, 512> pendingFrameSyncs{};
    inline BNM::Field<int> cardViewEntityId;
    inline BNM::Field<Obj*> cardViewFrame;
    inline BNM::Field<int> frameCurrentAttack;
    inline BNM::Method<void> frameBuffAttack;
    inline BNM::Method<int> componentCalculateAttack;
    // Generic hand-entry synchronization. Stock GrantAbilityPresentation remains
    // the sole contributor of custom abilities to SpecialAbilityChecker.
    inline BNM::Field<int> frameMaxHealth;
    inline BNM::Field<int> frameCurrentHealth;
    inline BNM::Field<int> frameCurrentSunCost;
    inline BNM::Field<int> healthCurrentDamage;
    inline BNM::Field<Obj*> healthMaxHealth;
    inline BNM::Field<Obj*> sunCostValue;
    inline BNM::Method<int> calculateModifiableValue;
    inline BNM::Method<void> frameBuffHealth;
    inline BNM::Method<void> frameModifySunCost;
    inline BNM::Method<void> frameSetSpecialAbilitySprites;
    inline BNM::Class stateClass;
    inline BNM::Class checkerClass;
    inline BNM::Method<void> stateCtor;
    inline BNM::Method<void> checkerCtor;
    inline BNM::Method<void> checkerAddSpecialAbilities;
    inline BNM::Field<Obj*> stateAbilities;
    inline bool traitSyncReady = false;
    // A minimal EntityState is constructed solely for its initialized Abilities
    // list. No factory injection, stat mutation or new hook is needed.

    using CardLoadedFn = void (*)(void*, Obj*, const void*);
    inline CardLoadedFn originalCardLoaded = nullptr;

    inline void RememberFrameSync(Obj* model, int id) {
        if (!model || id < 0) return;
        for (auto& e : pendingFrameSyncs) {
            if (e.model == model && e.entityId == id) return;
            if (!e.model) { e = {model, id}; return; }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: tracking full id=%d", id);
    }

    // VIEWTRACE: diagnostic-only. Never apply a BuffAttack correction during
    // card loading; it is too early to establish the final view association.
    inline void TraceFrameForCard(Obj* view, const char* stage, int mappedId = -1, Obj* model = nullptr) {
        if (!view) return;
        const int cardId = cardViewEntityId.IsValid() ? cardViewEntityId[view].Get() : -1;
        const bool tracked = [&]() {
            for (const auto& e : pendingFrameSyncs)
                if (e.model && e.entityId == (mappedId >= 0 ? mappedId : cardId) &&
                    (!model || model == e.model)) return true;
            return false;
        }();
        Obj* frame = cardViewFrame.IsValid() ? cardViewFrame[view].Get() : nullptr;
        const int frameAttack = frame && frameCurrentAttack.IsValid() ? frameCurrentAttack[frame].Get() : -99999;
        int gameplayAttack = -99999;
        Obj* resolvedModel = model;
        const int id = mappedId >= 0 ? mappedId : cardId;
        if (!resolvedModel) for (const auto& e : pendingFrameSyncs)
            if (e.model && e.entityId == id) { resolvedModel = e.model; break; }
        if (resolvedModel && id >= 0 && entityExists.IsValid() && getEntityById.IsValid() &&
            entityExists[resolvedModel](id)) {
            Obj* entity = getEntityById[resolvedModel](id);
            Obj* attack = TraceFindComponent(entity, "Attack");
            if (attack && componentCalculateAttack.IsValid())
                gameplayAttack = componentCalculateAttack[attack]();
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: %s view=%p viewType=%s mappedId=%d cardId=%d tracked=%d model=%p frame=%p frameAttack=%d gameplayAttack=%d",
                 stage, view, TraceObjectType(view), mappedId, cardId, int(tracked), resolvedModel, frame, frameAttack, gameplayAttack);
    }

    static void CardLoadedSyncHook(void* self, Obj* data, const void* method) {
        Obj* view = static_cast<Obj*>(self);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: OnCardLoaded ENTER view=%p data=%p", self, data);
        if (view) TraceFrameForCard(view, "OnCardLoaded BEFORE");
        originalCardLoaded(self, data, method);
        if (view) TraceFrameForCard(view, "OnCardLoaded AFTER");
    }

    using AddViewObjectFn = void (*)(void*, int, Obj*, const void*);
    using ShowInHandFn = void (*)(void*, const void*);
    inline AddViewObjectFn originalAddViewObject = nullptr;
    inline ShowInHandFn originalShowInHand = nullptr;

    // Exact model/entity/view association, captured only after stock mapper registration.
    // This avoids guessing the EntityModel from a CardId shared across matches.
    struct FrameViewBinding { Obj* view = nullptr; Obj* model = nullptr; int id = -1; };
    inline std::array<FrameViewBinding, 256> frameViewBindings{};

    // Hand/deck -> discard -> hand: capture provenance on the gameplay thread
    // while Discarded.Source still exists. The frontend consumes a ticket only
    // on a newly shown card view. This does not duplicate off-board buff deltas
    // or apply gameplay buffs; the model is the only source of target values.
    // Integer identity + numeric model address only; no cross-thread Unity access.
    struct HandDeckReturnTicket {
        std::uintptr_t model = 0;
        int cardId = -1;
        int source = 0;
        unsigned remaining = 0;
        std::uintptr_t lastView = 0;
        // Captured before ReturnedFromDiscardPileTrigger changes the model.
        int attackBeforeTrigger = -99999;
        int healthBeforeTrigger = -99999;
        int damageBeforeTrigger = -99999;
        int costBeforeTrigger = -99999;
        // SpecialAbilityChecker reports both stock and registry-driven traits.
        // Copy enum values only; never carry managed objects across threads.
        std::array<int, 32> traitsBeforeTrigger{};
        unsigned traitCountBeforeTrigger = 0;
        bool traitSnapshotValid = false;
    };
    inline std::mutex handDeckReturnMutex;
    inline std::array<HandDeckReturnTicket, 512> handDeckReturnTickets{};

    inline void RememberHandDeckReturn(Obj* model, int id, int source) {
        if (!model || id < 0 || (source != 1 && source != 2)) return;
        // This function is called after the live entity moves into hand but
        // BEFORE FireReturnedFromDiscardPile. Read gameplay stats only here.
        int attackValue = -99999, healthValue = -99999;
        int damageValue = -99999, costValue = -99999;
        std::array<int, 32> traitValues{};
        unsigned traitCount = 0;
        bool traitValid = false;
        if (entityExists.IsValid() && getEntityById.IsValid() &&
            entityExists[model](id)) {
            Obj* entity = getEntityById[model](id);
            Obj* attack = TraceFindComponent(entity, "Attack");
            Obj* health = TraceFindComponent(entity, "Health");
            Obj* cost = TraceFindComponent(entity, "SunCost");
            if (attack && componentCalculateAttack.IsValid())
                attackValue = componentCalculateAttack[attack]();
            if (health && healthMaxHealth.IsValid() && calculateModifiableValue.IsValid()) {
                Obj* value = healthMaxHealth[health].Get();
                if (value) healthValue = calculateModifiableValue(value);
            }
            if (health && healthCurrentDamage.IsValid())
                damageValue = healthCurrentDamage[health].Get();
            if (cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid()) {
                Obj* value = sunCostValue[cost].Get();
                if (value) costValue = calculateModifiableValue(value);
                if (costValue != -99999 && costValue < 0) costValue = 0;
            }
            // Capture PRE-trigger traits using the existing registry bridge.
            // Its hook runs within SpecialAbilityChecker::AddSpecialAbilities.
            if (traitSyncReady && stateClass.IsValid() && checkerClass.IsValid()) {
                Obj* state = stateClass.CreateNewInstance();
                Obj* checker = checkerClass.CreateNewInstance();
                if (state && checker) {
                    stateCtor[state]();
                    checkerCtor[checker]();
                    Obj* abilities = stateAbilities[state].Get();
                    if (abilities) {
                        checkerAddSpecialAbilities[checker](abilities, entity);
                        // EntityState.Abilities is a managed List<enum> (int-backed).
                        auto* list = reinterpret_cast<BNM::Structures::Mono::List<int>*>(abilities);
                        if (list->size >= 0 && list->size <= static_cast<int>(traitValues.size()) &&
                            (list->size == 0 || list->items)) {
                            traitCount = static_cast<unsigned>(list->size);
                            for (unsigned i = 0; i < traitCount; ++i)
                                traitValues[i] = list->items->m_Items[i];
                            traitValid = true;
                        } else {
                            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: snapshot overflow id=%d count=%d", id, list->size);
                        }
                    }
                }
            }
        }
        std::lock_guard<std::mutex> lock(handDeckReturnMutex);
        HandDeckReturnTicket* empty = nullptr;
        const auto key = reinterpret_cast<std::uintptr_t>(model);
        for (auto& e : handDeckReturnTickets) {
            if (e.model == key && e.cardId == id) {
                if (e.remaining < 32) ++e.remaining;
                e.source = source;
                e.attackBeforeTrigger = attackValue;
                e.healthBeforeTrigger = healthValue;
                e.damageBeforeTrigger = damageValue;
                e.costBeforeTrigger = costValue;
                e.traitsBeforeTrigger = traitValues;
                e.traitCountBeforeTrigger = traitCount;
                e.traitSnapshotValid = traitValid;
                LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: SNAPSHOT id=%d source=%d count=%u valid=%d (updated ticket)", id, source, traitCount, int(traitValid));
                LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: SNAPSHOT id=%d source=%d atk=%d hp=%d damage=%d cost=%d (updated ticket)",
                    id, source, attackValue, healthValue, damageValue, costValue);
                return;
            }
            if (!e.model && !empty) empty = &e;
        }
        if (empty) {
            *empty = {key, id, source, 1, 0, attackValue, healthValue, damageValue, costValue};
            empty->traitsBeforeTrigger = traitValues;
            empty->traitCountBeforeTrigger = traitCount;
            empty->traitSnapshotValid = traitValid;
            LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: SNAPSHOT id=%d source=%d count=%u valid=%d", id, source, traitCount, int(traitValid));
            LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: SNAPSHOT id=%d source=%d atk=%d hp=%d damage=%d cost=%d",
                id, source, attackValue, healthValue, damageValue, costValue);
        } else LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: ticket table full card=%d", id);
    }

    inline bool HasHandDeckReturn(Obj* model, int id) {
        std::lock_guard<std::mutex> lock(handDeckReturnMutex);
        const auto key = reinterpret_cast<std::uintptr_t>(model);
        for (const auto& e : handDeckReturnTickets)
            if (e.model == key && e.cardId == id && e.remaining) return true;
        return false;
    }

    inline HandDeckReturnTicket ConsumeHandDeckReturnSnapshot(Obj* model, int id, Obj* view) {
        if (!view) return {};
        std::lock_guard<std::mutex> lock(handDeckReturnMutex);
        const auto key = reinterpret_cast<std::uintptr_t>(model);
        const auto viewKey = reinterpret_cast<std::uintptr_t>(view);
        for (auto& e : handDeckReturnTickets) {
            if (e.model != key || e.cardId != id || !e.remaining) continue;
            if (e.lastView == viewKey) return {};
            HandDeckReturnTicket result = e;
            --e.remaining;
            if (e.remaining) e.lastView = viewKey;
            else e = {};
            return result;
        }
        return {};
    }

    inline void BindFrameView(Obj* view, Obj* model, int id) {
        if (!view || !model || id < 0) return;
        for (auto& binding : frameViewBindings) {
            if (binding.view == view || !binding.view) {
                binding = {view, model, id};
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: view binding table full id=%d", id);
    }

    // PHASE 2A: diagnostic-only accumulation for a single off-board visit.
    // Entries are keyed by (EntityModel*, entityId), not by raw Entity*.
    // They are retired on first hand/lane comparison so subsequent returns do
    // not accidentally inherit the earlier trip's changes.
    struct OffboardDeltaEntry {
        Obj* model = nullptr;
        int cardId = -1;
        int attack = 0;
        int health = 0;
        int cost = 0;
        unsigned events = 0;
        unsigned invalidFields = 0;
        unsigned traitObservations = 0;
        std::uint64_t firstSignature = 0;
        std::uint64_t lastSignature = 0;
        Obj* lastComparedView = nullptr;
        bool compared = false;
    };
    inline std::array<OffboardDeltaEntry, 512> offboardDeltaEntries{};
    inline std::uint64_t offboardDeltaComparisons = 0;
    inline std::uint64_t offboardDeltaOverflow = 0;

    inline OffboardDeltaEntry* FindOffboardDelta(Obj* model, int id, bool create) {
        if (!model || id < 0) return nullptr;
        OffboardDeltaEntry* freeEntry = nullptr;
        for (auto& entry : offboardDeltaEntries) {
            if (entry.model == model && entry.cardId == id) return &entry;
            if (!entry.model && !freeEntry) freeEntry = &entry;
        }
        if (create && freeEntry) {
            *freeEntry = {};
            freeEntry->model = model;
            freeEntry->cardId = id;
            return freeEntry;
        }
        if (create && offboardDeltaOverflow++ < 8)
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: table full model=%p card=%d (read-only)", model, id);
        return nullptr;
    }

    // Reports the recorded off-board deltas and the hypothetical full-value
    // gaps immediately before Phase 2B's selective correction.
    inline void CompareOffboardDelta(Obj* model, int id, Obj* view, Obj* frame,
                                      int gameplayAttack, const char* stage) {
        auto* entry = FindOffboardDelta(model, id, false);
        if (!entry || !frame || !frameCurrentAttack.IsValid()) return;
        if (entry->compared && entry->lastComparedView == view) return;
        const int beforeAttack = frameCurrentAttack[frame].Get();
        const std::int64_t fullAttack = static_cast<std::int64_t>(gameplayAttack) - beforeAttack;
        int fullHealth = -99999, fullCost = -99999;
        Obj* entity = entityExists.IsValid() && getEntityById.IsValid() && entityExists[model](id)
            ? getEntityById[model](id) : nullptr;
        if (entity) {
            Obj* health = TraceFindComponent(entity, "Health");
            if (health && healthMaxHealth.IsValid() && calculateModifiableValue.IsValid() && frameMaxHealth.IsValid()) {
                Obj* value = healthMaxHealth[health].Get();
                if (value) fullHealth = calculateModifiableValue(value) - frameMaxHealth[frame].Get();
            }
            Obj* cost = TraceFindComponent(entity, "SunCost");
            if (cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid() && frameCurrentSunCost.IsValid()) {
                Obj* value = sunCostValue[cost].Get();
                if (value) {
                    int current = calculateModifiableValue(value);
                    if (current < 0) current = 0;
                    fullCost = current - frameCurrentSunCost[frame].Get();
                }
            }
        }
        if (++offboardDeltaComparisons <= 256)
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: COMPARE stage=%s model=%p card=%d view=%p events=%u offboardAttack=%d offboardHealth=%d offboardCost=%d fullAttack=%lld fullHealth=%d fullCost=%d invalid=%u traitTypeChanges=%u traitSignature=%llx->%llx (Phase 2B selective sync)",
                stage, model, id, view, entry->events, entry->attack, entry->health,
                entry->cost, (long long)fullAttack, fullHealth, fullCost,
                entry->invalidFields, entry->traitObservations,
                (unsigned long long)entry->firstSignature,
                (unsigned long long)entry->lastSignature);
        entry->compared = true;
        entry->lastComparedView = view;
        // Keep the entry until a *new* off-board mutation arrives, at which
        // point the next off-board visit begins a fresh observation epoch.
    }

    // Phase 2B: only the gameplay deltas observed in Deck/Discard are
    // eligible for manual frame updates. A missing/invalid epoch means stock
    // presentation owns the result. Do not replay an epoch on the same view.
    inline int SelectOffboardFrameDelta(int pending, int gap) {
        // Never move the frame away from the currently authoritative gameplay
        // value. This also avoids replaying a delta stock has already shown.
        if (pending > 0 && gap > 0) return pending < gap ? pending : gap;
        if (pending < 0 && gap < 0) return pending > gap ? pending : gap;
        return 0;
    }

    inline void ReconcileVisibleAttack(Obj* view, const char* syncStage = "SHOW_IN_HAND") {
        if (!view || !cardViewEntityId.IsValid() || !cardViewFrame.IsValid() ||
            !frameCurrentAttack.IsValid() || !frameBuffAttack.IsValid() ||
            !componentCalculateAttack.IsValid() || !entityExists.IsValid() ||
            !getEntityById.IsValid()) return;
        const int id = cardViewEntityId[view].Get();
        Obj* model = nullptr;
        for (const auto& binding : frameViewBindings) {
            if (binding.view == view && binding.id == id) {
                model = binding.model;
                break;
            }
        }
        if (!model) return;
        bool tracked = false;
        for (const auto& pending : pendingFrameSyncs) {
            if (pending.model == model && pending.entityId == id) {
                tracked = true;
                break;
            }
        }
        const bool returnTicket = HasHandDeckReturn(model, id);
        if ((!tracked && !returnTicket) || !entityExists[model](id)) return;
        Obj* entity = getEntityById[model](id);
        if (!entity || !TraceFindComponent(entity, "Card")) return;
        Obj* attack = TraceFindComponent(entity, "Attack");
        Obj* frame = cardViewFrame[view].Get();
        if (!frame) return;
        // Only hand/deck-origin returned cards in an actual hand may use the
        // full model-to-frame gap. Lane->discard must remain stock-owned.
        const bool returnedToHand = returnTicket &&
            TraceFindComponent(entity, "HandElement") &&
            !TraceFindComponent(entity, "LaneElement") &&
            !TraceFindComponent(entity, "DiscardPileElement");
        if (!attack && !returnedToHand) return;
        const HandDeckReturnTicket snapshot = returnedToHand
            ? ConsumeHandDeckReturnSnapshot(model, id, view) : HandDeckReturnTicket{};
        const int returnSource = snapshot.source;
        const bool fullReturn = returnSource == 1 || returnSource == 2;
        // Two-stage return: restore ONLY the snapshot from before the
        // ReturnedFromDiscardPileTrigger. Any later trigger buff must be
        // presented by its own stock visual event, not by this correction.
        if (fullReturn) {
            Obj* offboardEntity = entity;
            (void)offboardEntity;
            const int beforeAtk = frameCurrentAttack[frame].Get();
            const int beforeHp = frameMaxHealth.IsValid() ? frameMaxHealth[frame].Get() : -99999;
            const int beforeCost = frameCurrentSunCost.IsValid() ? frameCurrentSunCost[frame].Get() : -99999;
            int atkApplied = 0, hpApplied = 0, costApplied = 0;
            auto boundedGap = [](int target, int actual) -> int {
                if (target == -99999 || actual == -99999) return 0;
                const std::int64_t gap = static_cast<std::int64_t>(target) - actual;
                return gap >= -10000 && gap <= 10000 ? static_cast<int>(gap) : 0;
            };
            if (snapshot.attackBeforeTrigger != -99999 && frameBuffAttack.IsValid()) {
                atkApplied = boundedGap(snapshot.attackBeforeTrigger, beforeAtk);
                if (atkApplied) frameBuffAttack[frame](atkApplied);
            }
            if (snapshot.healthBeforeTrigger != -99999 &&
                snapshot.damageBeforeTrigger >= 0 &&
                snapshot.damageBeforeTrigger <= snapshot.healthBeforeTrigger &&
                frameMaxHealth.IsValid() && frameCurrentHealth.IsValid() &&
                frameBuffHealth.IsValid()) {
                const int current = frameCurrentHealth[frame].Get();
                // BuffHealth preserves current damage. Don't change a frame
                // whose damage state is inconsistent with the snapshot.
                if (current == beforeHp - snapshot.damageBeforeTrigger) {
                    hpApplied = boundedGap(snapshot.healthBeforeTrigger, beforeHp);
                    if (hpApplied) frameBuffHealth[frame](hpApplied);
                }
            }
            if (snapshot.costBeforeTrigger != -99999 &&
                frameCurrentSunCost.IsValid() && frameModifySunCost.IsValid()) {
                costApplied = boundedGap(snapshot.costBeforeTrigger, beforeCost);
                if (costApplied) frameModifySunCost[frame](costApplied);
            }
            LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: PRETRIGGER_RESTORE id=%d source=%d view=%p snapshot=%d/%d cost=%d frameBefore=%d/%d cost=%d appliedAtk=%d appliedHp=%d appliedCost=%d frameAfter=%d/%d cost=%d (later triggers stock-owned)",
                id, returnSource, view, snapshot.attackBeforeTrigger,
                snapshot.healthBeforeTrigger, snapshot.costBeforeTrigger,
                beforeAtk, beforeHp, beforeCost, atkApplied, hpApplied, costApplied,
                frameCurrentAttack[frame].Get(),
                frameMaxHealth.IsValid() ? frameMaxHealth[frame].Get() : -99999,
                frameCurrentSunCost.IsValid() ? frameCurrentSunCost[frame].Get() : -99999);
            // Restore only abilities that existed BEFORE this return trigger.
            // Later GrantAbilityEffect presentation is still owned by stock.
            if (snapshot.traitSnapshotValid && traitSyncReady &&
                frameSetSpecialAbilitySprites.IsValid() && stateClass.IsValid()) {
                Obj* state = stateClass.CreateNewInstance();
                if (state) {
                    stateCtor[state]();
                    Obj* abilities = stateAbilities[state].Get();
                    if (abilities) {
                        auto add = BNM::Class(abilities).GetMethod("Add", 1).cast<void>();
                        if (add.IsValid()) {
                            for (unsigned i = 0; i < snapshot.traitCountBeforeTrigger; ++i)
                                add[abilities](snapshot.traitsBeforeTrigger[i]);
                            frameSetSpecialAbilitySprites[frame](state);
                            LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: PRETRIGGER_RESTORE id=%d source=%d view=%p count=%u (later grants stock-owned)",
                                id, returnSource, view, snapshot.traitCountBeforeTrigger);
                        }
                    }
                }
            }
            // Old off-board deltas cannot be replayed onto another view.
            if (auto* oldDelta = FindOffboardDelta(model, id, false)) *oldDelta = {};
            return;
        }
        const int gameplay = attack ? componentCalculateAttack[attack]() : -99999;
        auto* offboard = FindOffboardDelta(model, id, false);
        const bool alreadyApplied = offboard && offboard->compared &&
            offboard->lastComparedView == view;
        const bool safeEpoch = !fullReturn && offboard && offboard->events > 0 &&
            offboard->invalidFields == 0 && !alreadyApplied;
        const int pendingAttack = safeEpoch ? offboard->attack : 0;
        const int pendingHealth = safeEpoch ? offboard->health : 0;
        const int pendingCost = safeEpoch ? offboard->cost : 0;
        if (attack) CompareOffboardDelta(model, id, view, frame, gameplay, syncStage);
        // This view consumed the entire authoritative gap. Retire any older
        // off-board mutation epoch now, so a future CardView for the same
        // entity (e.g. after playing to a lane) cannot replay it again.
        if (fullReturn && offboard) *offboard = {};
        // A return correction REPLACES selective off-board replay for this
        // view: never apply both paths. Use the remaining gap, not the total
        // historical buff amount. A second ShowInHand cannot replay the ticket.
        const int before = frameCurrentAttack[frame].Get();
        const std::int64_t wideDelta = attack
            ? static_cast<std::int64_t>(gameplay) - before : 0;
        if (wideDelta < -10000 || wideDelta > 10000) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: INVALID_DELTA card=%d gameplay=%d frame=%d", id, gameplay, before);
            return;
        }
        const int delta = fullReturn ? static_cast<int>(wideDelta)
            : SelectOffboardFrameDelta(pendingAttack, static_cast<int>(wideDelta));
        if (attack && delta != 0) frameBuffAttack[frame](delta);
        if (fullReturn) LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: stage=%s id=%d source=%d view=%p attack=%d frameBefore=%d applied=%d frameAfter=%d (single full-gap reconciliation)",
            syncStage, id, returnSource, view, gameplay, before, delta, frameCurrentAttack[frame].Get());
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: APPLY stage=%s card=%d view=%p eligible=%d alreadyApplied=%d attackPending=%d attackGap=%lld attackApplied=%d healthPending=%d costPending=%d (stock owns all other changes)",
                 syncStage, id, view, int(safeEpoch), int(alreadyApplied), pendingAttack,
                 (long long)wideDelta, delta, pendingHealth, pendingCost);
        const int after = frameCurrentAttack[frame].Get();
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: %s card=%d view=%p gameplayAttack=%d frameBefore=%d appliedDelta=%d frameAfter=%d corrected=%d",
                 syncStage, id, view, gameplay, before, delta, after, int(after == gameplay));

        // Numerical safety checks still use the authoritative active component
        // values; only recorded off-board deltas may change the frame.
        // BuffHealth adjusts BOTH MaxHealth and CurrentHealth by the same
        // delta, preserving damage already taken. Never set Health directly.
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: NUMERIC_READY card=%d calc=%d healthFields=%d healthMethod=%d costFields=%d costMethod=%d",
                 id, int(calculateModifiableValue.IsValid()),
                 int(healthMaxHealth.IsValid() && healthCurrentDamage.IsValid() && frameMaxHealth.IsValid() && frameCurrentHealth.IsValid()),
                 int(frameBuffHealth.IsValid()),
                 int(sunCostValue.IsValid() && frameCurrentSunCost.IsValid()),
                 int(frameModifySunCost.IsValid()));
        Obj* health = TraceFindComponent(entity, "Health");
        if (health && healthMaxHealth.IsValid() && healthCurrentDamage.IsValid() &&
            calculateModifiableValue.IsValid() && frameMaxHealth.IsValid() &&
            frameCurrentHealth.IsValid() && frameBuffHealth.IsValid()) {
            Obj* value = healthMaxHealth[health].Get();
            if (value) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_CALC_ENTER card=%d modInt=%p", id, value);
                const int authoritativeMax = calculateModifiableValue(value);
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_CALC_EXIT card=%d max=%d", id, authoritativeMax);
                const int damage = healthCurrentDamage[health].Get();
                const int displayedMax = frameMaxHealth[frame].Get();
                const int displayedCurrent = frameCurrentHealth[frame].Get();
                const std::int64_t difference = static_cast<std::int64_t>(authoritativeMax) - displayedMax;
                // Don't risk healing/damaging unexpectedly if the frame's
                // health doesn't match the model's current-damage semantics.
                if (authoritativeMax >= 0 && damage >= 0 && damage <= authoritativeMax &&
                    difference >= -10000 && difference <= 10000 &&
                    displayedCurrent == displayedMax - damage) {
                    const int selective = fullReturn ? static_cast<int>(difference)
                        : SelectOffboardFrameDelta(pendingHealth, static_cast<int>(difference));
                    if (selective) frameBuffHealth[frame](selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: HEALTH_APPLY card=%d fullGap=%lld pending=%d applied=%d", id, (long long)difference, pendingHealth, selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH card=%d max=%d damage=%d beforeMax=%d afterMax=%d afterCurrent=%d",
                        id, authoritativeMax, damage, displayedMax,
                        frameMaxHealth[frame].Get(), frameCurrentHealth[frame].Get());
                } else {
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_SKIP card=%d max=%d damage=%d frameMax=%d frameCurrent=%d",
                        id, authoritativeMax, damage, displayedMax, displayedCurrent);
                }
            }
        }
        if (!health) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_MISSING card=%d", id);
        Obj* cost = TraceFindComponent(entity, "SunCost");
        if (cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid() &&
            frameCurrentSunCost.IsValid() && frameModifySunCost.IsValid()) {
            Obj* value = sunCostValue[cost].Get();
            if (value) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST_CALC_ENTER card=%d modInt=%p", id, value);
                int authoritative = calculateModifiableValue(value);
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST_CALC_EXIT card=%d cost=%d", id, authoritative);
                if (authoritative < 0) authoritative = 0;
                const int displayed = frameCurrentSunCost[frame].Get();
                const std::int64_t difference = static_cast<std::int64_t>(authoritative) - displayed;
                if (difference >= -10000 && difference <= 10000) {
                    const int selective = fullReturn ? static_cast<int>(difference)
                        : SelectOffboardFrameDelta(pendingCost, static_cast<int>(difference));
                    if (selective) frameModifySunCost[frame](selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: COST_APPLY card=%d fullGap=%lld pending=%d applied=%d", id, (long long)difference, pendingCost, selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST card=%d authoritative=%d before=%d after=%d",
                        id, authoritative, displayed, frameCurrentSunCost[frame].Get());
                }
            }
        }
        if (!cost) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST_MISSING card=%d", id);
        if (fullReturn) LOG_INFO("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: FINAL id=%d source=%d view=%p attack=%d healthMax=%d healthCurrent=%d sunCost=%d (gameplay unchanged)",
            id, returnSource, view, frameCurrentAttack[frame].Get(),
            frameMaxHealth.IsValid() ? frameMaxHealth[frame].Get() : -99999,
            frameCurrentHealth.IsValid() ? frameCurrentHealth[frame].Get() : -99999,
            frameCurrentSunCost.IsValid() ? frameCurrentSunCost[frame].Get() : -99999);
        // Trait rebuild uses precisely the hook that GrantAbilityPresentation
        // already extends; do NOT add/remove icons manually or touch star icon.
        if (traitSyncReady && frameSetSpecialAbilitySprites.IsValid()) {
            Obj* state = stateClass.CreateNewInstance();
            Obj* checker = checkerClass.CreateNewInstance();
            if (state && checker) {
                stateCtor[state]();
                checkerCtor[checker]();
                Obj* abilities = stateAbilities[state].Get();
                if (abilities) {
                    auto list = BNM::Class(abilities);
                    auto count = list.GetMethod("get_Count", 0).cast<int>();
                    const int beforeCount = count.IsValid() ? count[abilities]() : -1;
                    // Calls the GrantAbilityPresentation::AddSpecialAbilitiesHook,
                    // which incorporates active custom registry abilities.
                    checkerAddSpecialAbilities[checker](abilities, entity);
                    const int afterCount = count.IsValid() ? count[abilities]() : -1;
                    frameSetSpecialAbilitySprites[frame](state);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: TRAITS card=%d listBefore=%d listAfter=%d nativeRebuild=1 grantBridge=1",
                        id, beforeCount, afterCount);
                }
            }
        }
    }

    static void AddViewObjectTraceHook(void* self, int entityId, Obj* view, const void* method) {
        Obj* mapper = static_cast<Obj*>(self);
        Obj* model = mapper && viewMapperEntityModel.IsValid() ? viewMapperEntityModel[mapper].Get() : nullptr;
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: AddViewObject ENTER mapper=%p model=%p entity=%d view=%p type=%s",
                 self, model, entityId, view, TraceObjectType(view));
        // Do not assume every ViewObject is CardViewComponent.
        if (view && std::strcmp(TraceObjectType(view), "CardViewComponent") == 0)
            TraceFrameForCard(view, "AddViewObject BEFORE", entityId, model);
        originalAddViewObject(self, entityId, view, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: AddViewObject EXIT mapper=%p entity=%d view=%p",
                 self, entityId, view);
        if (view && std::strcmp(TraceObjectType(view), "CardViewComponent") == 0) {
            BindFrameView(view, model, entityId);
            TraceFrameForCard(view, "AddViewObject AFTER", entityId, model);
        }
    }

    static void ShowInHandTraceHook(void* self, const void* method) {
        Obj* view = static_cast<Obj*>(self);
        if (view) TraceFrameForCard(view, "ShowInHand BEFORE");
        originalShowInHand(self, method);
        // Stock hand presentation completes first. Never change gameplay Attack.
        if (view) ReconcileVisibleAttack(view);
        if (view) TraceFrameForCard(view, "ShowInHand AFTER");
    }



    inline std::uint64_t oneTimeTraceCount = 0;
    inline std::uint64_t groupTraceCount = 0;
    inline std::uint64_t coreTraceCount = 0;
    inline std::uint64_t addTraceCount = 0;

    inline void TraceBridgeCandidates(Obj* model, int sourceId, Obj* animation, const char* stage) {
        if (!model || sourceId < 0) return;
        for (const auto& e : pendingPresentations) {
            if (e.model == model && e.presentationSourceId == sourceId) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: %s candidate animation=%p model=%p source=%d eventCard=%d zone=%s sequence=%llu (peek only)",
                    stage, animation, model, sourceId, e.eventCardId, ZoneName(e.zone),
                    static_cast<unsigned long long>(e.sequence));
            }
        }
    }

    // Directly logs normal Trick/one-time presentation without changing it.
    static void OneTimeRecordTraceHook(void* self, Obj* record, const void* method) {
        const auto n = ++oneTimeTraceCount;
        int card = -1, hand = -1, location = -1;
        if (record) {
            if (oneTimeCardId.IsValid()) card = oneTimeCardId[record]();
            if (oneTimeHandId.IsValid()) hand = oneTimeHandId[record]();
            if (oneTimeLocationId.IsValid()) location = oneTimeLocationId[record]();
        }
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ONETIME ENTER seq=%llu record=%p card=%d hand=%d location=%d processor=%p",
            static_cast<unsigned long long>(n), record, card, hand, location, self);
        originalOneTimeRecord(self, record, method);
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ONETIME EXIT seq=%llu card=%d",
            static_cast<unsigned long long>(n), card);
    }

    static void TriggerRecordGroupTraceHook(void* self, Obj* records, int cardEntityId, Obj* record, const void* method) {
        const auto n = ++groupTraceCount;
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: GROUP ENTER seq=%llu processor=%p records=%p cardEntityId=%d startRecord=%p",
            static_cast<unsigned long long>(n), self, records, cardEntityId, record);
        originalTriggerRecordGroup(self, records, cardEntityId, record, method);
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: GROUP EXIT seq=%llu cardEntityId=%d",
            static_cast<unsigned long long>(n), cardEntityId);
    }

    // Trace the substantive four-parameter implementation, NOT the 8-byte
    // 3-parameter public wrapper at 0x1FCB630 (adjacent RVA hazard).
    static Obj* CreateAbilityCoreTraceHook(void* self, int groupId, int cardId, Obj* records, bool death, const void* method) {
        const auto n = ++coreTraceCount;
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: FACTORY ENTER seq=%llu factory=%p group=%d card=%d records=%p death=%d",
            static_cast<unsigned long long>(n), self, groupId, cardId, records, static_cast<int>(death));
        Obj* result = originalCreateAbilityCore(self, groupId, cardId, records, death, method);
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: FACTORY EXIT seq=%llu animation=%p group=%d card=%d",
            static_cast<unsigned long long>(n), result, groupId, cardId);
        return result;
    }

    inline OffBoardZone GetOffBoardZone(Obj* model, int entityIdValue);

    // RecordKey in this build: SourceId=0, EffectEntityId=2, AbilityGroupId=3,
    // CardId=6. Start records do not guarantee all these keys are populated.
    inline bool TryTraceRecordInt(Obj* record, int key, int& value) {
        if (!record || !traceRecordGetDictionary.IsValid() ||
            !traceRecordGetInt.IsValid()) return false;
        Obj* dictionary = traceRecordGetDictionary[record]();
        if (!dictionary) return false;
        auto contains = BNM::Class(dictionary).GetMethod("ContainsKey", 1).cast<bool>();
        if (!contains.IsValid() || !contains[dictionary](key)) return false;
        value = traceRecordGetInt[record](key);
        return true;
    }

    inline const char* TraceObjectType(Obj* object) {
        if (!object) return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "unknown";
    }

    inline Obj* TraceFindComponent(Obj* entity, const char* name) {
        if (!entity || !name || !traceEntityComponents.IsValid()) return nullptr;
        Obj* listObject = traceEntityComponents[entity].Get();
        if (!listObject) return nullptr;
        auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
        const int count = list->GetSize();
        if (count < 0 || count > 256 || !list->items) return nullptr;
        Obj** data = list->GetData();
        if (!data) return nullptr;
        for (int i = 0; i < count; ++i)
            if (data[i] && std::strcmp(TraceObjectType(data[i]), name) == 0) return data[i];
        return nullptr;
    }

    // CompositeAll/Any can nest arbitrarily. Bound traversal to avoid cycles,
    // malformed serialization, or excessive logging on a production match.
    inline void TraceQueryTree(Obj* query, int depth, int& budget) {
        if (!query || depth > 10 || budget-- <= 0) return;
        const char* kind = TraceObjectType(query);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDZONE: QUERY depth=%d type=%s ptr=%p",
            depth, kind, query);
        if (std::strcmp(kind, "CompositeAllQuery") != 0 &&
            std::strcmp(kind, "CompositeAnyQuery") != 0) return;
        if (!traceCompositeQueries.IsValid()) return;
        Obj* childrenObject = traceCompositeQueries[query].Get();
        if (!childrenObject) return;
        auto* children = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(childrenObject);
        const int count = children->GetSize();
        if (count < 0 || count > 128 || !children->items) return;
        Obj** data = children->GetData();
        if (!data) return;
        for (int i = 0; i < count && budget > 0; ++i)
            TraceQueryTree(data[i], depth + 1, budget);
    }

    // Materialized deck membership is determined from the OrderedCardPool,
    // independently of DeckElement presence (which may be absent).
    inline void TraceOwnerLocation(Obj* model, Obj* ownerEntity, int ownerId, int effectId) {
        if (!model || !ownerEntity || ownerId < 0) return;
        const Obj* card = TraceFindComponent(ownerEntity, "Card");
        const int guid = card && traceCardGuid.IsValid()
            ? traceCardGuid[const_cast<Obj*>(card)].Get() : -1;
        const bool deckMarker = TraceFindComponent(ownerEntity, "DeckElement") != nullptr;
        const bool discardMarker = TraceFindComponent(ownerEntity, "DiscardPileElement") != nullptr;
        const bool handMarker = TraceFindComponent(ownerEntity, "HandElement") != nullptr;
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDZONE: OWNER card=%d guid=%d effect=%d DeckElement=%d DiscardPileElement=%d HandElement=%d",
            ownerId, guid, effectId, int(deckMarker), int(discardMarker), int(handMarker));

        Obj* effectEntity = entityExists[model](effectId) ? getEntityById[model](effectId) : nullptr;
        Obj* self = TraceFindComponent(effectEntity, "SelfEntityFilter");
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDZONE: SELF effect=%d present=%d", effectId, int(self != nullptr));
        if (self && traceSelfFilterQuery.IsValid()) {
            int budget = 64;
            TraceQueryTree(traceSelfFilterQuery[self].Get(), 0, budget);
        }

        if (!traceModelEntities.IsValid() || !traceOrderedPoolEntries.IsValid() ||
            !tracePoolEntryEntityId.IsValid()) return;
        Obj* entitiesObject = traceModelEntities[model].Get();
        if (!entitiesObject) return;
        auto* entities = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(entitiesObject);
        const int count = entities->GetSize();
        if (count < 0 || count > 32768 || !entities->items) return;
        Obj** data = entities->GetData();
        if (!data) return;
        int pools = 0, matches = 0;
        for (int i = 0; i < count; ++i) {
            Obj* pool = TraceFindComponent(data[i], "OrderedCardPool");
            if (!pool) continue;
            ++pools;
            Obj* entriesObject = traceOrderedPoolEntries[pool].Get();
            if (!entriesObject) continue;
            auto* entries = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(entriesObject);
            const int n = entries->GetSize();
            if (n < 0 || n > 4096 || !entries->items) continue;
            Obj** entryData = entries->GetData();
            if (!entryData) continue;
            for (int j = 0; j < n; ++j) {
                Obj* entry = entryData[j];
                if (entry && tracePoolEntryEntityId[entry].Get() == ownerId) {
                    ++matches;
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDZONE: DECKMATCH card=%d poolOwnerEntity=%p poolEntryIndex=%d entryGuid=%d",
                        ownerId, data[i], j,
                        tracePoolEntryGuid.IsValid() ? tracePoolEntryGuid[entry].Get() : -1);
                }
            }
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDZONE: POOLSCAN card=%d pools=%d matches=%d", ownerId, pools, matches);
    }

    // The self filter declares which off-board zones this precise ability may
    // fire from. This is NOT a substitute for stock query validation.
    // Deliberately ignore TriggerSourceFilter and TriggerTargetFilter.
    inline bool SelfQueryDeclaresZone(Obj* query, OffBoardZone zone,
                                     int depth, int& budget) {
        if (!query || depth > 10 || budget-- <= 0) return false;
        const char* kind = TraceObjectType(query);
        if (zone == OffBoardZone::Deck &&
            std::strcmp(kind, "InDeckQuery") == 0) return true;
        if (zone == OffBoardZone::DiscardPile &&
            std::strcmp(kind, "InDiscardPileQuery") == 0) return true;
        if (std::strcmp(kind, "CompositeAllQuery") != 0 &&
            std::strcmp(kind, "CompositeAnyQuery") != 0) return false;
        if (!traceCompositeQueries.IsValid()) return false;
        Obj* listObject = traceCompositeQueries[query].Get();
        if (!listObject) return false;
        auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
        const int count = list->GetSize();
        if (count < 0 || count > 128 || !list->items) return false;
        Obj** entries = list->GetData();
        if (!entries) return false;
        for (int i = 0; i < count && budget > 0; ++i) {
            if (SelfQueryDeclaresZone(entries[i], zone, depth + 1, budget))
                return true;
        }
        return false;
    }

    inline bool IsInMaterializedDeckPool(Obj* model, int cardId) {
        if (!model || cardId < 0 || !traceModelEntities.IsValid() ||
            !traceOrderedPoolEntries.IsValid() ||
            !tracePoolEntryEntityId.IsValid()) return false;
        Obj* listObject = traceModelEntities[model].Get();
        if (!listObject) return false;
        auto* entities = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
        const int count = entities->GetSize();
        if (count < 0 || count > 32768 || !entities->items) return false;
        Obj** data = entities->GetData();
        if (!data) return false;
        for (int i = 0; i < count; ++i) {
            Obj* pool = TraceFindComponent(data[i], "OrderedCardPool");
            if (!pool) continue;
            Obj* entriesObject = traceOrderedPoolEntries[pool].Get();
            if (!entriesObject) continue;
            auto* entries = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(entriesObject);
            const int n = entries->GetSize();
            if (n < 0 || n > 4096 || !entries->items) continue;
            Obj** entryData = entries->GetData();
            if (!entryData) continue;
            for (int j = 0; j < n; ++j)
                if (entryData[j] && tracePoolEntryEntityId[entryData[j]].Get() == cardId)
                    return true;
        }
        return false;
    }

    // TARGETBOUNDARY: diagnostic-only shared source/target inspection.
    // Do not change effect records, target lists, event actions or animation results.
    inline OffBoardZone TraceVisualZone(Obj* model, int id) {
        if (!model || id < 0 || !entityExists.IsValid() ||
            !getEntityById.IsValid() || !entityExists[model](id))
            return OffBoardZone::None;
        Obj* entity = getEntityById[model](id);
        if (!entity) return OffBoardZone::None;
        if (TraceFindComponent(entity, "DiscardPileElement"))
            return OffBoardZone::DiscardPile;
        if (IsInMaterializedDeckPool(model, id))
            return OffBoardZone::Deck;
        return OffBoardZone::None;
    }

    inline void TraceVisualActionTargets(Obj* action, const char* stage) {
        if (!action || !traceVisualSourceId.IsValid() ||
            !traceVisualTargetIds.IsValid() || !traceVisualMapper.IsValid() ||
            !viewMapperEntityModel.IsValid()) return;
        Obj* mapper = traceVisualMapper[action].Get();
        Obj* model = mapper ? viewMapperEntityModel[mapper].Get() : nullptr;
        const int source = traceVisualSourceId[action].Get();
        const auto sourceZone = TraceVisualZone(model, source);
        Obj* listObject = traceVisualTargetIds[action].Get();
        int count = -1;
        int visible = 0, deck = 0, discard = 0;
        if (listObject) {
            auto* list = reinterpret_cast<BNM::Structures::Mono::List<int>*>(listObject);
            count = list->GetSize();
            if (count >= 0 && count <= 128 && list->items) {
                int* ids = list->GetData();
                if (ids) for (int i = 0; i < count; ++i) {
                    const OffBoardZone zone = TraceVisualZone(model, ids[i]);
                    if (zone == OffBoardZone::Deck) ++deck;
                    else if (zone == OffBoardZone::DiscardPile) ++discard;
                    else ++visible;
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: %s target[%d]=%d zone=%s model=%p",
                             stage, i, ids[i], ZoneName(zone), model);
                }
            }
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: %s action=%p type=%s model=%p mapper=%p source=%d sourceZone=%s targets=%d visible=%d deck=%d discard=%d scoped=%d",
                 stage, action, TraceObjectType(action), model, mapper, source,
                 ZoneName(sourceZone), count, visible, deck, discard,
                 int(IsZoneNullTimelineActive()));
    }

    // Capture the zone while processing the start record: a card might move
    // before OnStarted runs. No card/effect/record data is modified.
    inline AutoOffBoardPresentation ClassifyStockStartRecord(
        void* processor, Obj* record, int cardId) {
        AutoOffBoardPresentation result{};
        if (!record || cardId < 0 || !traceProcessorEntityModel.IsValid() ||
            !entityExists.IsValid() || !getEntityById.IsValid() ||
            !traceEffectEntityGetSource.IsValid() ||
            !traceSelfFilterQuery.IsValid()) return result;
        int effectId = -1;
        if (!TryTraceRecordInt(record, 2, effectId) || effectId < 0) return result;
        Obj* model = processor ? traceProcessorEntityModel[static_cast<Obj*>(processor)].Get() : nullptr;
        if (!model || !entityExists[model](effectId)) return result;
        Obj* effectEntity = getEntityById[model](effectId);
        if (!effectEntity || !TraceFindComponent(effectEntity, "EffectEntity")) return result;
        int ownerId = traceEffectEntityGetSource[static_cast<Obj*>(nullptr)](effectEntity);
        if (ownerId != cardId || !entityExists[model](ownerId)) return result;
        Obj* owner = getEntityById[model](ownerId);
        Obj* self = TraceFindComponent(effectEntity, "SelfEntityFilter");
        if (!owner || !self) return result;
        Obj* query = traceSelfFilterQuery[self].Get();
        if (!query) return result;
        // The persistent deck has no DeckElement, so check OrderedCardPool.
        const bool deck = IsInMaterializedDeckPool(model, ownerId);
        const bool discard = TraceFindComponent(owner, "DiscardPileElement") != nullptr;
        OffBoardZone zone = OffBoardZone::None;
        int budget = 64;
        if (deck && !discard && !TraceFindComponent(owner, "HandElement") &&
            SelfQueryDeclaresZone(query, OffBoardZone::Deck, 0, budget)) {
            zone = OffBoardZone::Deck;
        } else if (discard && !deck) {
            budget = 64;
            if (SelfQueryDeclaresZone(query, OffBoardZone::DiscardPile, 0, budget))
                zone = OffBoardZone::DiscardPile;
        } else if (!discard && !deck &&
                   TraceFindComponent(owner, "LaneElement") &&
                   !TraceFindComponent(owner, "HandElement") &&
                   IsDiscardToLaneOrigin(model, ownerId, effectId)) {
            // The start record belongs to the exact custom effect which JUST
            // moved this same card from discard to a lane. The effect's
            // discard self-filter remains attached to its historical record.
            // Keep its stale particle/target visuals offboard, while allowing
            // CreateCardInLaneRecord to run on the normal presentation path.
            budget = 64;
            if (SelfQueryDeclaresZone(query, OffBoardZone::DiscardPile, 0, budget)) {
                zone = OffBoardZone::DiscardPile;
                CUSTOMCOMPONENTS_ZTAP_TRACE(
                    "CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-BRIDGE: stale discard-scoped start owner=%d effect=%d nowInLane=1 (exact effect provenance, presentation only)",
                    ownerId, effectId
                );
            }
        } else if (!discard && !deck && TraceFindComponent(owner, "HandElement")) {
            // DISCARD->HAND DIAGNOSTIC ONLY:
            // A discard-scoped effect can already have generated a Start record
            // before its owner is moved into hand. The frontend builds that
            // record later, when the live card has HandElement instead of
            // DiscardPileElement. Without this narrow fallback, the source
            // visual is passed to stock OnStarted even if it has no CardView.
            // Do not use this as evidence of current gameplay discard membership.
            budget = 64;
            if (SelfQueryDeclaresZone(query, OffBoardZone::DiscardPile, 0, budget)) {
                zone = OffBoardZone::DiscardPile;
                CUSTOMCOMPONENTS_ZTAP_TRACE(
                    "CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-HAND-BRIDGE: stale discard-scoped start owner=%d effect=%d nowInHand=1 (diagnostic presentation-only)",
                    ownerId, effectId
                );
            }
        }
        if (zone != OffBoardZone::None) {
            result.model = model;
            result.sourceId = ownerId;
            result.effectEntityId = effectId;
            result.zone = zone;
        }
        return result;
    }

    inline void RememberAutoOffBoardPresentation(AutoOffBoardPresentation info,
                                                   Obj* animation) {
        if (!animation || !info.model || info.zone == OffBoardZone::None) return;
        // No eviction: an overflow must fail closed, never reroute a different animation.
        for (auto& entry : autoOffBoardPresentations) {
            if (!entry.animation || entry.animation == animation) {
                info.animation = animation;
                entry = info;
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/AUTOZONE: REGISTER animation=%p model=%p source=%d effect=%d zone=%s",
                         animation, info.model, info.sourceId, info.effectEntityId, ZoneName(info.zone));
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/AUTOZONE: table full; stock animation retained animation=%p", animation);
    }

    inline bool TakeAutoOffBoardPresentation(Obj* animation, Obj* model, int source,
                                              OffBoardZone& outZone) {
        if (!animation || !model) return false;
        for (auto& entry : autoOffBoardPresentations) {
            if (entry.animation != animation) continue;
            AutoOffBoardPresentation saved = entry;
            entry = {};
            if (saved.model != model || saved.sourceId != source) {
                LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/AUTOZONE: identity mismatch animation=%p source=%d expected=%d; stock retained",
                        animation, source, saved.sourceId);
                return false;
            }
            outZone = saved.zone;
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/AUTOZONE: CONSUME animation=%p source=%d effect=%d zone=%s",
                     animation, source, saved.effectEntityId, ZoneName(saved.zone));
            return outZone != OffBoardZone::None;
        }
        return false;
    }

    inline void TraceStartRecordIdentity(void* processor, Obj* record, int cardId) {
        if (!record) return;
        int source = -1, effect = -1, group = -1, recordCard = -1;
        const bool hasSource = TryTraceRecordInt(record, 0, source);
        const bool hasEffect = TryTraceRecordInt(record, 2, effect);
        const bool hasGroup = TryTraceRecordInt(record, 3, group);
        const bool hasCard = TryTraceRecordInt(record, 6, recordCard);
        Obj* model = processor && traceProcessorEntityModel.IsValid()
            ? traceProcessorEntityModel[static_cast<Obj*>(processor)].Get() : nullptr;
        int owner = -1;
        OffBoardZone ownerZone = OffBoardZone::None;
        if (hasEffect && effect >= 0 && model && entityExists.IsValid() &&
            getEntityById.IsValid() && traceEffectEntityGetSource.IsValid() &&
            entityExists[model](effect)) {
            Obj* effectEntity = getEntityById[model](effect);
            if (effectEntity) {
                // GetSource is valid for effect entities only. Check the
                // marker first instead of calling on arbitrary model entities.
                auto effectComponent = BNM::Class("PvZCards.Engine.Components", "EffectEntity");
                if (effectComponent.IsValid() && entityHasComponent.IsValid() &&
                    entityHasComponent[effectEntity](reinterpret_cast<Obj*>(effectComponent.GetMonoType()))) {
                    owner = traceEffectEntityGetSource[static_cast<Obj*>(nullptr)](effectEntity);
                    ownerZone = GetOffBoardZone(model, owner);
                    if (entityExists[model](owner))
                        TraceOwnerLocation(model, getEntityById[model](owner), owner, effect);
                }
            }
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDIDENTITY: start=%p processor=%p model=%p argumentCard=%d keySource=%d presentSource=%d keyEffectEntity=%d presentEffect=%d keyGroup=%d presentGroup=%d keyCard=%d presentCard=%d owner=%d ownerZone=%s",
            record, processor, model, cardId, source, static_cast<int>(hasSource),
            effect, static_cast<int>(hasEffect), group, static_cast<int>(hasGroup),
            recordCard, static_cast<int>(hasCard), owner, ZoneName(ownerZone));
    }

    static Obj* AddAbilityRecordTraceHook(void* self, Obj* record, int cardId, Obj* records, const void* method) {
        const auto n = ++addTraceCount;
        if (n <= 300) TraceStartRecordIdentity(self, record, cardId);
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ADDABILITY ENTER seq=%llu processor=%p record=%p card=%d records=%p",
            static_cast<unsigned long long>(n), self, record, cardId, records);
        AutoOffBoardPresentation autoZone = ClassifyStockStartRecord(self, record, cardId);
        Obj* result = originalAddAbilityRecord(self, record, cardId, records, method);
        RememberAutoOffBoardPresentation(autoZone, result);
        if (n <= 300) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ADDABILITY EXIT seq=%llu card=%d animation=%p group=%d",
            static_cast<unsigned long long>(n), cardId, result,
            (result && traceAbilityGroupId.IsValid()) ? traceAbilityGroupId[result].Get() : -1);
        return result;
    }

    inline OffBoardZone GetOffBoardZone(Obj* model, int entityIdValue) {
        if (!model || entityIdValue < 0 ||
            !entityExists.IsValid() ||
            !getEntityById.IsValid() ||
            !entityHasComponent.IsValid() ||
            !deckElementType || !discardPileElementType)
            return OffBoardZone::None;

        if (!entityExists[model](entityIdValue))
            return OffBoardZone::None;

        Obj* entity = getEntityById[model](entityIdValue);
        if (!entity)
            return OffBoardZone::None;

        if (entityHasComponent[entity](discardPileElementType))
            return OffBoardZone::DiscardPile;

        if (entityHasComponent[entity](deckElementType))
            return OffBoardZone::Deck;

        return OffBoardZone::None;
    }

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

    // =====================================================================
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
        CUSTOMCOMPONENTS_ZTAP_TRACE(
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
        CUSTOMCOMPONENTS_ZTAP_TRACE(
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
        if (!model || !IsDiscardToLaneOrigin(model, id) ||
            !entityExists.IsValid() || !getEntityById.IsValid() ||
            !entityExists[model](id)) return;
        Obj* entity = getEntityById[model](id);
        if (!entity || !TraceFindComponent(entity, "LaneElement") ||
            TraceFindComponent(entity, "DiscardPileElement")) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-SYNC: SKIP card=%d model=%p reason=not-in-lane", id, model);
            return;
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-SYNC: BEGIN card=%d model=%p view=%p", id, model, view);
        RememberFrameSync(model, id);
        ReconcileVisibleAttack(view, "RETURN_TO_LANE");
        LaneTraceCardFrame(view, "RETURN-LANE FRAME AFTER SYNC");
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-LANE-SYNC: END card=%d view=%p", id, view);
    }

    static Obj* LaneCreateCardEventTraceHook(void* self, Obj* record, const void* method) {
        const auto n = ++laneTraceEventCount;
        int recordCardId = -1;
        if (record && laneRecordGetCard.IsValid() && laneCardDataEntityId.IsValid()) {
            Obj* data = laneRecordGetCard[record]();
            if (data) recordCardId = laneCardDataEntityId[data].Get();
        }
        if (n <= 200) CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: CREATE_EVENT ENTER seq=%llu record=%p card=%d laneTarget=%d",
            (unsigned long long)n, record, recordCardId,
            record && laneRecordTargetId.IsValid() ? laneRecordTargetId[record].Get() : -1);
        Obj* action = originalLaneCreateCardEvent(self, record, method);
        if (n <= 200) CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: CREATE_EVENT EXIT seq=%llu action=%p type=%s actionCard=%d order=%d faceDown=%d",
            (unsigned long long)n, action, TraceObjectType(action),
            action && laneEventCardId.IsValid() ? laneEventCardId[action].Get() : -1,
            action && laneEventOrder.IsValid() ? laneEventOrder[action].Get() : -1,
            action && laneEventFaceDown.IsValid() ? int(laneEventFaceDown[action].Get()) : -1);
        return action;
    }

    static void LanePreloadTraceHook(void* self, const void* method) {
        const auto n = ++laneTracePreloadCount;
        if (n <= 200) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: PRELOAD ENTER seq=%llu action=%p card=%d",
            (unsigned long long)n, self,
            self && laneEventCardId.IsValid() ? laneEventCardId[static_cast<Obj*>(self)].Get() : -1);
        originalLanePreload(self, method);
        if (n <= 200) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: PRELOAD EXIT seq=%llu action=%p",
            (unsigned long long)n, self);
    }

    static void LaneCreateStartedTraceHook(void* self, const void* method) {
        const auto n = ++laneTraceStartCount;
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "CREATE_ON_STARTED BEFORE");
        originalLaneCreateStarted(self, method);
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "CREATE_ON_STARTED AFTER");
    }

    static void LaneShowCardTraceHook(void* self, const void* method) {
        const auto n = ++laneTraceShowCount;
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "SHOW_CARD BEFORE");
        originalLaneShowCard(self, method);
        // At this point the stock code has loaded, placed and initialized the
        // card frame. Fix3 is idempotent and runs only for a logged discard
        // -> lane origin; do not make it contingent on diagnostic log limits.
        if (laneAnimationCardView.IsValid())
            ReconcileReturnedLaneView(laneAnimationCardView[static_cast<Obj*>(self)].Get());
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
        if (n <= 200) LaneTraceAnimation(static_cast<Obj*>(self), "CREATE_COMPLETED AFTER");
    }

    static void LaneAddToLaneTraceHook(void* self, Obj* card, int order, const void* method) {
        const auto n = ++laneTraceAddCount;
        if (n <= 200) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: ADD_TO_LANE ENTER seq=%llu laneView=%p cardView=%p type=%s order=%d",
                (unsigned long long)n, self, card, TraceObjectType(card), order);
            LaneTraceCardFrame(card, "ADD_TO_LANE FRAME BEFORE");
        }
        originalLaneAddToLane(self, card, order, method);
        if (n <= 200) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: ADD_TO_LANE EXIT seq=%llu laneView=%p cardView=%p order=%d",
                (unsigned long long)n, self, card, order);
            LaneTraceCardFrame(card, "ADD_TO_LANE FRAME AFTER");
        }
    }

    static void LanePlayedToBoardTraceHook(void* self, Obj* state, const void* method) {
        const auto n = ++laneTraceBoardCount;
        if (n <= 200) LaneTraceCardFrame(static_cast<Obj*>(self), "PLAYED_TO_BOARD BEFORE");
        originalLanePlayedToBoard(self, state, method);
        if (n <= 200) LaneTraceCardFrame(static_cast<Obj*>(self), "PLAYED_TO_BOARD AFTER");
    }

    // Read-only list inspection, including the queue's FrontAnimations list.
    // Never mutate List<T> or animation state in diagnostic hooks.
    struct QueueListSnapshot {
        int count = -1;
        bool containsTarget = false;
    };

    inline QueueListSnapshot InspectQueueList(Obj* queue, BNM::Field<Obj*>& field) {
        QueueListSnapshot snapshot{};
        if (!queue || !field.IsValid()) return snapshot;
        Obj* listObject = field[queue].Get();
        if (!listObject) return snapshot;
        auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
        snapshot.count = list->GetSize();
        if (snapshot.count < 0 || snapshot.count > 4096 || !list->items)
            return snapshot;
        Obj** entries = list->GetData();
        if (!entries) return snapshot;
        for (int i = 0; i < snapshot.count; ++i) {
            if (entries[i] == lifecycleTargetAnimation) {
                snapshot.containsTarget = true;
                break;
            }
        }
        return snapshot;
    }

    inline void LogQueueLifecycle(Obj* queue, const char* phase, bool force = false) {
        if (!queue || queue != lifecycleTargetQueue || !lifecycleTargetAnimation)
            return;
        const auto normal = InspectQueueList(queue, queueAnimations);
        const auto front = InspectQueueList(queue, queueFrontAnimations);
        const auto signature =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(normal.count)) << 32) ^
            (static_cast<std::uint64_t>(static_cast<std::uint16_t>(front.count)) << 16) ^
            (normal.containsTarget ? 2ull : 0ull) ^
            (front.containsTarget ? 1ull : 0ull);
        if (!force && signature == lifecycleQueueLastSignature &&
            lifecycleQueueUpdates > 12)
            return;
        lifecycleQueueLastSignature = signature;
        const int started = effectIsStarted.IsValid()
            ? static_cast<int>(effectIsStarted[lifecycleTargetAnimation].Get()) : -1;
        const int cancelled = effectIsCancelled.IsValid()
            ? static_cast<int>(effectIsCancelled[lifecycleTargetAnimation].Get()) : -1;
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: %s queue=%p update=%llu normal=%d front=%d target=%p inNormal=%d inFront=%d started=%d cancelled=%d",
            phase, queue, static_cast<unsigned long long>(lifecycleQueueUpdates),
            normal.count, front.count, lifecycleTargetAnimation,
            static_cast<int>(normal.containsTarget), static_cast<int>(front.containsTarget),
            started, cancelled);
    }

    static void CheckForCompletionHook(void* self, const void* method) {
        if (self != lifecycleTargetAnimation) {
            originalCheckForCompletion(self, method);
            return;
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: CheckForCompletion ENTER animation=%p", self);
        originalCheckForCompletion(self, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: CheckForCompletion EXIT animation=%p", self);
        LogQueueLifecycle(lifecycleTargetQueue, "AFTER completion check", true);
    }

    static void QueueUpdateHook(void* self, const void* method) {
        if (self != lifecycleTargetQueue || !lifecycleTargetAnimation) {
            originalQueueUpdate(self, method);
            return;
        }
        ++lifecycleQueueUpdates;
        LogQueueLifecycle(static_cast<Obj*>(self), "UPDATE BEFORE");
        originalQueueUpdate(self, method);
        LogQueueLifecycle(static_cast<Obj*>(self), "UPDATE AFTER");
    }

    static bool QueueIsEmptyHook(void* self, const void* method) {
        const bool result = originalQueueIsEmpty(self, method);
        if (self == lifecycleTargetQueue && lifecycleTargetAnimation &&
            (lifecycleQueueUpdates <= 12)) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: IsEmpty queue=%p result=%d", self, static_cast<int>(result));
        }
        return result;
    }

    static bool TriggeredCanYieldHook(void* self, const void* method) {
        const bool result = originalTriggeredCanYield(self, method);
        if (self == lifecycleTargetAnimation &&
            (lifecycleQueueUpdates <= 12)) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: CanYield animation=%p result=%d", self, static_cast<int>(result));
        }
        return result;
    }

    static void TriggeredAbilityOnStartedHook(
        void* self, const void* method
    ) {
        Obj* animation = static_cast<Obj*>(self);
        ++presentationStartTraceCount;

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/TRACE: TriggeredAbility.OnStarted ENTER animation=%p count=%llu",
            animation,
            static_cast<unsigned long long>(presentationStartTraceCount)
        );

        int presentationSourceId = -1;
        int zoneCardId = -1;
        OffBoardZone zone = OffBoardZone::None;

        if (!IsDeckOrDiscardSource(
            animation, &presentationSourceId, &zoneCardId, &zone))
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
            !tracePoolEntryEntityId.IsValid()) return result;
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
        const TargetVisualDecision policy = (!damageAction && ownedByOffBoardAbility)
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

    // Read-only command inventory. Uses the BNM List<T> layout; no activation,
    // mutation, or animation creation is performed by the tracer.
    inline void TraceEventCommands(void* container, const char* phase) {
        if (!container || !commandEventCommands.IsValid()) return;
        Obj* commandsObj = commandEventCommands[static_cast<Obj*>(container)].Get();
        if (!commandsObj) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s container=%p EventCommands=null", phase, container);
            return;
        }
        auto* commands = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(commandsObj);
        const int count = commands->GetSize();
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s container=%p count=%d", phase, container, count);
        if (count < 0 || count > 4096 || !commands->items) return;
        Obj** entries = commands->GetData();
        if (!entries) return;
        for (int i = 0; i < count && i < 64; ++i) {
            Obj* action = entries[i];
            if (!action) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s index=%d action=null", phase, i);
                continue;
            }
            auto klass = BNM::Class(action);
            auto activated = klass.GetMethod("get_HasBeenActivated", 0).cast<bool>();
            auto eventType = klass.GetMethod("get_EventType", 0).cast<int>();
            const int activeValue = activated.IsValid() ? static_cast<int>(activated[action]()) : -1;
            const int typeValue = eventType.IsValid() ? eventType[action]() : -1;
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s index=%d action=%p klass=%p activated=%d eventType=%d", phase, i, action, action->klass, activeValue, typeValue);
        }
    }

    static void ExecuteRemainingCommandsHook(
        void* self, const void* method
    ) {
        if (IsZoneNullTimelineActive()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: STOCK ENTER container=%p", self);
            TraceEventCommands(self, "BEFORE");
            originalExecuteRemainingCommands(self, method);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: STOCK EXIT container=%p", self);
            TraceEventCommands(self, "AFTER");
            return;
        }
        originalExecuteRemainingCommands(self, method);
    }

    static void HandleCommandLoadCompleteHook(void* self, const void* method) {
        Obj* animation = static_cast<Obj*>(self);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/WAITLOAD: HandleCommandLoadComplete ENTER animation=%p", animation);
        LogWaitForLoad(animation, "BEFORE load callback");
        PendingLoadScope pending{};
        if (ConsumeLoadScope(animation, pending)) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DELAYEDSCOPE: ENTER animation=%p mapper=%p source=%d", animation, pending.mapper, pending.sourceId);
            ZoneNullTimelineScope scope(pending.mapper, pending.sourceId);
            originalHandleCommandLoadComplete(self, method);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DELAYEDSCOPE: EXIT animation=%p", animation);
        } else {
            originalHandleCommandLoadComplete(self, method);
        }
        LogWaitForLoad(animation, "AFTER load callback");
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/WAITLOAD: HandleCommandLoadComplete EXIT animation=%p", animation);
    }

    static void AddParticleEffectAnimationHook(
        void* self, Obj* action, Obj* data, const void* method
    ) {
        // Never suppress this entire registration stage: it can contain
        // legitimate Trick/target particles even within an off-board scope.
        if (IsZoneNullTimelineActive()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/PARTICLE: STOCK FORWARD self=%p action=%p data=%p",
                self, action, data);
        }
        originalAddParticleEffectAnimation(self, action, data, method);
    }


    // PHASE 1: read-only off-board gameplay effect audit.
    // No frontend modifications, no persistent snapshot state, no gameplay writes.
    // The six hooks observe effective stats before and after a stock handler.
    inline BNM::Field<int> auditEffectTarget;
    inline BNM::Field<int> auditEffectSource;
    inline BNM::Field<int> auditEffectEntity;
    inline BNM::Field<Obj*> auditSystemModel[6];
    inline BNM::Field<int> auditGrantType;
    inline BNM::Field<int> auditRemoveGrantType;
    inline std::uint64_t auditSeen[6]{};
    inline std::uint64_t auditReported[6]{};
    inline constexpr std::uint64_t kAuditLogLimit = 240;
    using AuditProcessFn = void (*)(void*, Obj*, const void*);
    inline AuditProcessFn originalAuditProcess[6]{};

    struct AuditStats {
        int attack = -99999;
        int maxHealth = -99999;
        int damage = -99999;
        int cost = -99999;
        int componentCount = -1;
        std::uint64_t componentSignature = 1469598103934665603ull;
    };

    inline AuditStats CaptureAuditStats(Obj* entity) {
        AuditStats stats{};
        if (!entity) return stats;
        if (Obj* attack = TraceFindComponent(entity, "Attack");
            attack && componentCalculateAttack.IsValid())
            stats.attack = componentCalculateAttack[attack]();
        if (Obj* health = TraceFindComponent(entity, "Health"); health) {
            if (healthCurrentDamage.IsValid()) stats.damage = healthCurrentDamage[health].Get();
            if (healthMaxHealth.IsValid() && calculateModifiableValue.IsValid()) {
                Obj* value = healthMaxHealth[health].Get();
                if (value) stats.maxHealth = calculateModifiableValue(value);
            }
        }
        if (Obj* cost = TraceFindComponent(entity, "SunCost");
            cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid()) {
            Obj* value = sunCostValue[cost].Get();
            if (value) stats.cost = calculateModifiableValue(value);
        }
        // Diagnostic component-type fingerprint. This detects added/removed
        // components, but not ability-value changes within an existing component.
        if (traceEntityComponents.IsValid()) {
            Obj* listObject = traceEntityComponents[entity].Get();
            if (listObject) {
                auto* components = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
                int count = components->GetSize();
                if (count >= 0 && count <= 256 && components->items) {
                    stats.componentCount = count;
                    Obj** elements = components->GetData();
                    if (elements) for (int i = 0; i < count; ++i) {
                        const char* name = TraceObjectType(elements[i]);
                        for (const unsigned char* c = reinterpret_cast<const unsigned char*>(name); *c; ++c) {
                            stats.componentSignature ^= *c;
                            stats.componentSignature *= 1099511628211ull;
                        }
                        stats.componentSignature ^= 0xff;
                        stats.componentSignature *= 1099511628211ull;
                    }
                }
            }
        }
        return stats;
    }

    // Existing hook owners (DiscardedSystem and GrantAbility) call this
    // around their OWN handler. No second inline hook is installed.
    template<class CallOriginal>
    inline void AuditOwnedEffect(int index, void* self, Obj* effect,
                                 CallOriginal&& invoke) {
        const auto seen = ++auditSeen[index];
        Obj* model = self && auditSystemModel[index].IsValid()
            ? auditSystemModel[index][static_cast<Obj*>(self)].Get() : nullptr;
        const int target = effect && auditEffectTarget.IsValid()
            ? auditEffectTarget[effect].Get() : -1;
        const int source = effect && auditEffectSource.IsValid()
            ? auditEffectSource[effect].Get() : -1;
        const int effectEntity = effect && auditEffectEntity.IsValid()
            ? auditEffectEntity[effect].Get() : -1;
        Obj* card = model && target >= 0 && entityExists.IsValid() && getEntityById.IsValid()
            && entityExists[model](target) ? getEntityById[model](target) : nullptr;
        // Require a card and an unambiguous current Deck/Discard membership.
        const bool isCard = card && TraceFindComponent(card, "Card");
        const bool hand = isCard && TraceFindComponent(card, "HandElement");
        const bool lane = isCard && TraceFindComponent(card, "LaneElement");
        const bool discard = isCard && TraceFindComponent(card, "DiscardPileElement");
        const bool deck = isCard && IsInMaterializedDeckPool(model, target);
        const bool offboard = isCard && !hand && !lane && (discard != deck);
        const char* zone = offboard ? (discard ? "discard" : "deck") : "stock";
        const bool report = offboard && auditReported[index] < kAuditLogLimit;
        const AuditStats before = report ? CaptureAuditStats(card) : AuditStats{};
        const int abilityType = effect && index == 2 && auditGrantType.IsValid()
            ? auditGrantType[effect].Get() :
            (effect && index == 3 && auditRemoveGrantType.IsValid()
                ? auditRemoveGrantType[effect].Get() : -1);
        invoke();
        if (!report) return;
        ++auditReported[index];
        const AuditStats after = entityExists[model](target)
            ? CaptureAuditStats(getEntityById[model](target)) : AuditStats{};
        // Record changes only if the handler entered in Deck/Discard.
        // A repeated off-board visit starts a new epoch after presentation.
        if (auto* delta = FindOffboardDelta(model, target, true)) {
            if (delta->compared) {
                const Obj* oldModel = delta->model;
                const int oldId = delta->cardId;
                *delta = {};
                delta->model = const_cast<Obj*>(oldModel);
                delta->cardId = oldId;
            }
            auto addValid = [&](int a, int b, int& accumulator) {
                if (a == -99999 || b == -99999) {
                    ++delta->invalidFields;
                    return;
                }
                const std::int64_t total = static_cast<std::int64_t>(accumulator) +
                    static_cast<std::int64_t>(b) - a;
                if (total < -100000 || total > 100000) {
                    ++delta->invalidFields;
                    return;
                }
                accumulator = static_cast<int>(total);
            };
            if (delta->events == 0) delta->firstSignature = before.componentSignature;
            addValid(before.attack, after.attack, delta->attack);
            addValid(before.maxHealth, after.maxHealth, delta->health);
            addValid(before.cost, after.cost, delta->cost);
            if (before.componentSignature != after.componentSignature)
                ++delta->traitObservations;
            delta->lastSignature = after.componentSignature;
            ++delta->events;
            if (delta->events <= 12)
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: ACCUMULATE model=%p card=%d zone=%s event=%u attack=%d health=%d cost=%d traitsChanged=%u (read-only)",
                    model, target, zone, delta->events, delta->attack,
                    delta->health, delta->cost, delta->traitObservations);
        }
        static constexpr const char* names[] = {
            "Buff", "ModifySunCost", "GrantAbility", "RemoveGrantAbility",
            "RemoveBuff", "RemoveModifySunCost"
        };
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s event=%llu model=%p target=%d source=%d effectEntity=%d zoneAtEntry=%s abilityType=%d attack=%d->%d maxHealth=%d->%d damage=%d->%d cost=%d->%d components=%d->%d signature=%llx->%llx (read-only)",
            names[index], (unsigned long long)seen, model, target, source,
            effectEntity, zone, abilityType, before.attack, after.attack,
            before.maxHealth, after.maxHealth, before.damage, after.damage,
            before.cost, after.cost, before.componentCount, after.componentCount,
            (unsigned long long)before.componentSignature,
            (unsigned long long)after.componentSignature);
    }

    inline void AuditedStockEffect(int index, void* self, Obj* effect,
                                   const void* method) {
        auto original = originalAuditProcess[index];
        if (!original) return;
        AuditOwnedEffect(index, self, effect, [&] { original(self, effect, method); });
    }

    static void AuditBuffHook(void* s, Obj* e, const void* m) { AuditedStockEffect(0,s,e,m); }
    static void AuditCostHook(void* s, Obj* e, const void* m) { AuditedStockEffect(1,s,e,m); }
    static void AuditGrantHook(void* s, Obj* e, const void* m) { AuditedStockEffect(2,s,e,m); }
    static void AuditRemoveGrantHook(void* s, Obj* e, const void* m) { AuditedStockEffect(3,s,e,m); }
    static void AuditRemoveBuffHook(void* s, Obj* e, const void* m) { AuditedStockEffect(4,s,e,m); }
    static void AuditRemoveCostHook(void* s, Obj* e, const void* m) { AuditedStockEffect(5,s,e,m); }

    inline void InstallOffBoardAudit() {
        auto baseEffect = BNM::Class("PvZCards.Engine", "Effect");
        if (!baseEffect.IsValid()) return;
        auditEffectTarget = baseEffect.GetField("TargetId");
        auditEffectSource = baseEffect.GetField("SourceId");
        auditEffectEntity = baseEffect.GetField("EffectEntityId");
        if (!auditEffectTarget.IsValid() || !auditEffectSource.IsValid() ||
            !auditEffectEntity.IsValid() || !traceEntityComponents.IsValid() ||
            !componentCalculateAttack.IsValid()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: unavailable base metadata");
            return;
        }
        const char* systems[] = { "BuffSystem", "ModifySunCostSystem", "GrantAbilitySystem",
            "RemoveGrantAbilitySystem", "RemoveBuffSystem", "RemoveModifySunCostSystem" };
        const char* modelFields[] = { "<Entities>k__BackingField", "<Entities>k__BackingField",
            "<Entities>k__BackingField", "<Model>k__BackingField",
            "<Model>k__BackingField", "<Model>k__BackingField" };
        AuditProcessFn hooks[] = { AuditBuffHook, AuditCostHook, AuditGrantHook,
            AuditRemoveGrantHook, AuditRemoveBuffHook, AuditRemoveCostHook };
        const std::uintptr_t expectedRvas[] = {0x21FD444, 0x21F10DC, 0x21F45EC,
            0x21F1C58, 0x21FD258, 0x21F1EA8};
        auto grant = BNM::Class("PvZCards.Engine.Effects", "GrantAbilityEffect");
        auto removeGrant = BNM::Class("PvZCards.Engine.Effects", "RemoveGrantAbilityEffect");
        if (grant.IsValid()) auditGrantType = grant.GetField("GrantableAbilityType");
        if (removeGrant.IsValid()) auditRemoveGrantType = removeGrant.GetField("GrantableAbilityType");
        for (int i = 0; i < 6; ++i) {
            // The stock buff hook belongs to DiscardedSystem; grant and
            // remove-grant hooks belong to GrantAbility. Those owners call
            // AuditOwnedEffect directly; never compete with their hooks.
            const bool ownedElsewhere = i == 0 || i == 2 || i == 3;
            if (ownedElsewhere) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s shared-owner (no duplicate native hook)", systems[i]);
            }
            auto klass = BNM::Class("PvZCards.Core.Systems", systems[i]);
            if (!klass.IsValid()) continue;
            auditSystemModel[i] = klass.GetField(modelFields[i]);
            auto method = klass.GetMethod("ProcessEffect", 1);
            if (!auditSystemModel[i].IsValid() || !method.IsValid() ||
                !nativeIl2cppBase ||
                static_cast<std::uintptr_t>(method.GetOffset()) != nativeIl2cppBase + expectedRvas[i]) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s skipped (metadata/RVA mismatch)", systems[i]);
                continue;
            }
            if (ownedElsewhere) continue;
            BNM::BasicHook(method, hooks[i], originalAuditProcess[i]);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s hooked=%d rva=0x%llx",
                systems[i], int(originalAuditProcess[i] != nullptr),
                (unsigned long long)expectedRvas[i]);
        }
    }

    inline bool Initialize() {
        auto oneTimeProcessor = BNM::Class("PvZCards.Game", "PlayOneTimeEffectRecordProcessor");
        auto oneTimeRecordClass = BNM::Class("PvZCards.Engine.Records", "PlayOneTimeEffectRecord");
        auto startProcessor = BNM::Class("PvZCards.Game", "TriggeredAbilityStartRecordProcessor");
        auto abilityFactory = BNM::Class("PvZCards.Game", "TriggeredAbilityAnimationFactory");

        auto triggeredAbilityAnimation =
            BNM::Class("PvZCards.Game", "TriggeredAbilityEffectAnimation");

        auto timelineAnimation =
            BNM::Class("PvZCards.Game", "TimelineCommandEffectAnimation");

        auto localCardChecker =
            BNM::Class("PvZCards.Game", "LocalCardChecker");

        auto eventActionSequence =
            BNM::Class("PvZCards.Game", "EventActionSequencedEffectAnimation");

        auto commandContainer =
            BNM::Class("PvZCards.Game", "AnimationEventCommandContainer");

        auto animationQueue =
            BNM::Class("PvZCards.Game", "EffectAnimationQueue");

        auto effectAnimation =
            BNM::Class("PvZCards.Game", "EffectAnimation");

        auto visualEffectEventAction =
            BNM::Class("PvZCards.Game", "VisualEffectEventAction");
        auto buffEventAction = BNM::Class("PvZCards.Game", "BuffEventAction");
        auto cardViewClass = BNM::Class("PvZCards.Game", "CardViewComponent");
        auto cardFrameClass = BNM::Class("PvZCards.Game", "CardFrameViewComponent");
        auto attackComponentClass = BNM::Class("PvZCards.Engine.Components", "Attack");
        auto drawAllCardsEventAction =
            BNM::Class("PvZCards.Game", "DrawAllCardsEventAction");
        auto drawAllCardAnimationFactory =
            BNM::Class("PvZCards.Game", "DrawAllCardAnimationFactory");
        nullEffectAnimationClass =
            BNM::Class("PvZCards.Game", "NullEffectAnimation");


        auto entityViewMapper =
            BNM::Class("PvZCards.Game", "EntityViewMapper");

        auto entityModel =
            BNM::Class("PvZCards.Engine", "EntityModel");

        auto entity =
            BNM::Class("PvZCards.Engine", "Entity");

        auto deckElement =
            BNM::Class("PvZCards.Engine.Components", "DeckElement");

        auto discardPileElement =
            BNM::Class("PvZCards.Engine.Components", "DiscardPileElement");

        if (!triggeredAbilityAnimation.IsValid() ||
            !timelineAnimation.IsValid() ||
            !localCardChecker.IsValid() ||
            !eventActionSequence.IsValid() ||
            !commandContainer.IsValid() ||
            !animationQueue.IsValid() ||
            !effectAnimation.IsValid() ||
            !visualEffectEventAction.IsValid() ||
            !drawAllCardsEventAction.IsValid() ||
            !drawAllCardAnimationFactory.IsValid() ||
            !nullEffectAnimationClass.IsValid() ||
            !entityViewMapper.IsValid() ||
            !entityModel.IsValid() ||
            !entity.IsValid() ||
            !deckElement.IsValid() ||
            !discardPileElement.IsValid())
        {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: metadata class lookup failed"
            );
            return false;
        }

        timelineEntityViewMapper =
            timelineAnimation.GetField("<EntityViewMapper>k__BackingField");

        timelineSourceCardId =
            timelineAnimation.GetField("SourceCardId");

        viewMapperEntityModel =
            entityViewMapper.GetField("<EntityModel>k__BackingField");

        entityExists =
            entityModel.GetMethod("EntityExists", 1).cast<bool>();

        getEntityById =
            entityModel.GetMethod("GetEntityById", 1).cast<Obj*>();

        entityHasComponent =
            entity.GetMethod("HasComponent", 1).cast<bool>();

        deckElementType =
            reinterpret_cast<Obj*>(deckElement.GetMonoType());

        discardPileElementType =
            reinterpret_cast<Obj*>(discardPileElement.GetMonoType());

        if (cardViewClass.IsValid()) {
            cardViewEntityId = cardViewClass.GetField("CardId");
            cardViewFrame = cardViewClass.GetField("Frame");
        }
        if (cardFrameClass.IsValid()) {
            frameCurrentAttack = cardFrameClass.GetField("CurrentAttack");
            frameBuffAttack = cardFrameClass.GetMethod("BuffAttack", 1).cast<void>();
        }
        if (attackComponentClass.IsValid())
            componentCalculateAttack = attackComponentClass.GetMethod("CalculateAttack", 0).cast<int>();
        auto healthClass = BNM::Class("PvZCards.Engine.Components", "Health");
        auto sunCostClass = BNM::Class("PvZCards.Engine.Components", "SunCost");
        auto modCalculator = BNM::Class("PvZCards.Engine.Utilities", "ModifiableIntCalculator");
        if (healthClass.IsValid()) {
            healthMaxHealth = healthClass.GetField("MaxHealth");
            healthCurrentDamage = healthClass.GetField("CurrentDamage");
        }
        if (sunCostClass.IsValid()) sunCostValue = sunCostClass.GetField("SunCostValue");
        if (modCalculator.IsValid())
            calculateModifiableValue = modCalculator.GetMethod("CalculateValue", 1).cast<int>();
        if (cardFrameClass.IsValid()) {
            frameMaxHealth = cardFrameClass.GetField("MaxHealth");
            frameCurrentHealth = cardFrameClass.GetField("CurrentHealth");
            frameCurrentSunCost = cardFrameClass.GetField("CurrentSunCost");
            frameBuffHealth = cardFrameClass.GetMethod("BuffHealth", 1).cast<void>();
            frameModifySunCost = cardFrameClass.GetMethod("ModifySunCost", 1).cast<void>();
            frameSetSpecialAbilitySprites = cardFrameClass.GetMethod("SetSpecialAbilitySprites", 1).cast<void>();
        }
        stateClass = BNM::Class("PvZCards.Engine", "EntityState");
        checkerClass = BNM::Class("PvZCards.Core.Systems.Utilities", "SpecialAbilityChecker");
        if (stateClass.IsValid() && checkerClass.IsValid()) {
            stateCtor = stateClass.GetMethod(".ctor", 0).cast<void>();
            checkerCtor = checkerClass.GetMethod(".ctor", 0).cast<void>();
            stateAbilities = stateClass.GetField("Abilities");
            checkerAddSpecialAbilities = checkerClass.GetMethod("AddSpecialAbilities", 2).cast<void>();
            traitSyncReady = stateCtor.IsValid() && checkerCtor.IsValid() &&
                stateAbilities.IsValid() && checkerAddSpecialAbilities.IsValid();
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: health=%d cost=%d traits=%d",
            int(frameBuffHealth.IsValid() && calculateModifiableValue.IsValid()),
            int(frameModifySunCost.IsValid() && calculateModifiableValue.IsValid()),
            int(traitSyncReady && frameSetSpecialAbilitySprites.IsValid()));
        traceAbilityGroupId = triggeredAbilityAnimation.GetField("AbilityGroupId");
        traceVisualSourceId = visualEffectEventAction.GetField("SourceId");
        traceVisualTargetIds = visualEffectEventAction.GetField("TargetIds");
        traceVisualMapper = visualEffectEventAction.GetField("<EntityViewMapper>k__BackingField");
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: metadata source=%d targets=%d mapper=%d buffClass=%d",
                 int(traceVisualSourceId.IsValid()), int(traceVisualTargetIds.IsValid()),
                 int(traceVisualMapper.IsValid()), int(buffEventAction.IsValid()));
        // Optional inspection metadata: missing fields disable only diagnostics.
        traceModelEntities = entityModel.GetField("Entities");
        traceEntityComponents = entity.GetField("Components");
        auto cardClass = BNM::Class("PvZCards.Engine.Components", "Card");
        auto filterClass = BNM::Class("PvZCards.Engine.Components", "EntityFilter");
        auto compositeClass = BNM::Class("PvZCards.Engine.Queries", "CompositeQuery");
        auto poolClass = BNM::Class("PvZCards.Engine.Components", "OrderedCardPool");
        auto poolEntryClass = BNM::Class("PvZCards.Engine.Components.Data", "CardPoolEntry");
        if (cardClass.IsValid()) traceCardGuid = cardClass.GetField("Guid");
        if (filterClass.IsValid()) traceSelfFilterQuery = filterClass.GetField("Query");
        if (compositeClass.IsValid()) traceCompositeQueries = compositeClass.GetField("Queries");
        if (poolClass.IsValid()) traceOrderedPoolEntries = poolClass.GetField("Entries");
        if (poolEntryClass.IsValid()) {
            tracePoolEntryEntityId = poolEntryClass.GetField("EntityId");
            tracePoolEntryGuid = poolEntryClass.GetField("Guid");
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDZONE: setup modelEntities=%d entityComponents=%d cardGuid=%d selfQuery=%d compositeQueries=%d poolEntries=%d entryId=%d",
            int(traceModelEntities.IsValid()), int(traceEntityComponents.IsValid()),
            int(traceCardGuid.IsValid()), int(traceSelfFilterQuery.IsValid()),
            int(traceCompositeQueries.IsValid()), int(traceOrderedPoolEntries.IsValid()),
            int(tracePoolEntryEntityId.IsValid()));

        auto recordClass = BNM::Class("PvZCards.Engine", "Record");
        auto effectEntityClass = BNM::Class("PvZCards.Engine.Components", "EffectEntity");
        if (recordClass.IsValid()) {
            traceRecordGetDictionary = recordClass.GetMethod("GetDictionary", 0).cast<Obj*>();
            traceRecordGetInt = recordClass.GetMethod("GetInt", 1).cast<int>();
        }
        if (startProcessor.IsValid())
            traceProcessorEntityModel = startProcessor.GetField("<EntityModel>k__BackingField");
        if (effectEntityClass.IsValid())
            traceEffectEntityGetSource = effectEntityClass.GetMethod("GetSource", 1).cast<int>();
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/RECORDIDENTITY: diagnostics ready dict=%d getInt=%d model=%d getSource=%d",
            static_cast<int>(traceRecordGetDictionary.IsValid()),
            static_cast<int>(traceRecordGetInt.IsValid()),
            static_cast<int>(traceProcessorEntityModel.IsValid()),
            static_cast<int>(traceEffectEntityGetSource.IsValid()));
        if (oneTimeRecordClass.IsValid()) {
            oneTimeCardId = oneTimeRecordClass.GetMethod("get_CardId", 0).cast<int>();
            oneTimeHandId = oneTimeRecordClass.GetMethod("get_HandId", 0).cast<int>();
            oneTimeLocationId = oneTimeRecordClass.GetMethod("get_LocationId", 0).cast<int>();
        }
        // Private method overload is uniquely selected by argument count.
        auto oneTimeProcess = oneTimeProcessor.IsValid()
            ? oneTimeProcessor.GetMethod("ProcessRecord", 1) : BNM::MethodBase{};
        auto processRecordGroup = startProcessor.IsValid()
            ? startProcessor.GetMethod("ProcessRecordGroup", 3) : BNM::MethodBase{};
        auto createAbilityCore = abilityFactory.IsValid()
            ? abilityFactory.GetMethod("CreateAbilityAnimation", 4) : BNM::MethodBase{};
        auto addAbility = startProcessor.IsValid()
            ? startProcessor.GetMethod("AddAbilityAnimation", 3) : BNM::MethodBase{};

        auto baseOnStarted =
            timelineAnimation.GetMethod("OnStarted", 0);

        timelineBaseOnStarted = baseOnStarted.cast<void>();
        timelineCommandContainer = timelineAnimation.GetField("<CommandContainer>k__BackingField");
        timelineIsCardTimelineComplete = timelineAnimation.GetField("IsCardTimelineComplete");
        timelineWaitForLoad = timelineAnimation.GetField("WaitForLoad");
        commandEventCommands = commandContainer.GetField("EventCommands");
        commandAnimationQueue = commandContainer.GetField("AnimationQueue");
        queueAnimations = animationQueue.GetField("Animations");
        queueFrontAnimations = animationQueue.GetField("FrontAnimations");
        effectIsStarted = effectAnimation.GetField("<IsStarted>k__BackingField");
        effectIsCancelled = effectAnimation.GetField("<IsCancelled>k__BackingField");
        timelineIsCompleted = timelineAnimation.GetMethod("IsCompleted", 0).cast<bool>();
        commandHasAllCommandsBeenActivated = commandContainer.GetMethod("HasAllCommandsBeenActivated", 0).cast<bool>();
        commandIsChildAnimationsComplete = commandContainer.GetMethod("IsChildAnimationsComplete", 0).cast<bool>();

        auto handleCommandLoadComplete =
            timelineAnimation.GetMethod("HandleCommandLoadComplete", 0);

        auto getCardView =
            timelineAnimation.GetMethod("get_CardView", 0);

        auto triggeredOnStarted =
            triggeredAbilityAnimation.GetMethod("OnStarted", 0);

        auto isInLocalHand =
            localCardChecker.GetMethod("IsInLocalHand", 1);

        auto addParticleEffectAnimation =
            eventActionSequence.GetMethod("AddParticleEffectAnimation", 2);

        auto executeRemainingCommands =
            commandContainer.GetMethod("ExecuteRemainingCommands", 0);

        auto createRequiredAnimation =
            visualEffectEventAction.GetMethod("CreateAnimationForRequiredEvent", 0);
        auto createTargetReactionAnimation =
            visualEffectEventAction.GetMethod("CreateTargetReactionAnimation", 1);

        auto buffCommonTarget = buffEventAction.IsValid() ? buffEventAction.GetMethod("CreateCommonTargetReactionAnimation", 0) : BNM::MethodBase{};
        auto drawCommonTarget = drawAllCardsEventAction.GetMethod("CreateCommonTargetReactionAnimation", 0);
        auto createDrawAnimation = drawAllCardsEventAction.GetMethod("CreateDrawAnimation", 0);
        auto buildFromRecords = drawAllCardAnimationFactory.GetMethod("BuildAnimationFromRecordList", 2);
        auto drawSourceParticle = drawAllCardsEventAction.GetMethod("RetrieveFactoryAndCreateParticle", 1);
        auto baseSourceParticle = visualEffectEventAction.GetMethod("RetrieveFactoryAndCreateParticle", 1);
        auto createPersistentVisuals = visualEffectEventAction.GetMethod("CreatePersistentVisuals", 1);
        nullEffectAnimationCtor = nullEffectAnimationClass.GetMethod(".ctor", 0).cast<void>();

        auto checkForCompletion = timelineAnimation.GetMethod("CheckForCompletion", 0);
        auto queueUpdate = animationQueue.GetMethod("Update", 0);
        auto queueIsEmpty = animationQueue.GetMethod("IsEmpty", 0);
        auto triggeredCanYield = triggeredAbilityAnimation.GetMethod("CanYield", 0);


        bool valid =
            timelineEntityViewMapper.IsValid() &&
            timelineSourceCardId.IsValid() &&
            viewMapperEntityModel.IsValid() &&
            entityExists.IsValid() &&
            getEntityById.IsValid() &&
            entityHasComponent.IsValid() &&
            deckElementType &&
            discardPileElementType &&
            timelineBaseOnStarted.IsValid() &&
            timelineCommandContainer.IsValid() &&
            timelineIsCardTimelineComplete.IsValid() &&
            timelineWaitForLoad.IsValid() &&
            commandEventCommands.IsValid() &&
            commandAnimationQueue.IsValid() &&
            queueAnimations.IsValid() &&
            queueFrontAnimations.IsValid() &&
            effectIsStarted.IsValid() &&
            effectIsCancelled.IsValid() &&
            checkForCompletion.IsValid() &&
            queueUpdate.IsValid() &&
            queueIsEmpty.IsValid() &&
            triggeredCanYield.IsValid() &&
            createRequiredAnimation.IsValid() &&
            createTargetReactionAnimation.IsValid() &&
            drawCommonTarget.IsValid() &&
            createDrawAnimation.IsValid() &&
            buildFromRecords.IsValid() &&
            drawSourceParticle.IsValid() &&
            baseSourceParticle.IsValid() &&
            createPersistentVisuals.IsValid() &&
            nullEffectAnimationCtor.IsValid() &&
            handleCommandLoadComplete.IsValid() &&
            timelineIsCompleted.IsValid() &&
            commandHasAllCommandsBeenActivated.IsValid() &&
            commandIsChildAnimationsComplete.IsValid() &&
            getCardView.IsValid() &&
            triggeredOnStarted.IsValid() &&
            isInLocalHand.IsValid() &&
            addParticleEffectAnimation.IsValid() &&
            executeRemainingCommands.IsValid();

        if (!valid) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: metadata method/field lookup failed"
            );
            return false;
        }

        // Attack-only test: sync tracked off-board buff recipients after stock ShowInHand.
        if (cardViewClass.IsValid()) {
            auto cardLoaded = cardViewClass.GetMethod("OnCardLoaded", 1);
            auto showInHand = cardViewClass.GetMethod("ShowInHand", 0);
            if (cardLoaded.IsValid())
                BNM::BasicHook(cardLoaded, CardLoadedSyncHook, originalCardLoaded);
            if (showInHand.IsValid())
                BNM::BasicHook(showInHand, ShowInHandTraceHook, originalShowInHand);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: cardLoadedHook=%d showInHandHook=%d",
                     int(originalCardLoaded != nullptr), int(originalShowInHand != nullptr));
        }
        auto addViewObject = entityViewMapper.GetMethod("AddViewObject", 2);
        if (addViewObject.IsValid())
            BNM::BasicHook(addViewObject, AddViewObjectTraceHook, originalAddViewObject);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: addViewHook=%d expectedRVA=0x1F1A340",
                 int(originalAddViewObject != nullptr));

        // BRIDGETRACE is diagnostic only. Fail gracefully if a lookup is
        // unavailable; never alter any gameplay or stock presentation result.
        if (!traceAbilityGroupId.IsValid())
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: AbilityGroupId missing; group on-start logging unavailable");
        if (oneTimeProcess.IsValid() && oneTimeCardId.IsValid()) {
            BNM::BasicHook(oneTimeProcess, OneTimeRecordTraceHook, originalOneTimeRecord);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ONETIME hook=%d native=%p RVA=0x1FA7C04",
                static_cast<int>(originalOneTimeRecord != nullptr), reinterpret_cast<void*>(oneTimeProcess.GetOffset()));
        } else LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ONETIME metadata unavailable");
        if (processRecordGroup.IsValid()) {
            BNM::BasicHook(processRecordGroup, TriggerRecordGroupTraceHook, originalTriggerRecordGroup);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: GROUP hook=%d native=%p RVA=0x1FCD14C",
                static_cast<int>(originalTriggerRecordGroup != nullptr), reinterpret_cast<void*>(processRecordGroup.GetOffset()));
        } else LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: GROUP metadata unavailable");
        if (createAbilityCore.IsValid()) {
            BNM::BasicHook(createAbilityCore, CreateAbilityCoreTraceHook, originalCreateAbilityCore);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: FACTORY hook=%d native=%p RVA=0x1FCB638 (4 args; wrapper UNHOOKED)",
                static_cast<int>(originalCreateAbilityCore != nullptr), reinterpret_cast<void*>(createAbilityCore.GetOffset()));
        } else LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: FACTORY metadata unavailable");
        if (addAbility.IsValid()) {
            BNM::BasicHook(addAbility, AddAbilityRecordTraceHook, originalAddAbilityRecord);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ADDABILITY hook=%d native=%p RVA=0x1FCD7E4",
                static_cast<int>(originalAddAbilityRecord != nullptr), reinterpret_cast<void*>(addAbility.GetOffset()));
        } else LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/BRIDGETRACE: ADDABILITY metadata unavailable");

        // Resolve native libil2cpp base with ASLR accounted for.
        Dl_info il2cppInfo{};

        void* baseOnStartedAddress =
            reinterpret_cast<void*>(baseOnStarted.GetOffset());

        if (!baseOnStartedAddress ||
            !dladdr(baseOnStartedAddress, &il2cppInfo) ||
            !il2cppInfo.dli_fbase)
        {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: failed to locate libil2cpp base for view lookup trace"
            );
            return false;
        }

        nativeIl2cppBase = reinterpret_cast<std::uintptr_t>(il2cppInfo.dli_fbase);
        void* genericLookupAddress = reinterpret_cast<void*>(
            reinterpret_cast<std::uintptr_t>(il2cppInfo.dli_fbase) +
            kGenericGetViewObjectRva
            );

        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/VIEWLOOKUP: installing generic hook base=%p target=%p RVA=0x%llx",
            il2cppInfo.dli_fbase,
            genericLookupAddress,
            static_cast<unsigned long long>(kGenericGetViewObjectRva)
        );

        ::BasicHook(
            genericLookupAddress,
            GetViewObjectGenericHook,
            originalGetViewObjectGeneric
        );

        if (!originalGetViewObjectGeneric) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: generic GetViewObject hook failed"
            );
            return false;
        }

        BNM::BasicHook(
            handleCommandLoadComplete,
            HandleCommandLoadCompleteHook,
            originalHandleCommandLoadComplete
        );

        if (!originalHandleCommandLoadComplete) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation: HandleCommandLoadComplete hook failed");
            return false;
        }

        BNM::BasicHook(
            getCardView,
            GetCardViewHook,
            originalGetCardView
        );

        if (!originalGetCardView) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: get_CardView hook failed"
            );
            return false;
        }

        BNM::BasicHook(
            triggeredOnStarted,
            TriggeredAbilityOnStartedHook,
            originalTriggeredAbilityOnStarted
        );

        if (!originalTriggeredAbilityOnStarted) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: TriggeredAbilityEffectAnimation.OnStarted hook failed"
            );
            return false;
        }

        BNM::BasicHook(
            isInLocalHand,
            IsInLocalHandHook,
            originalIsInLocalHand
        );

        if (!originalIsInLocalHand) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: LocalCardChecker.IsInLocalHand hook failed"
            );
            return false;
        }

        BNM::BasicHook(
            createRequiredAnimation,
            CreateRequiredAnimationHook,
            originalCreateRequiredAnimation
        );

        if (!originalCreateRequiredAnimation) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation: VisualEffectEventAction.CreateAnimationForRequiredEvent hook failed");
            return false;
        }

        BNM::BasicHook(
            createTargetReactionAnimation,
            CreateTargetReactionAnimationHook,
            originalCreateTargetReactionAnimation
        );
        if (!originalCreateTargetReactionAnimation) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEONLY: target reaction trace hook failed");
            return false;
        }

        // This is a long, substantive method, not the adjacent draw thunk.
        // Failure to install diagnostics must not affect gameplay initialization.
        if (buffCommonTarget.IsValid() && traceVisualSourceId.IsValid() &&
            traceVisualTargetIds.IsValid() && traceVisualMapper.IsValid()) {
            BNM::BasicHook(buffCommonTarget, BuffCommonTargetTraceHook, originalBuffCommonTarget);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: BUFF hook=%d native=%p expectedRVA=0x1EDC9B0",
                     int(originalBuffCommonTarget != nullptr), reinterpret_cast<void*>(buffCommonTarget.GetOffset()));
        } else CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/TARGETBOUNDARY: BUFF hook unavailable");

        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DISPATCH: resolved drawCommon=%p drawParticle=%p baseParticle=%p (expected RVA common=0x1eddb2c particle=0x1eddb30 base=0x1edb798)",
            reinterpret_cast<void*>(drawCommonTarget.GetOffset()),
            reinterpret_cast<void*>(drawSourceParticle.GetOffset()),
            reinterpret_cast<void*>(baseSourceParticle.GetOffset()));
        // SAFETY: do not inline-hook adjacent draw methods. On ARM64,
        // CreateCommonTargetReactionAnimation (0x1EDDB2C) is a single branch
        // immediately before particle override (0x1EDDB30). Installing
        // trampolines here can invalidate stock forwarding under Houdini.
        // Leave CreateDrawAnimation and BuildAnimationFromRecordList untouched.
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/HOOKSAFETY: draw common, create draw and record factory inline hooks DISABLED");
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DRAWTRACE: resolved CreateDrawAnimation=%p BuildAnimationFromRecordList=%p (metadata only)",
            reinterpret_cast<void*>(createDrawAnimation.GetOffset()),
            reinterpret_cast<void*>(buildFromRecords.GetOffset()));

        // Dump verified: DrawAllCardsEventAction override RVA 0x1EDDB30,
        // VisualEffectEventAction base implementation RVA 0x1EDB798.
        // Other concrete overrides will require registration separately.
        BNM::BasicHook(drawSourceParticle, DrawSourceParticleHook, originalDrawSourceParticle);
        if (!originalDrawSourceParticle) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEPARTICLE: draw override hook failed");
            return false;
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/HOOKSAFETY: draw particle native=%p trampoline=%p",
            reinterpret_cast<void*>(drawSourceParticle.GetOffset()), reinterpret_cast<void*>(originalDrawSourceParticle));
        BNM::BasicHook(baseSourceParticle, BaseSourceParticleHook, originalBaseSourceParticle);
        if (!originalBaseSourceParticle) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/SOURCEPARTICLE: base hook failed");
            return false;
        }

        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/HOOKSAFETY: base particle native=%p trampoline=%p",
            reinterpret_cast<void*>(baseSourceParticle.GetOffset()), reinterpret_cast<void*>(originalBaseSourceParticle));
        // Base virtual slot 18, RVA 0x1EDBEC8. DrawAllCardsEventAction
        // inherits this implementation. Preserve overrides from other classes.
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/PERSISTENT: resolved base=%p expectedRVA=0x1EDBEC8",
            reinterpret_cast<void*>(createPersistentVisuals.GetOffset()));
        BNM::BasicHook(createPersistentVisuals, CreatePersistentVisualsHook, originalCreatePersistentVisuals);
        if (!originalCreatePersistentVisuals) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/PERSISTENT: hook installation failed");
            return false;
        }

        BNM::BasicHook(
            executeRemainingCommands,
            ExecuteRemainingCommandsHook,
            originalExecuteRemainingCommands
        );

        if (!originalExecuteRemainingCommands) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: AnimationEventCommandContainer.ExecuteRemainingCommands hook failed"
            );
            return false;
        }

        BNM::BasicHook(
            addParticleEffectAnimation,
            AddParticleEffectAnimationHook,
            originalAddParticleEffectAnimation
        );

        if (!originalAddParticleEffectAnimation) {
            LOG_ERR(
                "CustomComponents/ZoneTriggeredAbilityPresentation: EventActionSequencedEffectAnimation.AddParticleEffectAnimation hook failed"
            );
            return false;
        }

        BNM::BasicHook(checkForCompletion, CheckForCompletionHook, originalCheckForCompletion);
        BNM::BasicHook(queueUpdate, QueueUpdateHook, originalQueueUpdate);
        BNM::BasicHook(queueIsEmpty, QueueIsEmptyHook, originalQueueIsEmpty);
        BNM::BasicHook(triggeredCanYield, TriggeredCanYieldHook, originalTriggeredCanYield);
        if (!originalCheckForCompletion || !originalQueueUpdate ||
            !originalQueueIsEmpty || !originalTriggeredCanYield) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: hook installation failed");
            return false;
        }

        // Optional, read-only lane-entry hooks. Do not fail initialization if
        // the game changes: strict RVA checks protect our working hand paths.
        // ARM64 RVAs from the supplied dump.cs (same libil2cpp build).
        auto laneCreateClass = BNM::Class("PvZCards.Game", "CreateCardAnimation");
        auto laneEventClass = BNM::Class("PvZCards.Game", "CreateCardEventAction");
        auto laneFactoryClass = BNM::Class("PvZCards.Game", "AnimationEventActionFactory");
        auto laneMediatorClass = BNM::Class("PvZCards.Game", "LaneViewMediator");
        auto laneRecordClass = BNM::Class("PvZCards.Engine.Records", "CreateCardInLaneRecord");
        auto laneCardIdData = BNM::Class("PvZCards.Engine.Records", "CardIdData");
        auto laneEffectRecord = BNM::Class("PvZCards.Engine.Records", "EffectRecord");
        if (laneCreateClass.IsValid()) {
            laneAnimationCardId = laneCreateClass.GetField("CardId");
            laneAnimationLaneId = laneCreateClass.GetField("LaneId");
            laneAnimationOrder = laneCreateClass.GetField("OrderInLane");
            laneAnimationFaceDown = laneCreateClass.GetField("IsFaceDown");
            laneAnimationComplete = laneCreateClass.GetField("IsComplete");
            laneAnimationCardView = laneCreateClass.GetField("CardView");
        }
        if (laneEventClass.IsValid()) {
            laneEventCardId = laneEventClass.GetField("CardId");
            laneEventOrder = laneEventClass.GetField("OrderInLane");
            laneEventFaceDown = laneEventClass.GetField("IsFaceDown");
        }
        if (laneRecordClass.IsValid())
            laneRecordGetCard = laneRecordClass.GetMethod("get_Card", 0).cast<Obj*>();
        if (laneCardIdData.IsValid()) laneCardDataEntityId = laneCardIdData.GetField("EntityId");
        if (laneEffectRecord.IsValid()) laneRecordTargetId = laneEffectRecord.GetField("TargetId");

        // GetOffset is the real native address in this BNM build. Refuse to
        // inline-hook unknown versions or unexpectedly resolved overloads.
        auto laneRvaMatches = [&](const auto& method, std::uintptr_t rva) {
            return method.IsValid() && nativeIl2cppBase != 0 &&
                static_cast<std::uintptr_t>(method.GetOffset()) == nativeIl2cppBase + rva;
        };
        auto laneCreateEventMethod = laneFactoryClass.IsValid()
            ? laneFactoryClass.GetMethod("CreateCardEvent", 1) : BNM::MethodBase{};
        auto lanePreloadMethod = laneEventClass.IsValid()
            ? laneEventClass.GetMethod("Preload", 0) : BNM::MethodBase{};
        auto laneStartMethod = laneCreateClass.IsValid()
            ? laneCreateClass.GetMethod("OnStarted", 0) : BNM::MethodBase{};
        auto laneShowMethod = laneCreateClass.IsValid()
            ? laneCreateClass.GetMethod("ShowCard", 0) : BNM::MethodBase{};
        auto laneCompletedMethod = laneCreateClass.IsValid()
            ? laneCreateClass.GetMethod("HandleCompleted", 0) : BNM::MethodBase{};
        auto laneAddMethod = laneMediatorClass.IsValid()
            ? laneMediatorClass.GetMethod("AddToLane", 2) : BNM::MethodBase{};
        auto laneBoardMethod = cardViewClass.IsValid()
            ? cardViewClass.GetMethod("HandlePlayedToBoard", 1) : BNM::MethodBase{};
        if (laneRvaMatches(laneCreateEventMethod, 0x1EDEF44))
            BNM::BasicHook(laneCreateEventMethod, LaneCreateCardEventTraceHook, originalLaneCreateCardEvent);
        if (laneRvaMatches(lanePreloadMethod, 0x1EDCEEC))
            BNM::BasicHook(lanePreloadMethod, LanePreloadTraceHook, originalLanePreload);
        if (laneRvaMatches(laneStartMethod, 0x1F02028))
            BNM::BasicHook(laneStartMethod, LaneCreateStartedTraceHook, originalLaneCreateStarted);
        if (laneRvaMatches(laneShowMethod, 0x1F02094))
            BNM::BasicHook(laneShowMethod, LaneShowCardTraceHook, originalLaneShowCard);
        if (laneRvaMatches(laneCompletedMethod, 0x1F02470))
            BNM::BasicHook(laneCompletedMethod, LaneCompletedTraceHook, originalLaneCompleted);
        if (laneRvaMatches(laneAddMethod, 0x1F81204))
            BNM::BasicHook(laneAddMethod, LaneAddToLaneTraceHook, originalLaneAddToLane);
        if (laneRvaMatches(laneBoardMethod, 0x1EF982C))
            BNM::BasicHook(laneBoardMethod, LanePlayedToBoardTraceHook, originalLanePlayedToBoard);
        CUSTOMCOMPONENTS_ZTAP_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: hooks factory=%d preload=%d start=%d show=%d completed=%d addToLane=%d playedToBoard=%d (read-only; RVA verified)",
            int(originalLaneCreateCardEvent != nullptr), int(originalLanePreload != nullptr),
            int(originalLaneCreateStarted != nullptr), int(originalLaneShowCard != nullptr),
            int(originalLaneCompleted != nullptr), int(originalLaneAddToLane != nullptr),
            int(originalLanePlayedToBoard != nullptr));

        InstallOffBoardAudit();

        LOG_INFO(
            "CustomComponents/ZoneTriggeredAbilityPresentation: ready (BRIDGEONLY: explicit custom trigger required; BRIDGETRACE retained; PARTICLEFORWARD retained)"
        );

        return true;
    }

} // namespace CustomComponents::Systems::ZoneTriggeredAbilityPresentationSystem

#undef CUSTOMCOMPONENTS_ZTAP_TRACE
