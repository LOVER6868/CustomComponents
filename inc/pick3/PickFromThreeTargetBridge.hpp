#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Step 2: opt-in READ-ONLY capture of stock eligibility for a ChooseTarget
// blocking phase. No Play, SelectTargetEffect, TargetId or queue is modified.
// The popup reports the chosen entity; it DOES NOT submit that choice to the
// game's SelectTargetService yet. This separation is crucial for PvP safety.
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <utility>
#include <vector>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Exceptions.hpp>
#include "log.h"
#include "core/SceneViewContext.hpp"
#include "pick3/PickFromThreeCore.hpp"
#include "presentation/detail/Pick3SpriteCache.hpp"

namespace CustomComponents::Pick3::TargetBridge {
using Obj = BNM::IL2CPP::Il2CppObject;
template<typename T> using List = BNM::Structures::Mono::List<T>;
inline constexpr int kPickFromThreeValue = EnumModes::kPickFromThree;
inline constexpr int kManualValue = EnumModes::kStockManual;
inline bool enabled = false;
inline bool normalizeGameplayFilter = false;
inline BNM::Field<int> selectionType, blockingEffectEntityId, blockingGroupId;
inline BNM::Field<int> blockingSourceId; // Effect.SourceId of activating card
inline BNM::Field<int> blockingPlayerId; // Effect.PlayerId is the choosing player
// The submission service supplies the locally-injected GameModel.LocalPlayerId.
// No extra hooks are necessary and the existing PvE path stays intact.
inline int (*resolveLocalPlayerId)() = nullptr;
inline int LocalPlayerId() { return resolveLocalPlayerId ? resolveLocalPlayerId() : -1; }
// Provided by the game's selected target service capture; no separate hook.
inline bool (*resolveRemoteSession)() = nullptr;
inline BNM::Field<Obj*> finderEntityFinder, entityFinderModel;
inline BNM::Method<Obj*> getEntityById;
inline BNM::Method<Obj*> getComponent;
inline BNM::Method<int> cardGuid;
inline BNM::Class primaryFilterClass, cardClass, cardDatabaseClass;
inline BNM::Class finderClass, entityFinderClass, entityModelClass, blockingEffectClass;
inline BNM::Method<Obj*> getDatabaseInstance;
inline BNM::Method<Obj*> findDatabaseEntities;
// Optional match-bound instance provider supplied by the proven Submission
// command-binder capture. Prefer it to constructing an empty standalone DB.
inline Obj* (*resolveMatchDatabase)() = nullptr;
inline BNM::Field<Obj*> targetFilterQuery, additionalTargetQuery;
inline BNM::Field<int> additionalTargetType;
inline BNM::Field<bool> onlyApplyToAdditionalTargets;
inline BNM::Field<int> databaseCardGuid;
// Auto-resolved stock recipient bridge. This ID is NEVER a user choice and
// NEVER a CardDatabase GUID; the three-card popup is the only choice UI.
// Entity ID 0 may be valid (it is not the kNoEntity=-1 sentinel).
inline int latestDatabaseProxy = -1;
inline bool ValidateAutomaticRecipient(Obj* model, int entityId) {
    if (!model || entityId < 0 || !getEntityById.IsValid()) return false;
    Obj* entity = nullptr;
    const auto ex = BNM::TryInvoke([&]{ entity = getEntityById[model](entityId); });
    return !ex.IsValid() && entity != nullptr;
}

// Use the game's EntityQuery itself against the backing CardDatabase model.
// The result is a managed List<Entity>, not a live match entity list. The
// subtype/has-component/not/composite JSON is evaluated by the game here.
// Do not infer database GUIDs from a numeric range or from a live deck.
inline std::vector<Candidate> EnumerateCardDatabase(Obj* query, int effectId) {
    std::vector<Candidate> candidates;
    if (!query || !getDatabaseInstance.IsValid() || !findDatabaseEntities.IsValid())
        return candidates;
    Obj* database = resolveMatchDatabase ? resolveMatchDatabase() : nullptr;
    const bool injected = database && BNM::IsA(database, cardDatabaseClass);
    if (!injected) {
        database = nullptr;
        const auto getEx = BNM::TryInvoke([&] { database = getDatabaseInstance(); });
        if (getEx.IsValid() || !database || !BNM::IsA(database, cardDatabaseClass)) {
            LOG_ERR("CustomComponents/Pick3/DB5: INSTANCE_UNAVAILABLE effect=%d bound=0 fallback=0", effectId);
            return candidates;
        }
    }
    CC_DIAG("CustomComponents/Pick3/DB5: DATABASE_SOURCE effect=%d bound=%d", effectId, injected ? 1 : 0);
    Obj* found = nullptr;
    auto findEx = BNM::TryInvoke([&] { found = findDatabaseEntities[database](query); });
    if (findEx.IsValid() || !found) {
        LOG_ERR("CustomComponents/Pick3/DB5: QUERY_EXCEPTION effect=%d", effectId);
        return candidates;
    }
    auto* matches = reinterpret_cast<List<Obj*>*>(found);
    if (matches->size < 0 || matches->size > 10000 ||
        (matches->size > 0 && !matches->items)) {
        LOG_ERR("CustomComponents/Pick3/DB5: INVALID_RESULTS effect=%d", effectId);
        return candidates;
    }
    candidates.reserve(static_cast<std::size_t>(matches->size));
    for (int i = 0; i < matches->size; ++i) {
        Obj* entity = matches->items->m_Items[i];
        if (!entity) continue;
        Obj* card = nullptr;
        auto componentEx = BNM::TryInvoke([&] {
            card = getComponent[entity](cardClass.GetMonoType());
        });
        if (componentEx.IsValid() || !card) continue;
        const int guid = databaseCardGuid[card].Get();
        if (guid > 0) candidates.push_back({kNoEntity, guid});
    }
    CC_DIAG("CustomComponents/Pick3/DB5: QUERY_MATCHES effect=%d matched=%d usable=%zu (filtered by JSON Query)",
             effectId, matches->size, candidates.size());
    return candidates;
}


using FinderFn = Obj* (*)(void*, void*, const void*);
using ShouldPrimaryFn = bool (*)(void*, void*, void*, const void*);
using ShouldSecondaryFn = bool (*)(void*, int, void*, void*, const void*);
inline FinderFn originalFindValid = nullptr;
inline ShouldPrimaryFn originalShouldPrimary = nullptr;
inline ShouldSecondaryFn originalShouldSecondary = nullptr;
inline std::mutex snapshotMutex;
inline PreviewRequest nextRequest;
// The current PlayCardCommand gesture is the immediate play's actual card.
// SourceId can instead refer to a nested effect's intermediary entity.
// This is a UI-localization hint ONLY; no targeting or gameplay state changes.
inline int lastPlayedSourceEntity=-1, lastPlayedCardGuid=-1;
inline std::chrono::steady_clock::time_point lastPlayedAt{};
inline void RememberPlayedSource(int entityId,int guid) {
    if(entityId<0 || guid<=0)return;
    lastPlayedSourceEntity=entityId;lastPlayedCardGuid=guid;
    lastPlayedAt=std::chrono::steady_clock::now();
}

inline bool requestReady = false;
inline std::uint32_t nonce = 0;
inline int latestEffect = -1, latestGroup = -1;
inline int latestOwnerPlayerId = -1;
inline std::vector<int> latestEligible;
// Preserve exact entity/GUID pairs from the *stock-valid* target list.
// Weighted choices must never quietly turn an entity selection into a
// different copy of the same card definition.
inline std::vector<Candidate> latestEligiblePairs;
inline BNM::Field<int> numberOfTargets;
inline int lastOfferedEffect = -1;
inline int lastOfferedGroup = -1;
// Custom enums (3/4/5) must only be mapped to stock Manual (1) while
// the specific choose-target transaction is outstanding. Without restoring
// the original value, a recycled filter or a subsequent activation can be
// interpreted as an ordinary stock manual target selection.
struct RuntimeManualOverride {
    int effectId;
    int groupId;
    int originalSelectionType;
    std::uint32_t filterHandle;
};
inline std::vector<RuntimeManualOverride> runtimeOverrides;
// SelectionType override and Query override have separate lifetimes. The
// database target's original Query is rooted while the stock engine briefly
// targets the recipient(s) selected by AdditionalTargetQuery. No JSON mutation.
struct DatabaseQueryOverride {
    int effectId, groupId;
    std::uint32_t filterRoot, queryRoot;
};
inline std::vector<DatabaseQueryOverride> databaseQueryOverrides;
inline void RestoreDatabaseOverride(const DatabaseQueryOverride& item, const char* reason) {
    Obj* filter = Core::SceneViewContext::getGCHandle(item.filterRoot);
    Obj* originalQuery = Core::SceneViewContext::getGCHandle(item.queryRoot);
    if (filter && originalQuery && targetFilterQuery.IsValid())
        targetFilterQuery[filter].Set(originalQuery);
    if (item.queryRoot) Core::SceneViewContext::freeGCHandle(item.queryRoot);
    if (item.filterRoot) Core::SceneViewContext::freeGCHandle(item.filterRoot);
    CC_DIAG("CustomComponents/Pick3/DB5: QUERY_RESTORE effect=%d group=%d reason=%s success=%d",
             item.effectId, item.groupId, reason, filter && originalQuery ? 1 : 0);
}
inline void CancelDatabaseOverride(int effectId, int groupId, const char* reason) {
    for (auto it = databaseQueryOverrides.begin(); it != databaseQueryOverrides.end();) {
        if (it->effectId == effectId && it->groupId == groupId) {
            RestoreDatabaseOverride(*it, reason);
            it = databaseQueryOverrides.erase(it);
        } else ++it;
    }
}
inline Obj* OriginalDatabaseQuery(Obj* filter, int effectId, int groupId) {
    for (const auto& entry : databaseQueryOverrides) {
        if (entry.effectId == effectId && entry.groupId == groupId &&
            Core::SceneViewContext::getGCHandle(entry.filterRoot) == filter)
            return Core::SceneViewContext::getGCHandle(entry.queryRoot);
    }
    return filter && targetFilterQuery.IsValid() ? targetFilterQuery[filter].Get() : nullptr;
}
inline bool RedirectDatabaseTargetQuery(Obj* filter, int effectId, int groupId) {
    if (!filter || !targetFilterQuery.IsValid() ||
        !additionalTargetQuery.IsValid() || !additionalTargetType.IsValid() ||
        !onlyApplyToAdditionalTargets.IsValid() ||
        additionalTargetType[filter].Get() != 1 ||
        !onlyApplyToAdditionalTargets[filter].Get()) return false;
    for (const auto& entry : databaseQueryOverrides)
        if (entry.effectId == effectId && entry.groupId == groupId &&
            Core::SceneViewContext::getGCHandle(entry.filterRoot) == filter)
            return true;
    if (databaseQueryOverrides.size() >= 8) return false;
    Obj* original = targetFilterQuery[filter].Get();
    Obj* recipientQuery = additionalTargetQuery[filter].Get();
    if (!original || !recipientQuery || original == recipientQuery) return false;
    auto root = Core::SceneViewContext::newGCHandle;
    auto release = Core::SceneViewContext::freeGCHandle;
    if (!root || !release || !Core::SceneViewContext::getGCHandle) return false;
    std::uint32_t filterRoot = root(filter, false);
    std::uint32_t queryRoot = root(original, false);
    if (!filterRoot || !queryRoot) {
        if (filterRoot) release(filterRoot);
        if (queryRoot) release(queryRoot);
        return false;
    }
    databaseQueryOverrides.push_back({effectId, groupId, filterRoot, queryRoot});
    targetFilterQuery[filter].Set(recipientQuery);
    CC_DIAG("CustomComponents/Pick3/DB5: QUERY_REDIRECT effect=%d group=%d (database eligibility preserved; stock recipient proxy)",
             effectId, groupId);
    return true;
}

// Must be called with snapshotMutex locked. A GC handle is essential because
// the native click arrives on a later Unity frame (and an IL2CPP GC may run).
inline bool RememberAndNormalize(Obj* filter, int effectId, int groupId) {
    if (!filter || !Core::SceneViewContext::newGCHandle ||
        !Core::SceneViewContext::getGCHandle || !Core::SceneViewContext::freeGCHandle)
        return false;
    if (runtimeOverrides.size() >= 8) return false;
    const std::uint32_t handle = Core::SceneViewContext::newGCHandle(filter, false);
    if (!handle) return false;
    runtimeOverrides.push_back({effectId, groupId, selectionType[filter].Get(), handle});
    selectionType[filter].Set(kManualValue);
    return true;
}
// Must be called with snapshotMutex locked. Never change an unrelated or
// already-repurposed value: restore only our 1 -> original (3/4/5) transition.
inline void RestoreOverride(const RuntimeManualOverride& item, const char* reason) {
    Obj* filter = Core::SceneViewContext::getGCHandle
        ? Core::SceneViewContext::getGCHandle(item.filterHandle) : nullptr;
    int before = -1;
    bool restored = false;
    if (filter && primaryFilterClass.IsValid() && BNM::IsA(filter, primaryFilterClass)) {
        before = selectionType[filter].Get();
        if (before == kManualValue) {
            selectionType[filter].Set(item.originalSelectionType);
            restored = true;
        }
    }
    if (item.filterHandle && Core::SceneViewContext::freeGCHandle)
        Core::SceneViewContext::freeGCHandle(item.filterHandle);
    CC_DIAG("CustomComponents/Pick3/STEP411: FILTER_RESTORE reason=%s effect=%d group=%d before=%d original=%d restored=%d",
             reason, item.effectId, item.groupId, before, item.originalSelectionType, restored ? 1 : 0);
}
// Called after the synchronous local service has processed the selected
// targets, before a future card can start another ChooseTarget phase.
inline void CompleteLocalSelection(int effectId, int groupId, bool rearm = true) {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    for (auto it = runtimeOverrides.begin(); it != runtimeOverrides.end();) {
        if (it->effectId == effectId && it->groupId == groupId) {
            RestoreOverride(*it, "local-service-complete");
            it = runtimeOverrides.erase(it);
        } else ++it;
    }
    for (auto it = databaseQueryOverrides.begin(); it != databaseQueryOverrides.end();) {
        if (it->effectId == effectId && it->groupId == groupId) {
            RestoreDatabaseOverride(*it, "local-service-complete");
            it = databaseQueryOverrides.erase(it);
        } else ++it;
    }
    if (rearm && lastOfferedEffect == effectId && lastOfferedGroup == groupId) {
        lastOfferedEffect = -1;
        lastOfferedGroup = -1;
    }
    if (latestEffect == effectId && latestGroup == groupId) {
        latestEffect = latestGroup = -1;
        latestOwnerPlayerId = -1;
        latestEligible.clear(); latestEligiblePairs.clear(); latestDatabaseProxy = -1;
    }
    CC_DIAG("CustomComponents/Pick3/STEP411: REARM effect=%d group=%d overridesRemaining=%zu",
             effectId, groupId, runtimeOverrides.size());
}

// The game must create a stock blocking selection for our new enum. When
// checking if a manual choose phase is required, interpret 3 as Manual=1 only
// for the duration of this original call. No targets are selected here.
static bool ShouldPrimaryHook(void* self, void* filter, void* candidates, const void* method) {
    if (!originalShouldPrimary) return false;
    if (!enabled || !filter || !EnumModes::IsPickFromThree(selectionType[filter].Get()))
        return originalShouldPrimary(self, filter, candidates, method);
    const int originalValue = selectionType[filter].Get();
    selectionType[filter].Set(kManualValue);
    const bool value = originalShouldPrimary(self, filter, candidates, method);
    selectionType[filter].Set(originalValue);
    if (originalValue == EnumModes::kPickFromThreeCardDatabase) {
        const bool valid = numberOfTargets.IsValid() && numberOfTargets[filter].Get() == 1 &&
            additionalTargetType.IsValid() && additionalTargetType[filter].Get() == 1 &&
            onlyApplyToAdditionalTargets.IsValid() && onlyApplyToAdditionalTargets[filter].Get() &&
            additionalTargetQuery.IsValid() && additionalTargetQuery[filter].Get() &&
            targetFilterQuery.IsValid() && targetFilterQuery[filter].Get();
        CC_DIAG("CustomComponents/Pick3/DB5: PRIMARY_PHASE stock=%d settingsValid=%d database=1",
                 value ? 1 : 0, valid ? 1 : 0);
        return valid; // No live Fruit target is needed to show database choices.
    }
    CC_DIAG("CustomComponents/Pick3/STEP2: primary manual-block check=%d", value ? 1 : 0);
    return value;
}
static bool ShouldSecondaryHook(void* self, int primary, void* filter, void* candidates, const void* method) {
    if (!originalShouldSecondary) return false;
    if (!enabled || !filter || !EnumModes::IsPickFromThree(selectionType[filter].Get()))
        return originalShouldSecondary(self, primary, filter, candidates, method);
    const int originalValue = selectionType[filter].Get();
    selectionType[filter].Set(kManualValue);
    const bool value = originalShouldSecondary(self, primary, filter, candidates, method);
    selectionType[filter].Set(originalValue);
    return value;
}

static Obj* FindValidHook(void* self, void* blocking, const void* method) {
    if (!originalFindValid) return nullptr;
    if (!enabled || !self || !blocking ||
        !finderClass.IsValid() || !blockingEffectClass.IsValid() ||
        !entityFinderClass.IsValid() || !entityModelClass.IsValid() ||
        !getEntityById.IsValid() || !getComponent.IsValid() ||
        !blockingEffectEntityId.IsValid() || !blockingGroupId.IsValid() ||
        !finderEntityFinder.IsValid() || !entityFinderModel.IsValid() ||
        !primaryFilterClass.IsValid() || !selectionType.IsValid())
        return originalFindValid(self, blocking, method);

    // Avoid recursively re-entering BNM's managed lookup via a nested stock
    // FindValidTargetsFromEffect call. Always preserve stock validation.
    static thread_local bool inspecting = false;
    if (inspecting) return originalFindValid(self, blocking, method);
    struct InspectionScope {
        bool& value;
        explicit InspectionScope(bool& v) : value(v) { value = true; }
        ~InspectionScope() { value = false; }
    } inspection(inspecting);

    Obj* blockingObject = static_cast<Obj*>(blocking);
    Obj* finderObject = static_cast<Obj*>(self);
    if (!BNM::IsA(blockingObject, blockingEffectClass) ||
        !BNM::IsA(finderObject, finderClass))
        return originalFindValid(self, blocking, method);
    const int effectId = blockingEffectEntityId[blockingObject].Get();
    const int groupId = blockingGroupId[blockingObject].Get();
    if (effectId <= 0) return originalFindValid(self, blocking, method);

    // Both PvP clients simulate the blocking Effect, but only Effect.PlayerId
    // owns its selection UI. The older bridge queued a popup on BOTH clients.
    // Use the engine's recorded owner, not VisualTurnPhaseModel.SubTurn: that
    // visual value can change during entrance/press animations.
    const int effectOwner = blockingPlayerId.IsValid()
        ? blockingPlayerId[blockingObject].Get() : -1;
    const int thisPlayer = LocalPlayerId();
    if (effectOwner >= 0 && thisPlayer >= 0 && effectOwner != thisPlayer) {
        static int lastLoggedEffect = -1, lastLoggedGroup = -1;
        if (lastLoggedEffect != effectId || lastLoggedGroup != groupId) {
            lastLoggedEffect = effectId; lastLoggedGroup = groupId;
            LOG_INFO("CustomComponents/Pick3/PVP_OWNER: REMOTE_POPUP_SUPPRESSED effect=%d group=%d owner=%d local=%d",
                     effectId, groupId, effectOwner, thisPlayer);
        }
        // Leave the simulation's original finder and targets unmodified.
        return originalFindValid(self, blocking, method);
    }

    // Stock asks for valid targets repeatedly, including after the popup has
    // been offered. An already-owned effect/group must not be resolved again
    // through an extra managed EntityModel.GetEntityById invocation.
    // CompleteLocalSelection resets this key for the next transaction.
    bool previouslyOffered = false;
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        previouslyOffered = lastOfferedEffect == effectId && lastOfferedGroup == groupId;
    }
    if (previouslyOffered) return originalFindValid(self, blocking, method);
    Obj* finderInstance = finderEntityFinder[finderObject].Get();
    if (!finderInstance || !BNM::IsA(finderInstance, entityFinderClass))
        return originalFindValid(self, blocking, method);
    Obj* model = entityFinderModel[finderInstance].Get();
    if (!model || !BNM::IsA(model, entityModelClass))
        return originalFindValid(self, blocking, method);
    Obj* effectEntity = nullptr;
    auto findEx = BNM::TryInvoke([&] { effectEntity = getEntityById[model](effectId); });
    if (findEx.IsValid() || !effectEntity) return originalFindValid(self, blocking, method);
    Obj* filter = nullptr;
    auto componentEx = BNM::TryInvoke([&] {
        filter = getComponent[effectEntity](primaryFilterClass.GetMonoType());
    });
    if (componentEx.IsValid() || !filter ||
        !BNM::IsA(filter, primaryFilterClass) ||
        !EnumModes::IsPickFromThree(selectionType[filter].Get()))
        return originalFindValid(self, blocking, method);
    const int jsonSelectionType = selectionType[filter].Get();
    const SelectionMode jsonMode = EnumModes::Resolve(jsonSelectionType, SelectionMode::RandomTargets);
    Obj* databaseQuery = nullptr;
    if (jsonMode == SelectionMode::CardDatabase) {
        if (!RedirectDatabaseTargetQuery(filter, effectId, groupId)) {
            LOG_ERR("CustomComponents/Pick3/DB5: RECIPIENT_QUERY_UNAVAILABLE effect=%d (requires AdditionalTargetType=Query, OnlyApplyEffectsOnAdditionalTargets=true)", effectId);
            return originalFindValid(self, blocking, method);
        }
        databaseQuery = OriginalDatabaseQuery(filter, effectId, groupId);
    }
    // Read-only boundary diagnostic: the call below may already apply stock
    // target count/randomization. This records the filter state before it runs.
    if (jsonMode == SelectionMode::UniqueGuidTargets) {
        LOG_INFO("CustomComponents/Pick3/POOL_BOUNDARY: BEFORE_STOCK effect=%d group=%d mode=%d numTargets=%d lastOffered=%d requestReady=%d overrideCount=%zu",
                 effectId, groupId, jsonSelectionType,
                 numberOfTargets.IsValid() ? numberOfTargets[filter].Get() : -1,
                 previouslyOffered ? 1 : 0, requestReady ? 1 : 0,
                 runtimeOverrides.size());
    }
    // Stock now evaluates the recipient query for its legal phase-completion
    // entity; the original CardDatabase candidate query is still GC-rooted.
    Obj* result = originalFindValid(self, blocking, method);
    if (!result) {
        if (jsonMode == SelectionMode::CardDatabase)
            CancelDatabaseOverride(effectId, groupId, "finder-null");
        return result;
    }
    // The game's NumTargets is the number to SELECT, not the three UI options.
    // The selection supports only 1..3 distinct targets in a single Play.
    const int required = numberOfTargets.IsValid() ? numberOfTargets[filter].Get() : -1;
    if (required < 1 || required > 3 ||
        (jsonMode == SelectionMode::CardDatabase && required != 1)) {
        CC_DIAG("CustomComponents/Pick3/STEP2: MODE_UNSUPPORTED NumTargets=%d effect=%d", required, effectId);
        if (jsonMode == SelectionMode::CardDatabase)
            CancelDatabaseOverride(effectId, groupId, "target-count");
        return result;
    }

