#include "diagnostics/DiagnosticLogging.hpp"

#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <cstring>
#include <dlfcn.h>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/ComplexMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "core/CardViewLifecycleCoordinator.hpp"
#include "core/SceneViewContext.hpp"
#include "log.h"

// Development traces are opt-in. Keep error reporting and the final
// initialization summary visible in normal builds.
#ifndef CUSTOMCOMPONENTS_ZTAP_VERBOSE
#define CUSTOMCOMPONENTS_ZTAP_VERBOSE 0
#endif
// Enable only for a focused lane presentation trace; standard return-stat and
// error logs stay active regardless of this switch.
#ifndef CUSTOMCOMPONENTS_LANE_I5_TRACE
#define CUSTOMCOMPONENTS_LANE_I5_TRACE 0
#endif
#if CUSTOMCOMPONENTS_LANE_I5_TRACE
#define CUSTOMCOMPONENTS_LANE_TRACE(...) CC_DIAG(__VA_ARGS__)
#else
#define CUSTOMCOMPONENTS_LANE_TRACE(...) ((void)0)
#endif
#if CUSTOMCOMPONENTS_ZTAP_VERBOSE
#define CUSTOMCOMPONENTS_ZTAP_TRACE(...) CC_DIAG(__VA_ARGS__)
#else
#define CUSTOMCOMPONENTS_ZTAP_TRACE(...) ((void)0)
#endif