    auto* source = reinterpret_cast<List<int>*>(result);
    if (!source->items || source->size < 0 || source->size > 4096) {
        if (jsonMode == SelectionMode::CardDatabase)
            CancelDatabaseOverride(effectId, groupId, "invalid-recipient-list");
        return result;
    }
    std::vector<Candidate> candidates;
    // OnlyApplyEffectsOnAdditionalTargets means the stock target is solely a
    // legal completion/recipient bridge. It is never a displayed DB option.
    if (jsonMode == SelectionMode::CardDatabase) {
        // IMPORTANT: the first Plants+Player entity can legitimately have ID
        // zero. We must check existence in EntityModel, not require id > 0.
        // Exactly one is required for this experimental PvE completion bridge;
        // it is selected automatically, never offered to the player.
        const int autoRecipient = source->size == 1 ? source->items->m_Items[0] : kNoEntity;
        const bool recipientValid = source->size == 1 &&
            ValidateAutomaticRecipient(model, autoRecipient);
        CC_DIAG("CustomComponents/Pick3/DB5: AUTO_RECIPIENT effect=%d count=%d entityId=%d valid=%d (no user targeting)",
                 effectId, source->size, autoRecipient, recipientValid ? 1 : 0);
        if (!recipientValid) {
            LOG_ERR("CustomComponents/Pick3/DB5: AUTO_RECIPIENT_UNAVAILABLE effect=%d count=%d id=%d",
                    effectId, source->size, autoRecipient);
            CancelDatabaseOverride(effectId, groupId, "no-auto-recipient");
            return result;
        }
        candidates = EnumerateCardDatabase(databaseQuery, effectId);
        if (candidates.size() < 3) {
            LOG_ERR("CustomComponents/Pick3/DB5: DATABASE_QUERY_EMPTY effect=%d entries=%zu",
                    effectId, candidates.size());
            CancelDatabaseOverride(effectId, groupId, "database-query-empty");
            return result;
        }
    } else {
    candidates.reserve(static_cast<std::size_t>(source->size));
    for (int i = 0; i < source->size; ++i) {
        const int id = source->items->m_Items[i];
        if (id <= 0) continue;
        Obj* target = nullptr;
        auto targetEx = BNM::TryInvoke([&] { target = getEntityById[model](id); });
        if (targetEx.IsValid() || !target) continue;
        int guid = -1;
        const auto ex = BNM::TryInvoke([&] { guid = cardGuid(target); });
        // The Step 2 popup knows how to display cards, not non-card lane/hero
        // targets. Leave those in the stock list; exclude them from the probe.
        if (!ex.IsValid() && guid > 0) candidates.push_back({id, guid});
    }
    }
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        latestEffect = effectId; latestGroup = blockingGroupId[blocking].Get();
        latestOwnerPlayerId = effectOwner;
        latestEligible.clear();
        latestEligiblePairs.clear();
        latestDatabaseProxy = -1;
        if (jsonMode == SelectionMode::CardDatabase) {
            latestDatabaseProxy = source->items->m_Items[0];
            latestEligible.push_back(latestDatabaseProxy);
        } else {
            latestEligiblePairs = candidates;
            for (const auto& candidate : candidates) latestEligible.push_back(candidate.entityId);
        }
    }
    // The database popup displays the original JSON Query's backing GUIDs.
    // The stock list contains only legal recipient proxy targets, never GUIDs.
    // Do not modify the PoolableList or stock selector's RNG stream.
    // The query can run BEFORE SelectTargetCommand injects its service.
    // Choosing the sampler based on Remote() at this point makes one peer use
    // a local ++nonce while the other uses canonical ordering, even with
    // identical eligible states. Use a canonical, nonce-free offer for BOTH
    // local and remote completion. Different effect/group keys still produce
    // different offers; the same pending effect never silently rerolls.
    // This stabilizes client previews, not server-side GUID transmission.
    if (jsonMode == SelectionMode::UniqueGuidTargets) {
        LOG_INFO("CustomComponents/Pick3/POOL_BOUNDARY: AFTER_STOCK effect=%d group=%d returned=%d resolved=%zu required=%d",
                 effectId, groupId, source->size, candidates.size(), required);
        std::unordered_map<int, int> counts;
        std::unordered_set<int> uniqueEntities;
        for (const auto& card : candidates)
            if (card.entityId > 0 && card.cardGuid > 0 && uniqueEntities.insert(card.entityId).second)
                ++counts[card.cardGuid];
        LOG_INFO("CustomComponents/Pick3/WEIGHTED_POOL: effect=%d group=%d raw=%d valid=%zu distinctEntities=%zu guidStacks=%zu targetSlots=3",
                 effectId, blockingGroupId[blocking].Get(), source->size, candidates.size(),
                 uniqueEntities.size(), counts.size());
        // Log the post-stock GUID distribution so a stock-side truncation is
        // distinguishable from duplicate-GUID removal in our sampler.
        for (const auto& count : counts)
            LOG_INFO("CustomComponents/Pick3/POOL_STACK: effect=%d group=%d guid=%d eligibleCopies=%d",
                     effectId, groupId, count.first, count.second);
        // A pool with only two distinct eligible GUIDs cannot provide three
        // different GUIDs. Never invent targets or duplicate an offered GUID.
        if (counts.size() < kMaximumChoices)
            LOG_INFO("CustomComponents/Pick3/WEIGHTED_POOL: SHORT_POOL effect=%d availableGuidStacks=%zu",
                     effectId, counts.size());
    }
    const bool networkSelection = resolveRemoteSession && resolveRemoteSession();
    auto request = MakeStableNetworkPreviewRequest(
        effectId, blockingGroupId[blocking].Get(), jsonMode, candidates, required);
    CC_DIAG("CustomComponents/Pick3/PVP: CANONICAL_OPTIONS effect=%d group=%d count=%zu serviceRemote=%d; "
            "server validation and remote GUID transport still required",
            effectId, request.effectGroupId, request.options.size(), networkSelection ? 1 : 0);
    if (jsonMode == SelectionMode::UniqueGuidTargets) {
        LOG_INFO("CustomComponents/Pick3/WEIGHTED_OFFER: effect=%d group=%d offered=%zu first=%d second=%d third=%d",
                 effectId, request.effectGroupId, request.options.size(),
                 request.options.size() > 0 ? request.options[0].cardGuid : -1,
                 request.options.size() > 1 ? request.options[1].cardGuid : -1,
                 request.options.size() > 2 ? request.options[2].cardGuid : -1);
    }
    request.ownerPlayerId = effectOwner;
    // The instruction localization belongs to the card playing this ability.
    // Resolve while the originating Effect and its GameModel are still live;
    // a GUI lookup from one of the offered candidate cards would be incorrect.
    if (blockingSourceId.IsValid()) {
        const int sourceId = blockingSourceId[static_cast<Obj*>(blocking)].Get();
        if (sourceId >= 0) {
            Obj* source = nullptr;
            const auto sourceEx = BNM::TryInvoke([&] { source = getEntityById[model](sourceId); });
            if (!sourceEx.IsValid() && source) {
                int guid = -1;
                const auto guidEx = BNM::TryInvoke([&] { guid = cardGuid(source); });
                if (!guidEx.IsValid() && guid > 0) request.sourceCardGuid = guid;
            }
        }
    }
    // Prefer the card that initiated the current play, but only for the
    // immediate effect created by that play (not later trigger effects).
    // Diagnostic provenance allows verifying source vs intermediary IDs.
    const auto recentAge = std::chrono::steady_clock::now()-lastPlayedAt;
    if (lastPlayedCardGuid>0 && recentAge < std::chrono::milliseconds(1500)) {
        LOG_INFO("CustomComponents/Pick3/UI: SOURCE_PROVENANCE effect=%d sourceEntity=%d effectGuid=%d playedEntity=%d playedGuid=%d",
                effectId, blockingSourceId.IsValid()?blockingSourceId[static_cast<Obj*>(blocking)].Get():-1,
                request.sourceCardGuid,lastPlayedSourceEntity,lastPlayedCardGuid);
        request.sourceCardGuid=lastPlayedCardGuid;
    }
    else LOG_INFO("CustomComponents/Pick3/UI: SOURCE_PROVENANCE effect=%d effectGuid=%d played=stale",
                 effectId,request.sourceCardGuid);
    if (request.options.size() < static_cast<std::size_t>(required)) {
        LOG_ERR("CustomComponents/Pick3/STEP2: insufficient offered targets required=%d actual=%zu effect=%d",
                required, request.options.size(), effectId);
        if (jsonMode == SelectionMode::CardDatabase)
            CancelDatabaseOverride(effectId, groupId, "insufficient-options");
        return result;
    }
    CC_DIAG("CustomComponents/Pick3/STEP2: valid=%d resolved=%zu sampled=%zu required=%d effect=%d selectionType=%d mode=%d",
             source->size, candidates.size(), request.options.size(), required, effectId, jsonSelectionType, static_cast<int>(jsonMode));
    bool offered = false;
    if (!request.options.empty()) {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        // Stock validation can query the same blocking effect repeatedly.
        if (!requestReady && (lastOfferedEffect != effectId ||
                             lastOfferedGroup != request.effectGroupId)) {
            // Make a rooted, reversible 3/4/5 -> 1 runtime override BEFORE
            // publishing the popup. A permanent conversion is not safe for
            // repeated card activations within the same match.
            if (normalizeGameplayFilter &&
                !RememberAndNormalize(filter, effectId, request.effectGroupId)) {
                LOG_ERR("CustomComponents/Pick3/STEP411: FILTER_ROOT_FAILED effect=%d group=%d; popup not offered",
                        effectId, request.effectGroupId);
                if (jsonMode == SelectionMode::CardDatabase)
                    CancelDatabaseOverride(effectId, groupId, "filter-root-failed");
                return result;
            }
            lastOfferedEffect = effectId;
            lastOfferedGroup = request.effectGroupId;
            nextRequest = std::move(request);
            requestReady = true;
            offered = true;
        }
    }
    if (offered && normalizeGameplayFilter) {
        CC_DIAG("CustomComponents/Pick3/STEP2: RUNTIME_FILTER_MANUAL effect=%d group=%d required=%d",
                 effectId, blockingGroupId[blocking].Get(), required);
        CC_DIAG("CustomComponents/Pick3/ENUM: FILTER_NORMALIZED effect=%d group=%d from=%d to=1 (rooted)",
                 effectId, blockingGroupId[blocking].Get(), jsonSelectionType);
    }
    return result;
}