namespace CustomComponents::Systems::ZoneTriggeredAbilityPresentationSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;

        // Scoped off-board source context, deferred callback identity and return origin.
    #include "presentation/detail/OffBoardSourceContext.inl"
    inline BNM::Field<Obj*> timelineEntityViewMapper;
    inline BNM::Field<int> timelineSourceCardId;
    inline BNM::Field<Obj*> viewMapperEntityModel;
    inline BNM::Field<Obj*> viewMapperViewMap;

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
    // TRICK-LIFECYCLE-DIAG: stock reveal observations only; no behavior changes.
    using TrickRevealVoidFn = void (*)(void*, const void*);
    inline TrickRevealVoidFn originalLocalRevealStart = nullptr;
    inline TrickRevealVoidFn originalLocalRevealComplete = nullptr;
    inline TrickRevealVoidFn originalOpponentRevealStart = nullptr;
    inline TrickRevealVoidFn originalOpponentRevealComplete = nullptr;
    inline BNM::Field<int> trickRevealCardId;
    inline BNM::Field<int> trickRevealHandId;
    inline BNM::Field<int> trickRevealTargetId;
    inline BNM::Field<Obj*> trickRevealCardView;
    inline BNM::Field<Obj*> trickRevealHandView;
    inline BNM::Field<Obj*> trickRevealTargetView;
    inline BNM::Field<bool> localRevealCompleteField;
    inline BNM::Field<bool> opponentRevealCompleteField;
    inline std::uint64_t trickRevealSequence = 0;
    static void LogTrickReveal(const char* stage, void* self, bool opponent) {
        Obj* a = static_cast<Obj*>(self);
        if (!a) return;
        if constexpr (false) CC_DIAG("CustomComponents/TRICK-LIFECYCLE-DIAG: %s side=%s seq=%llu anim=%p card=%d hand=%d target=%d cardView=%p handView=%p targetView=%p complete=%d",
            stage, opponent ? "opponent" : "local",
            static_cast<unsigned long long>(++trickRevealSequence), a,
            trickRevealCardId.IsValid() ? trickRevealCardId[a].Get() : -1,
            trickRevealHandId.IsValid() ? trickRevealHandId[a].Get() : -1,
            trickRevealTargetId.IsValid() ? trickRevealTargetId[a].Get() : -1,
            trickRevealCardView.IsValid() ? trickRevealCardView[a].Get() : nullptr,
            trickRevealHandView.IsValid() ? trickRevealHandView[a].Get() : nullptr,
            trickRevealTargetView.IsValid() ? trickRevealTargetView[a].Get() : nullptr,
            opponent ? (opponentRevealCompleteField.IsValid() ? int(opponentRevealCompleteField[a].Get()) : -1)
                     : (localRevealCompleteField.IsValid() ? int(localRevealCompleteField[a].Get()) : -1));
    }
    static void LocalRevealStartCapture(void* self, const void* method) {
        LogTrickReveal("REVEAL-START-ENTER", self, false);
        originalLocalRevealStart(self, method);
        LogTrickReveal("REVEAL-START-EXIT", self, false);
    }
    static void LocalRevealCompleteTrace(void* self, const void* method) {
        LogTrickReveal("REVEAL-COMPLETE-ENTER", self, false);
        originalLocalRevealComplete(self, method);
        LogTrickReveal("REVEAL-COMPLETE-EXIT", self, false);
    }
    static void OpponentRevealStartCapture(void* self, const void* method) {
        LogTrickReveal("REVEAL-START-ENTER", self, true);
        originalOpponentRevealStart(self, method);
        LogTrickReveal("REVEAL-START-EXIT", self, true);
    }
    static void OpponentRevealCompleteTrace(void* self, const void* method) {
        LogTrickReveal("REVEAL-COMPLETE-ENTER", self, true);
        originalOpponentRevealComplete(self, method);
        LogTrickReveal("REVEAL-COMPLETE-EXIT", self, true);
    }

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

    // Card-frame bindings, return baselines, frame repair and post-return sync hooks.
    #include "presentation/detail/CardFrameStatReconciliation.inl"

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
        if (n <= 300) if constexpr (false) CC_DIAG("CustomComponents/TRICK-LIFECYCLE-DIAG: PLAY-RECORD-ENTER seq=%llu record=%p card=%d hand=%d location=%d processor=%p",
            static_cast<unsigned long long>(n), record, card, hand, location, self);
        originalOneTimeRecord(self, record, method);
        if (n <= 300) if constexpr (false) CC_DIAG("CustomComponents/TRICK-LIFECYCLE-DIAG: PLAY-RECORD-EXIT seq=%llu card=%d",
            static_cast<unsigned long long>(n), card);
    }

    // Forward declarations for read-only diagnostics defined below.
    inline bool TryTraceRecordInt(Obj* record, int key, int& value);
    inline bool IsInMaterializedDeckPool(Obj* model, int cardId);
    inline OffBoardZone GetOffBoardZone(Obj* model, int entityIdValue);

    static void TriggerRecordGroupTraceHook(void* self, Obj* records, int cardEntityId, Obj* record, const void* method) {
        const auto n = ++groupTraceCount;
        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: GROUP-ENTER seq=%llu processor=%p records=%p card=%d record=%p",
            static_cast<unsigned long long>(n), self, records, cardEntityId, record);
        // DECK-HANG-DIAG: inspect the card/record at consumption, before stock
        // creates its animation. No entity-view lookup is performed here.
        if (record && self && traceProcessorEntityModel.IsValid()) {
            Obj* model = traceProcessorEntityModel[static_cast<Obj*>(self)].Get();
            int source = -1, effect = -1, group = -1, keyCard = -1;
            const bool hasSource = TryTraceRecordInt(record, 0, source);
            const bool hasEffect = TryTraceRecordInt(record, 2, effect);
            const bool hasGroup = TryTraceRecordInt(record, 3, group);
            const bool hasCard = TryTraceRecordInt(record, 6, keyCard);
            if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: GROUP seq=%llu card=%d deck=%d zone=%s source=%d/%d effect=%d/%d group=%d/%d keyCard=%d/%d record=%p records=%p",
                static_cast<unsigned long long>(n), cardEntityId,
                int(IsInMaterializedDeckPool(model, cardEntityId)), ZoneName(GetOffBoardZone(model, cardEntityId)),
                source, int(hasSource), effect, int(hasEffect), group, int(hasGroup), keyCard, int(hasCard), record, records);
        }
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
        if (result) {
            Obj* model = self && traceProcessorEntityModel.IsValid()
                ? traceProcessorEntityModel[static_cast<Obj*>(self)].Get() : nullptr;
            if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: ABILITY-CREATED card=%d group=%d anim=%p model=%p inDeck=%d zone=%s record=%p",
                cardId, traceAbilityGroupId.IsValid() ? traceAbilityGroupId[result].Get() : -1,
                result, model, int(IsInMaterializedDeckPool(model, cardId)),
                ZoneName(GetOffBoardZone(model, cardId)), record);
        }
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

    // UNIVERSAL-VIEW-GUARD: do not call EntityViewMapper.GetViewObject here;
    // it throws ViewMapException on absent keys. Fail open if mapper metadata
    // cannot be read. This is the live view map, not the model's card zone.
    inline bool TryResolveMappedCardView(Obj* mapper, int entityId, Obj** viewOut) {
        if (viewOut) *viewOut = nullptr;
        if (!mapper || entityId < 0 || !viewMapperViewMap.IsValid()) return false;
        Obj* mapObject = viewMapperViewMap[mapper].Get();
        if (!mapObject) return false;
        auto* map = reinterpret_cast<BNM::Structures::Mono::Dictionary<int, Obj*>*>(mapObject);
        Obj* view = nullptr;
        const bool found = map->TryGet(entityId, &view);
        if (viewOut) *viewOut = view;
        return found && view != nullptr;
    }

    // Called only after the owning stock animation queue becomes empty.
    // Unlike the return-ticket path this is equally valid for either client:
    // it uses the already-mapped CardView and the client's live gameplay model.
    // It never creates a view, changes the model or touches animation flags.
    inline void ReconcileIdleLaneFrame(const PendingIdleLaneFrame& entry) {
        if (!entry.model || !entry.mapper || !entry.view || entry.id < 0 ||
            !cardViewEntityId.IsValid() || !cardViewFrame.IsValid() ||
            !viewMapperEntityModel.IsValid() || !entityExists.IsValid() ||
            !getEntityById.IsValid()) return;
        Obj* mapper = Core::SceneViewContext::RootedMapper();
        if (!mapper || reinterpret_cast<std::uintptr_t>(mapper) != entry.mapper ||
            Core::SceneViewContext::ModelIdentity() != entry.model ||
            reinterpret_cast<std::uintptr_t>(boundFrameModel) != entry.model)
            return;  // Different scene/mapper: never dereference an old pointer.
        Obj* model = viewMapperEntityModel[mapper].Get();
        if (!model || reinterpret_cast<std::uintptr_t>(model) != entry.model ||
            !entityExists[model](entry.id)) return;
        Obj* mapped = nullptr;
        if (!TryResolveMappedCardView(mapper, entry.id, &mapped) ||
            reinterpret_cast<std::uintptr_t>(mapped) != entry.view ||
            std::strcmp(TraceObjectType(mapped), "CardViewComponent") != 0 ||
            cardViewEntityId[mapped].Get() != entry.id) return;
        Obj* entity = getEntityById[model](entry.id);
        if (!entity || !TraceFindComponent(entity, "Card") ||
            !TraceFindComponent(entity, "LaneElement") ||
            TraceFindComponent(entity, "DiscardPileElement") ||
            TraceFindComponent(entity, "HandElement")) return;
        Obj* frame = cardViewFrame[mapped].Get();
        if (!frame) return;
        auto gap = [](int target, int shown) -> int {
            const std::int64_t delta = static_cast<std::int64_t>(target) - shown;
            return delta >= -10000 && delta <= 10000 ? static_cast<int>(delta) : 0;
        };
        int attackDelta = 0, healthDelta = 0, costDelta = 0;
        if (frameCurrentAttack.IsValid() && frameBuffAttack.IsValid() &&
            componentCalculateAttack.IsValid()) {
            Obj* attack = TraceFindComponent(entity, "Attack");
            if (attack) {
                attackDelta = gap(componentCalculateAttack[attack](),
                                  frameCurrentAttack[frame].Get());
                if (attackDelta) frameBuffAttack[frame](attackDelta);
            }
        }
        if (frameMaxHealth.IsValid() && frameCurrentHealth.IsValid() &&
            frameBuffHealth.IsValid() && healthMaxHealth.IsValid() &&
            healthCurrentDamage.IsValid() && calculateModifiableValue.IsValid()) {
            Obj* health = TraceFindComponent(entity, "Health");
            if (health) {
                Obj* value = healthMaxHealth[health].Get();
                if (value) {
                    const int maxHealth = calculateModifiableValue(value);
                    const int damage = healthCurrentDamage[health].Get();
                    const int shownMax = frameMaxHealth[frame].Get();
                    const int shownHealth = frameCurrentHealth[frame].Get();
                    // BuffHealth preserves damage; never convert a damaged
                    // unit into a healed one due to a stale frame.
                    if (maxHealth >= 0 && damage >= 0 && damage <= maxHealth &&
                        shownHealth == shownMax - damage) {
                        healthDelta = gap(maxHealth, shownMax);
                        if (healthDelta) frameBuffHealth[frame](healthDelta);
                    }
                }
            }
        }
        if (frameCurrentSunCost.IsValid() && frameModifySunCost.IsValid() &&
            sunCostValue.IsValid() && calculateModifiableValue.IsValid()) {
            Obj* cost = TraceFindComponent(entity, "SunCost");
            if (cost) {
                Obj* value = sunCostValue[cost].Get();
                if (value) {
                    int finalCost = calculateModifiableValue(value);
                    if (finalCost < 0) finalCost = 0;
                    costDelta = gap(finalCost, frameCurrentSunCost[frame].Get());
                    if (costDelta) frameModifySunCost[frame](costDelta);
                }
            }
        }
        if (attackDelta || healthDelta || costDelta)
            CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/IDLE-STAT: repaired id=%d view=%p attack=%d health=%d cost=%d (both clients; gameplay untouched)",
                     entry.id, mapped, attackDelta, healthDelta, costDelta);
    }

    inline bool QueueContainsAnimation(Obj* queue, const PendingIdleLaneFrame& pending) {
        if (!queue || !pending.animation) return false;
        for (auto* source : {&queueAnimations, &queueFrontAnimations}) {
            if (!source->IsValid()) continue;
            Obj* listObject = (*source)[queue].Get();
            if (!listObject) continue;
            auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
            const int count = list->GetSize();
            if (count < 0 || count > 4096 || !list->items) continue;
            Obj** data = list->GetData();
            if (!data) continue;
            for (int i = 0; i < count; ++i)
                if (reinterpret_cast<std::uintptr_t>(data[i]) == pending.animation)
                    return true;
        }
        return false;
    }

    inline void FlushCompletedIdleLaneFrames(Obj* queue) {
        if (!queue || !queueAnimations.IsValid() || !queueFrontAnimations.IsValid()) return;
        bool hasReady = false;
        for (const auto& entry : pendingIdleLaneFrames)
            if (entry.model && entry.completed &&
                entry.queue == reinterpret_cast<std::uintptr_t>(queue)) {
                hasReady = true;
                break;
            }
        if (!hasReady) return;
        // The stock IsEmpty implementation only checks the normal list in
        // this binary. Also require FrontAnimations to be empty so a queued
        // follow-up buff is not visually pre-applied or duplicated.
        Obj* normalObject = queueAnimations[queue].Get();
        Obj* frontObject = queueFrontAnimations[queue].Get();
        if (!normalObject || !frontObject) return;
        auto* normal = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(normalObject);
        auto* front = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(frontObject);
        if (normal->GetSize() != 0 || front->GetSize() != 0) return;
        for (auto& entry : pendingIdleLaneFrames) {
            if (!entry.model || !entry.completed ||
                entry.queue != reinterpret_cast<std::uintptr_t>(queue)) continue;
            const PendingIdleLaneFrame snapshot = entry;
            entry = {}; // one-shot even when no correction was necessary
            ReconcileIdleLaneFrame(snapshot);
        }
    }

        // Off-board start-record routing and scoped EntityViewMapper lookups.
    #include "presentation/detail/OffBoardViewRouting.inl"
    // =====================================================================
    // Stock lane-creation and board-entry presentation hooks.
    #include "presentation/detail/LaneCardPresentation.inl"

    // Animation-queue observation and post-animation frame flush.
    #include "presentation/detail/AnimationQueueObservation.inl"

        // Scoped timeline start and stock local-hand / CardView fallbacks.
    #include "presentation/detail/OffBoardTimelineHooks.inl"
        // Stock target reactions and source-only visual guards.
    #include "presentation/detail/OffBoardVisualPolicies.inl"
        // Stock asynchronous command loading and completion observations.
    #include "presentation/detail/OffBoardCommandLifecycle.inl"
    // Effect audit / diagnostic stock effect callbacks and registration.
    #include "presentation/detail/OffBoardEffectAudit.inl"

    inline bool Initialize() {
        // Shared stock reveal structure. The Card/Hand/Target refs are the
        // ones owned by the reveal animation, not its later triggered effects.
        auto trickRevealBase = BNM::Class("PvZCards.Game", "RevealOneTimeEffectAnimation");
        auto trickRevealLocal = BNM::Class("PvZCards.Game", "LocalRevealOneTimeEffectAnimation");
        auto trickRevealOpponent = BNM::Class("PvZCards.Game", "OpponentRevealOneTimeEffectAnimation");
        if (trickRevealBase.IsValid()) {
            trickRevealCardId = trickRevealBase.GetField("CardId");
            trickRevealHandId = trickRevealBase.GetField("HandId");
            trickRevealTargetId = trickRevealBase.GetField("TargetId");
            trickRevealCardView = trickRevealBase.GetField("Card");
            trickRevealHandView = trickRevealBase.GetField("Hand");
            trickRevealTargetView = trickRevealBase.GetField("Target");
        }
        if (trickRevealLocal.IsValid()) localRevealCompleteField = trickRevealLocal.GetField("IsComplete");
        if (trickRevealOpponent.IsValid()) opponentRevealCompleteField = trickRevealOpponent.GetField("IsComplete");
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
        viewMapperViewMap = entityViewMapper.GetField("ViewMap");

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
            frameInitialAttack = cardFrameClass.GetField("InitialAttack");
            frameInitialHealth = cardFrameClass.GetField("InitialHealth");
            frameInitialSunCost = cardFrameClass.GetField("InitialSunCost");
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

        // Unconditional installation status: unlike BRIDGETRACE these lines
        // appear even in non-verbose builds. No hook behavior is changed.
        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: INSTALL group=%d factory=%d addAbility=%d modelField=%d groupField=%d (read-only)",
            int(originalTriggerRecordGroup != nullptr),
            int(originalCreateAbilityCore != nullptr),
            int(originalAddAbilityRecord != nullptr),
            int(traceProcessorEntityModel.IsValid()),
            int(traceAbilityGroupId.IsValid()));

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
        // Trick lifecycle hooks: verified ARM64 RVAs from dump.cs. Diagnostics
        // are optional and never replace the stock implementation on failure.
        auto installTrickHook = [&](BNM::Class& klass, const char* name,
            std::uintptr_t rva, TrickRevealVoidFn replacement, TrickRevealVoidFn& original) {
            auto m = klass.IsValid() ? klass.GetMethod(name, 0) : BNM::MethodBase{};
            const bool verified = m.IsValid() && nativeIl2cppBase &&
                static_cast<std::uintptr_t>(m.GetOffset()) == nativeIl2cppBase + rva;
            if (verified) BNM::BasicHook(m, replacement, original);
            if constexpr (false) CC_DIAG("CustomComponents/TRICK-LIFECYCLE-DIAG: INSTALL method=%s expectedRVA=0x%llx verified=%d hooked=%d",
                name, static_cast<unsigned long long>(rva), int(verified), int(original != nullptr));
        };
        installTrickHook(trickRevealLocal, "OnStarted", 0x1F8A480,
            LocalRevealStartCapture, originalLocalRevealStart);
        installTrickHook(trickRevealLocal, "HandleComplete", 0x1F8A97C,
            LocalRevealCompleteTrace, originalLocalRevealComplete);
        installTrickHook(trickRevealOpponent, "OnStarted", 0x1F9C324,
            OpponentRevealStartCapture, originalOpponentRevealStart);
        installTrickHook(trickRevealOpponent, "HandleAnimationComplete", 0x1F9D4D8,
            OpponentRevealCompleteTrace, originalOpponentRevealComplete);
        if constexpr (false) CC_DIAG("CustomComponents/TRICK-LIFECYCLE-DIAG: FIELDS card=%d hand=%d target=%d cardView=%d handView=%d targetView=%d localComplete=%d opponentComplete=%d",
            int(trickRevealCardId.IsValid()), int(trickRevealHandId.IsValid()),
            int(trickRevealTargetId.IsValid()), int(trickRevealCardView.IsValid()),
            int(trickRevealHandView.IsValid()), int(trickRevealTargetView.IsValid()),
            int(localRevealCompleteField.IsValid()), int(opponentRevealCompleteField.IsValid()));


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
            laneAnimationViewMapper = laneCreateClass.GetField("<ViewMap>k__BackingField");
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
        CUSTOMCOMPONENTS_LANE_TRACE(
            "CustomComponents/ZoneTriggeredAbilityPresentation/LANEENTRY: hooks factory=%d preload=%d start=%d show=%d completed=%d addToLane=%d playedToBoard=%d (read-only; RVA verified)",
            int(originalLaneCreateCardEvent != nullptr), int(originalLanePreload != nullptr),
            int(originalLaneCreateStarted != nullptr), int(originalLaneShowCard != nullptr),
            int(originalLaneCompleted != nullptr), int(originalLaneAddToLane != nullptr),
            int(originalLanePlayedToBoard != nullptr));

        if constexpr (false) CC_DIAG("CustomComponents/DECK-HANG-DIAG: INSTALL onStarted=%d native=%p (read-only)",
            int(originalTriggeredAbilityOnStarted != nullptr),
            reinterpret_cast<void*>(triggeredOnStarted.GetOffset()));

        InstallOffBoardAudit();

        CC_DIAG(
            "CustomComponents/ZoneTriggeredAbilityPresentation: ready (BRIDGEONLY: explicit custom trigger required; BRIDGETRACE retained; PARTICLEFORWARD retained)"
        );

        return true;
    }

} // namespace CustomComponents::Systems::ZoneTriggeredAbilityPresentationSystem

#undef CUSTOMCOMPONENTS_LANE_TRACE
#undef CUSTOMCOMPONENTS_ZTAP_TRACE