// A queued popup already owns the manual choice, even before UGUI has had
// its first Update(). Never let stock target clicks beat the popup onscreen.
// Only a DB5 activation redirects targeting to a non-interactive recipient.
// Presentation may suppress target icon highlights while this scoped override
// is live; never infer it from the JSON query or from an arbitrary hero ID.
inline bool HasAutomaticDatabaseRecipientPhase() {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return !databaseQueryOverrides.empty();
}
inline int ActiveOwnerPlayerId() {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return latestOwnerPlayerId;
}
inline bool HasQueuedChoice() {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return requestReady;
}
// Only resolve the additional recipient AFTER a user has picked a card.
// This is not a general auto-target rule: ordinary Pick3 selections never
// inspect or submit additional targets. More than one (or zero) eligible
// recipients means there is no automatic choice.
inline int SoleDatabaseRecipientAfterCardChoice(int effectId,int groupId) {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    if (latestEffect!=effectId || latestGroup!=groupId ||
        latestEligible.size()!=1 || latestEligible[0]!=latestDatabaseProxy)
        return kNoEntity;
    return latestDatabaseProxy;
}
inline bool StillEligible(int effectId, int groupId, int entityId) {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return latestEffect == effectId && latestGroup == groupId &&
        std::find(latestEligible.begin(), latestEligible.end(), entityId) != latestEligible.end();
}
// An offered weighted choice is an exact LIVE CARD, not merely its GUID.
// This is checked immediately before the DTO is constructed. The snapshot is
// not a substitute for the server's current-state target validation.
inline bool StillEligibleWithGuid(int effectId, int groupId, int entityId, int guid) {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    if (latestEffect != effectId || latestGroup != groupId || entityId <= 0 || guid <= 0)
        return false;
    return std::any_of(latestEligiblePairs.begin(), latestEligiblePairs.end(),
        [=](const Candidate& item) { return item.entityId == entityId && item.cardGuid == guid; });
}
inline bool Consume(PreviewRequest& out) {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    if (!requestReady) return false;
    out = std::move(nextRequest);
    nextRequest = PreviewRequest{};
    requestReady = false;
    return true;
}
inline void Clear() {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    for (const auto& entry : runtimeOverrides) RestoreOverride(entry, "match-reset");
    runtimeOverrides.clear();
    for (const auto& entry : databaseQueryOverrides) RestoreDatabaseOverride(entry, "match-reset");
    databaseQueryOverrides.clear();
    nextRequest = PreviewRequest{};
    requestReady = false;
    lastOfferedEffect = -1;
    lastOfferedGroup = -1;
    latestEffect = latestGroup = -1;
    latestOwnerPlayerId = -1;
    latestEligible.clear(); latestEligiblePairs.clear(); latestDatabaseProxy = -1;
}
inline bool Initialize(bool requested, bool useStockManualRuntime = false) {
    normalizeGameplayFilter = useStockManualRuntime;
    if (!requested) return true;
    auto chooser = BNM::Class("PvZCards.Core.Systems.Utilities", "ChooseTargetEffectCreator");
    auto finder = BNM::Class("PvZCards.Core.Systems.Utilities", "TargetEntityFinder");
    auto modelClass = BNM::Class("PvZCards.Engine", "EntityModel");
    finderClass = finder;
    entityFinderClass = BNM::Class("PvZCards.Engine", "EntityFinder");
    entityModelClass = modelClass;
    auto entityClass = BNM::Class("PvZCards.Engine", "Entity");
    auto blocking = BNM::Class("PvZCards.Engine.Effects", "ChooseTargetBlockingEffect");
    blockingEffectClass = blocking;
    primaryFilterClass = BNM::Class("PvZCards.Engine.Components", "PrimaryTargetFilter");
    blockingSourceId = BNM::Class("PvZCards.Engine", "Effect").GetField("SourceId").cast<int>();
    blockingPlayerId = BNM::Class("PvZCards.Engine", "Effect").GetField("PlayerId").cast<int>();
    cardClass = BNM::Class("PvZCards.Engine.Components", "Card");
    cardDatabaseClass = BNM::Class("PvZCards.Core.Utilities", "CardDatabase");
    if (cardDatabaseClass.IsValid()) {
        getDatabaseInstance = cardDatabaseClass.GetMethod("GetInstance", 0).cast<Obj*>();
        findDatabaseEntities = cardDatabaseClass.GetMethod("FindEntities", 1).cast<Obj*>();
    }
    databaseCardGuid = cardClass.GetField("Guid").cast<int>();
    CC_DIAG("CustomComponents/Pick3/DB5: QUERY_PREFLIGHT db=%d instance=%d find=%d guid=%d",
        cardDatabaseClass.IsValid() ? 1 : 0, getDatabaseInstance.IsValid() ? 1 : 0,
        findDatabaseEntities.IsValid() ? 1 : 0, databaseCardGuid.IsValid() ? 1 : 0);
    auto filterClass = BNM::Class("PvZCards.Engine.Components", "TargetEntityFilter");
    selectionType = filterClass.GetField("SelectionType");
    numberOfTargets = filterClass.GetField("NumTargets");
    targetFilterQuery = BNM::Class("PvZCards.Engine.Components", "EntityFilter").GetField("Query").cast<Obj*>();
    additionalTargetQuery = filterClass.GetField("AdditionalTargetQuery").cast<Obj*>();
    additionalTargetType = filterClass.GetField("AdditionalTargetType").cast<int>();
    onlyApplyToAdditionalTargets = filterClass.GetField("OnlyApplyEffectsOnAdditionalTargets").cast<bool>();
    auto effectClass = BNM::Class("PvZCards.Engine", "Effect");
    blockingEffectEntityId = effectClass.GetField("EffectEntityId");
    blockingGroupId = effectClass.GetField("EffectGroupId");
    finderEntityFinder = finder.GetField("<EntityFinder>k__BackingField");
    entityFinderModel = BNM::Class("PvZCards.Engine", "EntityFinder").GetField("Model");
    getEntityById = modelClass.GetMethod("GetEntityById", 1).cast<Obj*>();
    getComponent = entityClass.GetMethod("GetComponent", {
        BNM::Class("System", "Type").GetCompileTimeClass()
    }).cast<Obj*>();
    cardGuid = cardClass.GetMethod("GetGuid", 1).cast<int>();
    auto shouldPrimary = chooser.GetMethod("ShouldAddChooseTargetEffects", 2);
    auto shouldSecondary = chooser.GetMethod("ShouldAddChooseTargetEffects", 3);
    auto findValid = finder.GetMethod("FindValidTargetsFromEffect", 1);
    const bool ok = finderClass.IsValid() && entityFinderClass.IsValid() &&
        entityModelClass.IsValid() && blockingEffectClass.IsValid() &&
        primaryFilterClass.IsValid() && selectionType.IsValid() && numberOfTargets.IsValid() &&
        blockingEffectEntityId.IsValid() && blockingGroupId.IsValid() && blockingPlayerId.IsValid() &&
        finderEntityFinder.IsValid() && entityFinderModel.IsValid() && getEntityById.IsValid() && getComponent.IsValid() &&
        cardGuid.IsValid() && targetFilterQuery.IsValid() && additionalTargetQuery.IsValid() &&
        additionalTargetType.IsValid() && onlyApplyToAdditionalTargets.IsValid() &&
        cardDatabaseClass.IsValid() && getDatabaseInstance.IsValid() &&
        findDatabaseEntities.IsValid() && databaseCardGuid.IsValid() &&
        shouldPrimary.IsValid() && shouldSecondary.IsValid() &&
        findValid.IsValid();
    if (!ok) {
        LOG_ERR("CustomComponents/Pick3/STEP2: binding preflight FAILED; no hooks installed");
        return false;
    }
    BNM::BasicHook(shouldPrimary, ShouldPrimaryHook, originalShouldPrimary);
    BNM::BasicHook(shouldSecondary, ShouldSecondaryHook, originalShouldSecondary);
    BNM::BasicHook(findValid, FindValidHook, originalFindValid);
    enabled = originalFindValid && originalShouldPrimary && originalShouldSecondary;
    CC_DIAG("CustomComponents/Pick3/STEP2: candidate bridge=%d mode=%d gameplayManual=%d", enabled ? 1 : 0, -1, normalizeGameplayFilter ? 1 : 0);
    return enabled;
}
} // namespace CustomComponents::Pick3::TargetBridge
