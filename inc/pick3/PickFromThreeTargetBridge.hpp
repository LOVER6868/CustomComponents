#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Step 2: opt-in READ-ONLY capture of stock eligibility for a ChooseTarget
// blocking phase. No Play, SelectTargetEffect, TargetId or queue is modified.
// The popup reports the chosen entity; it DOES NOT submit that choice to the
// game's SelectTargetService yet. This separation is crucial for PvP safety.
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <unordered_set>
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
#include "queries/InDeckQuery.hpp"
#include "zones/DeckMaterializer.hpp"

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
// Scoped initial-play validator snapshot (owned by Submission).  It supplies
// real card entity IDs, not the board-lane recipients returned by stock finder.
// This changes the PICKER OFFER only; the stock blocking effect and its legal
// recipient list are never modified.
inline std::vector<int> (*resolveValidatedInitialCardTargets)(int sourceCard) = nullptr;
// Only active inside the synchronous stock SelectTargetService.Process of a
// verified 712/713 live-card choice. Never used for initial Play generation,
// popup offers, other PickFromThree cards, or foreign owner simulations.
struct ScopedValidationRecipient {
    int effect = -1, group = -1, owner = -1, sourceCard = -1, chosen = -1;
    std::vector<int> validated;
};
inline thread_local const ScopedValidationRecipient* activeValidationRecipient = nullptr;
struct ScopedValidationRecipientGuard {
    const ScopedValidationRecipient* previous;
    explicit ScopedValidationRecipientGuard(const ScopedValidationRecipient* state)
        : previous(activeValidationRecipient) { activeValidationRecipient = state; }
    ~ScopedValidationRecipientGuard() { activeValidationRecipient = previous; }
};

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
// A consumed UI message is not a completed game transaction. Reserve the
// owner through display and service completion, not just until Consume().
enum class OwnershipPhase { Idle, Queued, Displayed };
inline OwnershipPhase ownershipPhase = OwnershipPhase::Idle;
inline int ownerEffect = -1, ownerGroup = -1;
inline std::unordered_set<std::uint64_t> completedOffers;
inline std::uint64_t OfferKey(int effect, int group) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(effect)) << 32) |
           static_cast<std::uint32_t>(group);
}
inline void ReleaseOwnerIfMatching(int effect, int group) {
    if (ownerEffect == effect && ownerGroup == group) {
        ownershipPhase = OwnershipPhase::Idle;
        ownerEffect = ownerGroup = -1;
    }
}
inline std::uint32_t nonce = 0;
inline int latestEffect = -1, latestGroup = -1;
inline int latestOwnerPlayerId = -1;
inline std::vector<int> latestEligible;
// Preserve exact entity/GUID pairs from the *stock-valid* target list.
// Weighted choices must never quietly turn an entity selection into a
// different copy of the same card definition.
inline int lastOfferedEffect = -1;
inline int lastOfferedGroup = -1;
inline std::vector<Candidate> latestEligiblePairs;
inline bool RememberAndNormalize(Obj* filter,int effectId,int groupId);
// Finder invocations include speculative simulation; they are NOT UI events.
// Store each complete snapshot by effect/group and model, then publish only
// the precise effect currently owned by the stock input holder.
struct PendingOffer {
    PreviewRequest request;
    Obj* model = nullptr; // non-owning identity; only compared to rooted live model
    std::vector<Candidate> eligiblePairs;
    std::vector<int> eligible;
    int databaseProxy = -1;
    bool suspended = false;
    std::chrono::steady_clock::time_point cachedAt = std::chrono::steady_clock::now();
};
inline std::deque<PendingOffer> pendingOffers;
// No time-based reroll. This only discards stale *unconsumed* snapshots; an
// active popup stays governed by the stock selection service and FSM.
inline void PruneExpiredPendingOffersLocked() {
    const auto now = std::chrono::steady_clock::now();
    for(auto it=pendingOffers.begin();it!=pendingOffers.end();) {
        const auto age=std::chrono::duration_cast<std::chrono::seconds>(now-it->cachedAt).count();
        const long long lifetime=it->suspended?60:180;
        if(age<=lifetime) {++it;continue;}
        LOG_INFO("CustomComponents/Pick3/HANDOFF_REPAIR: CACHE_EXPIRED effect=%d group=%d suspended=%d age=%lld",
                 it->request.effectEntityId,it->request.effectGroupId,it->suspended?1:0,static_cast<long long>(age));
        it=pendingOffers.erase(it);
    }
}

// A predicted offer has been cached but is not yet visible. This check does
// not publish it, seize input, or bypass the local-holder ownership gate.
// Strict cold-start gate. Do not rebind ordinary stock target commands:
// the missing-cache path belongs only to our custom PickFromThree component.
// Classify the HOLDER'S true effect before interpreting a no-cache state.
// Stock manual targeting (SelectionType 1) is not a missing PickFromThree
// offer; a false positive here hid the root cause of the first-tap report.
// -1 means no suitable filter, -2 means model/API not ready.
inline int ReadActiveSelectionMode(int effectId, Obj* entityModel) {
    if(effectId<=0 || !entityModel || !entityModelClass.IsValid() ||
       !BNM::IsA(entityModel,entityModelClass) || !getEntityById.IsValid() ||
       !getComponent.IsValid() || !primaryFilterClass.IsValid() ||
       !selectionType.IsValid()) return -2;
    Obj* entity=nullptr;
    auto found=BNM::TryInvoke([&]{entity=getEntityById[entityModel](effectId);});
    if(found.IsValid() || !entity) return -1;
    Obj* filter=nullptr;
    found=BNM::TryInvoke([&]{filter=getComponent[entity](primaryFilterClass.GetMonoType());});
    if(found.IsValid() || !filter || !BNM::IsA(filter,primaryFilterClass)) return -1;
    return selectionType[filter].Get();
}
inline bool IsLivePickFromThree(int effectId, Obj* entityModel) {
    return EnumModes::IsPickFromThree(ReadActiveSelectionMode(effectId,entityModel));
}

inline bool HasCurrentModelOffer(int effectId,int groupId,Obj* model,int expectedOwner) {
    if(!model) return false;
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return std::any_of(pendingOffers.begin(),pendingOffers.end(),[&](const PendingOffer& item){
        return item.model==model && item.request.effectEntityId==effectId &&
               item.request.effectGroupId==groupId &&
               (expectedOwner<0 || item.request.ownerPlayerId==expectedOwner);
    });
}
inline bool HasPendingOffer() {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    PruneExpiredPendingOffersLocked();
    return !pendingOffers.empty();
}
inline constexpr std::size_t kMaxPendingOffers = 192;
// Only the stock holder's CURRENT ChooseTargetBlockingEffect can own a popup.
// This is called on Unity's UI thread after the stock holder's Update.
inline bool PublishForActiveEffect(int effectId,int groupId,Obj* liveModel,int expectedOwnerPlayerId=-1) {
    if(effectId<0 || groupId<0 || !liveModel) return false;
    std::lock_guard<std::mutex> lock(snapshotMutex);
    PruneExpiredPendingOffersLocked();
    if(ownershipPhase != OwnershipPhase::Idle || requestReady ||
       completedOffers.count(OfferKey(effectId,groupId))) return false;
    // A suspended popup was previously valid, but the intervening animation
    // may have moved or destroyed one of the offered live entities. Verify
    // its targets again before reopening, even if the EntityModel is identical.
    const auto validOnLiveModel=[&](const PendingOffer& offer) {
        if(offer.request.mode==SelectionMode::CardDatabase)
            return ValidateAutomaticRecipient(liveModel,offer.databaseProxy);
        if(!getEntityById.IsValid() || !cardGuid.IsValid() || offer.request.options.empty())
            return false;
        for(const auto& candidate:offer.request.options) {
            if(candidate.entityId<=0 || candidate.cardGuid<=0) return false;
            Obj* entity=nullptr;
            const auto lookup=BNM::TryInvoke([&]{entity=getEntityById[liveModel](candidate.entityId);});
            if(lookup.IsValid() || !entity) return false;
            int guid=-1;
            const auto found=BNM::TryInvoke([&]{guid=cardGuid(entity);});
            if(found.IsValid() || guid!=candidate.cardGuid) return false;
        }
        return true;
    };
    // Same model: ordinary offers retain their stock snapshot; a suspended
    // offer must pass fresh validity checks. Clone offers always validate.
    auto it=std::find_if(pendingOffers.begin(),pendingOffers.end(),[&](const PendingOffer& item) {
        return item.request.effectEntityId==effectId && item.request.effectGroupId==groupId && item.model==liveModel &&
               (expectedOwnerPlayerId<0 || item.request.ownerPlayerId==expectedOwnerPlayerId) &&
               (!item.suspended || validOnLiveModel(item));
    });
    bool verifiedClone=false;
    if(it==pendingOffers.end()) {
        it=std::find_if(pendingOffers.begin(),pendingOffers.end(),[&](const PendingOffer& item) {
            return item.request.effectEntityId==effectId && item.request.effectGroupId==groupId &&
                   (expectedOwnerPlayerId<0 || item.request.ownerPlayerId==expectedOwnerPlayerId) &&
                   validOnLiveModel(item);
        });
        verifiedClone=it!=pendingOffers.end();
    }
    if(it==pendingOffers.end()) {
        static int lastMissEffect=-1,lastMissGroup=-1;
        if(lastMissEffect!=effectId || lastMissGroup!=groupId) {
            lastMissEffect=effectId;lastMissGroup=groupId;
            LOG_ERR("CustomComponents/Pick3/IDENTITY: ACTIVE_NO_VALID_CACHE effect=%d group=%d cached=%zu requiredOwner=%d noUnsafeFinderCall=1",
                effectId,groupId,pendingOffers.size(),expectedOwnerPlayerId);
        }
        return false;
    }
    // Do not convert the reusable filter to Manual during prediction. A
    // reversible override is installed only for the active stock effect.
    if(normalizeGameplayFilter) {
        // Look up the filter from the holder's actual GameModel, not a
        // predictive clone. Root only this active filter in the scoped
        // override for the real service transaction.
        Obj* entity=nullptr;
        if(getEntityById.IsValid()) {
            const auto ex=BNM::TryInvoke([&]{entity=getEntityById[liveModel](effectId);});
            if(ex.IsValid()) entity=nullptr;
        }
        Obj* filter=nullptr;
        if(entity && getComponent.IsValid() && primaryFilterClass.IsValid()) {
            const auto ex=BNM::TryInvoke([&]{filter=getComponent[entity](primaryFilterClass.GetMonoType());});
            if(ex.IsValid()) filter=nullptr;
        }
        if(!filter || !BNM::IsA(filter,primaryFilterClass) ||
           !EnumModes::IsPickFromThree(selectionType[filter].Get()) ||
           !RememberAndNormalize(filter,effectId,groupId)) {
            LOG_ERR("CustomComponents/Pick3/IDENTITY: ACTIVATE_FILTER_UNAVAILABLE effect=%d group=%d",effectId,groupId);
            return false;
        }
    }
    const bool resumed=it->suspended;
    latestEffect=effectId;latestGroup=groupId;
    latestOwnerPlayerId=it->request.ownerPlayerId;
    latestEligible=std::move(it->eligible);
    latestEligiblePairs=std::move(it->eligiblePairs);
    latestDatabaseProxy=it->databaseProxy;
    nextRequest=std::move(it->request);
    pendingOffers.erase(it);
    requestReady=true;
    ownershipPhase=OwnershipPhase::Queued;
    ownerEffect=effectId;ownerGroup=groupId;
    completedOffers.insert(OfferKey(effectId,groupId));
    lastOfferedEffect=effectId;lastOfferedGroup=groupId;
    LOG_INFO("CustomComponents/Pick3/IDENTITY: ACTIVATE effect=%d group=%d cachedRemaining=%zu verifiedClone=%d resumed=%d",effectId,groupId,pendingOffers.size(),verifiedClone?1:0,resumed?1:0);
    return true;
}

inline BNM::Field<int> numberOfTargets;
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
// Receiver prediction may use a different EntityModel/filter object from the
// displayed popup. Retire only the override created for THAT filter; never
// accidentally restore a second simulation's still-active DB5 query.
inline void CancelDatabaseOverrideForFilter(Obj* filter, int effectId, int groupId,
                                            const char* reason) {
    if (!filter || !Core::SceneViewContext::getGCHandle) return;
    for (auto it=databaseQueryOverrides.begin(); it!=databaseQueryOverrides.end();) {
        if (it->effectId==effectId && it->groupId==groupId &&
            Core::SceneViewContext::getGCHandle(it->filterRoot)==filter) {
            RestoreDatabaseOverride(*it,reason);
            it=databaseQueryOverrides.erase(it);
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
    ReleaseOwnerIfMatching(effectId, groupId);
    if (rearm) completedOffers.erase(OfferKey(effectId, groupId));
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

// An animation/interruption can temporarily replace the input holder's
// blocking effect before the *same* effect becomes targetable again. That is
// NOT a selection completion. Preserve only a never-submitted UI request and
// its actual stock-eligible entity snapshot, then release the scoped filter
// override and active owner. Do not repeat the network dispatch/RNG draw.
// Caller must have ruled out pending, sealed and in-flight submissions.
inline bool SuspendUnsubmittedOffer(const PreviewRequest& request, Obj* liveModel) {
    if(!liveModel || !entityModelClass.IsValid() || !BNM::IsA(liveModel,entityModelClass) ||
       request.options.empty()) return false;
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        if(ownershipPhase!=OwnershipPhase::Displayed ||
           ownerEffect!=request.effectEntityId || ownerGroup!=request.effectGroupId ||
           latestEffect!=request.effectEntityId || latestGroup!=request.effectGroupId ||
           latestOwnerPlayerId!=request.ownerPlayerId || requestReady) return false;
        PruneExpiredPendingOffersLocked();
        // Delete speculative copies of the *same* transaction. Otherwise a
        // stale cloned prediction could win over this already-displayed choice.
        pendingOffers.erase(std::remove_if(pendingOffers.begin(),pendingOffers.end(),
            [&](const PendingOffer& existing){
                return existing.request.effectEntityId==request.effectEntityId &&
                       existing.request.effectGroupId==request.effectGroupId &&
                       existing.request.ownerPlayerId==request.ownerPlayerId;
            }),pendingOffers.end());
        PendingOffer saved;
        saved.request=request;
        saved.model=liveModel;
        saved.eligible=latestEligible;
        saved.eligiblePairs=latestEligiblePairs;
        saved.databaseProxy=latestDatabaseProxy;
        saved.suspended=true;
        saved.cachedAt=std::chrono::steady_clock::now();
        pendingOffers.push_front(std::move(saved));
        if(pendingOffers.size()>kMaxPendingOffers) pendingOffers.pop_back();
    }
    // Success remains non-rearmable. Only this pre-dispatch suspension clears
    // the offer reservation; no stock service/DTO is invoked here.
    CompleteLocalSelection(request.effectEntityId,request.effectGroupId,true);
    LOG_INFO("CustomComponents/Pick3/HANDOFF_REPAIR: SUSPENDED effect=%d group=%d options=%zu noSubmission=1 rearm=1",
             request.effectEntityId,request.effectGroupId,request.options.size());
    return true;
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
        // A non-owning PvP simulation must NOT show a popup, but it still
        // needs the SAME real recipient that the owning client uses. Its
        // CardDatabase Query yields definitions, not live target entities.
        // Redirect only the verified DB5 filter; keep the original database
        // query rooted until LocalSelectTargetService.Process consumes it.
        // No user selection or GUID reconstruction happens in this hook.
        if (EnumModes::kPickFromThreeCardDatabase > 0 &&
            finderEntityFinder.IsValid() && entityFinderModel.IsValid() &&
            primaryFilterClass.IsValid() && selectionType.IsValid() &&
            getComponent.IsValid() && getEntityById.IsValid()) {
            Obj* finder=finderEntityFinder[finderObject].Get();
            Obj* model=finder && BNM::IsA(finder,entityFinderClass)
                ? entityFinderModel[finder].Get() : nullptr;
            Obj* effectEntity=nullptr;
            Obj* filter=nullptr;
            if (model && BNM::IsA(model,entityModelClass)) {
                auto ex=BNM::TryInvoke([&] {effectEntity=getEntityById[model](effectId);});
                if (!ex.IsValid() && effectEntity)
                    ex=BNM::TryInvoke([&] {filter=getComponent[effectEntity](primaryFilterClass.GetMonoType());});
                if (ex.IsValid()) filter=nullptr;
            }
            if (filter && BNM::IsA(filter,primaryFilterClass) &&
                selectionType[filter].Get()==EnumModes::kPickFromThreeCardDatabase &&
                numberOfTargets.IsValid() && numberOfTargets[filter].Get()==1 &&
                RedirectDatabaseTargetQuery(filter,effectId,groupId)) {
                // The game's finder understands stock Manual (=1). The
                // enum override lives only across this synchronous finder
                // call; the redirected Query remains rooted for receive.
                selectionType[filter].Set(kManualValue);
                Obj* result=originalFindValid(self,blocking,method);
                selectionType[filter].Set(EnumModes::kPickFromThreeCardDatabase);
                if (resolveRemoteSession && resolveRemoteSession()) {
                    auto* recipients=reinterpret_cast<List<int>*>(result);
                    const int count=recipients && recipients->items ? recipients->size : -1;
                    const int recipient=count==1 ? recipients->items->m_Items[0] : kNoEntity;
                    if (count!=1 || !ValidateAutomaticRecipient(model,recipient)) {
                        LOG_ERR("CustomComponents/Pick3/DB5_PVP: FOREIGN_RECIPIENT_INVALID effect=%d group=%d count=%d id=%d",
                                effectId,groupId,count,recipient);
                        CancelDatabaseOverrideForFilter(filter,effectId,groupId,"foreign-invalid-recipient");
                    }
                }
                return result;
            }
        }
        // Leave ordinary foreign selections fully stock.
        return originalFindValid(self, blocking, method);
    }

    // Stock asks for valid targets repeatedly, including after the popup has
    // been offered. An already-owned effect/group must not be resolved again
    // through an extra managed EntityModel.GetEntityById invocation.
    // CompleteLocalSelection resets this key for the next transaction.
    bool previouslyOffered = false;
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        previouslyOffered = completedOffers.count(OfferKey(effectId, groupId)) != 0 ||
            (ownershipPhase != OwnershipPhase::Idle &&
             ownerEffect == effectId && ownerGroup == groupId);
    }
    if (previouslyOffered) {
        // Preserve stock behavior unless this exact pending SelectTargetService
        // dispatch was verified by the caller. The initial effect validator's
        // card list is the source of truth; do not substitute arbitrary cards.
        const auto* bridge = activeValidationRecipient;
        if (bridge && bridge->effect == effectId && bridge->group == groupId &&
            bridge->owner == effectOwner && bridge->owner == thisPlayer &&
            std::find(bridge->validated.begin(), bridge->validated.end(), bridge->chosen) != bridge->validated.end()) {
            LOG_INFO("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: FIND_ENTER effect=%d group=%d owner=%d chosen=%d validated=%zu",effectId,groupId,effectOwner,bridge->chosen,bridge->validated.size());
            Obj* result = originalFindValid(self, blocking, method);
            auto* list = reinterpret_cast<List<int>*>(result);
            Obj* ef = finderEntityFinder[finderObject].Get();
            Obj* model = ef && BNM::IsA(ef, entityFinderClass) ? entityFinderModel[ef].Get() : nullptr;
            const int stockCount=list && list->items && list->size>=0 && list->size<=4096 ? list->size : -1;
            const int stockFirst=stockCount>0 ? list->items->m_Items[0] : -1;
            const int stockContainsChosen=stockCount>=0 && std::find(list->items->m_Items,list->items->m_Items+stockCount,bridge->chosen)!=list->items->m_Items+stockCount;
            LOG_INFO("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: STOCK_FIND effect=%d group=%d count=%d first=%d hasChosen=%d model=%p",effectId,groupId,stockCount,stockFirst,stockContainsChosen,(void*)model);
            std::vector<int> safe;
            if (model && BNM::IsA(model, entityModelClass)) {
                for (int id : bridge->validated) {
                    if (id < 0 || safe.size() >= 12) continue;
                    Obj* entity=nullptr; Obj* card=nullptr; int guid=-1;
                    auto ex=BNM::TryInvoke([&] {entity=getEntityById[model](id);});
                    if (ex.IsValid() || !entity) continue;
                    ex=BNM::TryInvoke([&] {card=getComponent[entity](cardClass.GetMonoType());});
                    if (ex.IsValid() || !card) continue;
                    ex=BNM::TryInvoke([&] {guid=cardGuid(entity);});
                    if (!ex.IsValid() && guid>0 && std::find(safe.begin(),safe.end(),id)==safe.end()) safe.push_back(id);
                }
            }
            const bool valid = list && list->items && list->size >= 0 && list->size <= 4096 &&
                list->items->GetCapacity() >= safe.size() && !safe.empty() &&
                std::find(safe.begin(),safe.end(),bridge->chosen)!=safe.end();
            LOG_INFO("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: VERIFIED effect=%d group=%d chosen=%d safe=%zu containsChosen=%d capacity=%zu",effectId,groupId,bridge->chosen,safe.size(),int(std::find(safe.begin(),safe.end(),bridge->chosen)!=safe.end()),list && list->items ? static_cast<size_t>(list->items->GetCapacity()) : size_t(0));
            if (valid) {
                const int oldCount=list->size;
                for (size_t i=0;i<safe.size();++i) list->items->m_Items[i]=safe[i];
                list->size=static_cast<int>(safe.size());
                LOG_INFO("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: APPLIED effect=%d group=%d owner=%d chosen=%d oldCount=%d newCount=%d cap=%zu",
                    effectId,groupId,effectOwner,bridge->chosen,oldCount,list->size,static_cast<size_t>(list->items->GetCapacity()));
            } else LOG_INFO("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: SKIP effect=%d group=%d chosen=%d verified=%zu capacity=%zu",
                effectId,groupId,bridge->chosen,safe.size(),list && list->items ? static_cast<size_t>(list->items->GetCapacity()) : size_t(0));
            return result;
        }

        LOG_INFO("CustomComponents/UnearthTrace: REPEAT_FIND effect=%d group=%d owner=%d local=%d path=stock unchanged=1",
                 effectId, groupId, effectOwner, thisPlayer);
        return originalFindValid(self, blocking, method);
    }
    // Keep future effect/group evaluations in a separate prediction cache;
    // they must not steal or mutate the active popup's eligibility snapshot.
    Obj* finderInstance = finderEntityFinder[finderObject].Get();
    if (!finderInstance || !BNM::IsA(finderInstance, entityFinderClass))
        return originalFindValid(self, blocking, method);
    Obj* model = entityFinderModel[finderInstance].Get();
    if (!model || !BNM::IsA(model, entityModelClass))
        return originalFindValid(self, blocking, method);
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        if (std::any_of(pendingOffers.begin(),pendingOffers.end(),[&](const PendingOffer& item) {
            return item.model==model && item.request.effectEntityId==effectId &&
                   item.request.effectGroupId==groupId;
        })) return originalFindValid(self, blocking, method);
    }
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
    // Repair only the original factory-bound PvE EntityModel.  In particular,
    // never allocate in a cloned prediction model or a remote PvP match.
    if (jsonMode == SelectionMode::UniqueGuidTargets &&
        resolveRemoteSession && !resolveRemoteSession()) {
        const int repaired = Utilities::DeckMaterializer::RepairSameModelDeckEntries(model);
        if (repaired > 0)
            LOG_INFO("CustomComponents/Pick3/LIVE_DECK_REPAIRED: effect=%d group=%d created=%d",effectId,groupId,repaired);
    }
    Obj* result = originalFindValid(self, blocking, method);
    if (!result) {
        if (jsonMode == SelectionMode::CardDatabase)
            CancelDatabaseOverride(effectId, groupId, "finder-null");
        return result;
    }
    // Read-only, direct stock result. A one-entry stock list can complete
    // without our popup, but is not itself proof that a trigger fired.
    {
        const auto* stockRecipients = reinterpret_cast<List<int>*>(result);
        const int stockCount = stockRecipients && stockRecipients->items ? stockRecipients->size : -1;
        const int soleId = stockCount == 1 ? stockRecipients->items->m_Items[0] : -1;
        if (stockCount >= 0 && stockCount <= 3)
            LOG_INFO("CustomComponents/UnearthTrace: FIND_RESULT effect=%d group=%d model=%p owner=%d local=%d mode=%d stockCount=%d soleId=%d behavior=observed-only",
                     effectId, groupId, (void*)model, effectOwner, thisPlayer,
                     jsonSelectionType, stockCount, soleId);
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
    // Read-only diagnostic for UniqueGuidTargets. Preserve every stock ID, including
    // zero and negative values, before the existing candidate filter excludes them.
    // Bounded logging prevents a busy predictive finder from flooding logcat.
    if (jsonMode == SelectionMode::UniqueGuidTargets) {
        static int poolInspectCalls = 0;
        if (poolInspectCalls < 32) {
            ++poolInspectCalls;
            const int inspectCount = std::min(source->size, 12);
            LOG_INFO("CustomComponents/Pick3/POOL_ID_TRACE: BEGIN effect=%d group=%d call=%d count=%d inspected=%d model=%p",
                     effectId, groupId, poolInspectCalls, source->size, inspectCount, (void*)model);
            for (int i = 0; i < inspectCount; ++i) {
                const int id = source->items->m_Items[i];
                Obj* entity = nullptr;
                bool lookupException = false;
                if (getEntityById.IsValid() && model) {
                    const auto lookup = BNM::TryInvoke([&] { entity = getEntityById[model](id); });
                    lookupException = lookup.IsValid();
                }
                Obj* card = nullptr;
                bool componentException = false;
                if (entity && getComponent.IsValid() && cardClass.IsValid()) {
                    const auto check = BNM::TryInvoke([&] {
                        card = getComponent[entity](cardClass.GetMonoType());
                    });
                    componentException = check.IsValid();
                }
                int guid = -1;
                bool guidException = false;
                if (entity && cardGuid.IsValid()) {
                    const auto check = BNM::TryInvoke([&] { guid = cardGuid(entity); });
                    guidException = check.IsValid();
                }
                LOG_INFO("CustomComponents/Pick3/POOL_ID_TRACE: ITEM effect=%d group=%d index=%d rawId=%d entity=%p lookupException=%d cardComponent=%d componentException=%d guid=%d guidException=%d stockFilterWouldKeep=%d",
                         effectId, groupId, i, id, (void*)entity, lookupException ? 1 : 0,
                         card ? 1 : 0, componentException ? 1 : 0,
                         guid, guidException ? 1 : 0,
                         (id > 0 && entity && !lookupException && !guidException && guid > 0) ? 1 : 0);
            }
        }
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
    // 712/713: the initial Play uses the one-time-effect zone, but the
    // original effect validator evaluated the discarded-card target query.
    // Stock FindValidTargetsFromEffect for the blocking selection instead
    // returns lane entities. Reuse ONLY the same Play's previously validated
    // real card IDs for our local popup; never alter the stock result.
    const int sourceCard = blockingSourceId.IsValid()
        ? blockingSourceId[blockingObject].Get() : -1;
    int sourceGuid = -1;
    if (sourceCard >= 0) {
        Obj* sourceEntity = nullptr;
        auto sx = BNM::TryInvoke([&] { sourceEntity=getEntityById[model](sourceCard); });
        if (!sx.IsValid() && sourceEntity) {
            auto gx=BNM::TryInvoke([&] { sourceGuid=cardGuid(sourceEntity); });
            if (gx.IsValid()) sourceGuid=-1;
        }
    }
    if (jsonMode == SelectionMode::UniqueGuidTargets &&
        (sourceGuid==712 || sourceGuid==713) &&
        resolveValidatedInitialCardTargets) {
        auto ids = resolveValidatedInitialCardTargets(sourceCard);
        for (const int id : ids) {
            if (id < 0) continue;
            Obj* entity=nullptr;
            auto ex=BNM::TryInvoke([&] {entity=getEntityById[model](id);});
            if (ex.IsValid() || !entity) continue;
            Obj* card=nullptr;
            ex=BNM::TryInvoke([&] {card=getComponent[entity](cardClass.GetMonoType());});
            if (ex.IsValid() || !card) continue;
            int guid=-1;
            ex=BNM::TryInvoke([&] {guid=cardGuid(entity);});
            if (!ex.IsValid() && guid>0) candidates.push_back({id,guid});
        }
        LOG_INFO("CustomComponents/Pick3/PRIMARY_SOURCE: effect=%d group=%d source=%d guid=%d cached=%zu verified=%zu stockLanes=%d stockListUnchanged=1",
            effectId,groupId,sourceCard,sourceGuid,ids.size(),candidates.size(),source->size);
    }
    if (candidates.empty()) {
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
    } // stock fallback only if no verified initial primary card targets
    }
    // Keep eligibility local until stock activates this exact effect/group.
    // Predictive finder calls must not overwrite a displayed offer's targets.
    std::vector<int> offerEligible;
    std::vector<Candidate> offerEligiblePairs;
    int offerProxy = -1;
    if(jsonMode == SelectionMode::CardDatabase) {
        offerProxy=source->items->m_Items[0];
        offerEligible.push_back(offerProxy);
    } else {
        offerEligiblePairs=candidates;
        for(const auto& candidate:candidates) offerEligible.push_back(candidate.entityId);
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
    // LIVE deck identity audit.  Compare the physical OrderedCardPool entries
    // with the stock finder output on the SAME simulation model.  This never
    // adds targets and never changes deck order or the game's validation.
    if (jsonMode == SelectionMode::UniqueGuidTargets && model &&
        Queries::inDeckModelEntities.IsValid() &&
        Queries::inDeckEntityComponents.IsValid() &&
        Queries::inDeckOrderedPoolEntries.IsValid() &&
        Queries::inDeckPoolEntryEntityId.IsValid() &&
        Queries::inDeckPoolEntryGuid.IsValid() &&
        Queries::inDeckEntityId.IsValid() &&
        Queries::inDeckOrderedPoolClass) {
        auto* entities=Queries::inDeckModelEntities[model].Get();
        int deckEntries=0, liveEntries=0, staleEntries=0, negativeIds=0;
        int stockMatchedEntries=0, duplicateIds=0;
        // Preserve a physical-copy census independently of the stock live
        // entity list.  A GUID-only candidate is NOT a valid Play.Targets ID.
        std::unordered_map<int, int> physicalGuidCounts;
        int virtualCopies=0;
        struct PoolBreakdown {int entries=0,live=0,missing=0,stale=0,stockMatched=0;};
        std::unordered_map<int,PoolBreakdown> byDeckEntity;
        std::unordered_set<int> modelIds, deckIds, stockIds;
        for (const auto& candidate : candidates)
            if (candidate.entityId > 0) stockIds.insert(candidate.entityId);
        if (entities && entities->items && entities->size >= 0 && entities->size < 100000) {
            for (int i=0;i<entities->size;i++) {
                auto* e=Queries::InDeckListAt(entities,i);
                if (e) modelIds.insert(Queries::inDeckEntityId[e].Get());
            }
            for (int i=0;i<entities->size;i++) {
                auto* e=Queries::InDeckListAt(entities,i);
                if (!e) continue;
                auto* comps=Queries::inDeckEntityComponents[e].Get();
                if (!comps || !comps->items || comps->size<0 || comps->size>1000) continue;
                for(int c=0;c<comps->size;c++) {
                    auto* comp=Queries::InDeckListAt(comps,c);
                    if (!comp || comp->klass!=Queries::inDeckOrderedPoolClass) continue;
                    const int deckEntityId=Queries::inDeckEntityId[e].Get();
                    auto& pool=byDeckEntity[deckEntityId];
                    auto* entries=Queries::inDeckOrderedPoolEntries[comp].Get();
                    if (!entries || !entries->items || entries->size<0 || entries->size>10000) continue;
                    for(int j=0;j<entries->size;j++) {
                        auto* entry=Queries::InDeckListAt(entries,j);
                        if (!entry) continue;
                        ++deckEntries;
                        ++pool.entries;
                        const int physicalGuid=Queries::inDeckPoolEntryGuid[entry].Get();
                        if (physicalGuid > 0) ++physicalGuidCounts[physicalGuid];
                        int id=Queries::inDeckPoolEntryEntityId[entry].Get();
                        if (id < 0) ++virtualCopies;
                        if (id<=0) {++negativeIds;++pool.missing;continue;}
                        if (!deckIds.insert(id).second) ++duplicateIds;
                        if (modelIds.count(id)) {++liveEntries;++pool.live;}
                        else {++staleEntries;++pool.stale;}
                        if (stockIds.count(id)) {++stockMatchedEntries;++pool.stockMatched;}
                    }
                }
            }
            LOG_INFO("CustomComponents/Pick3/LIVE_DECK_AUDIT: effect=%d group=%d physicalEntries=%d liveEntries=%d staleEntries=%d missingIds=%d duplicateIds=%d stockEligible=%zu poolMatched=%d",
                     effectId,groupId,deckEntries,liveEntries,staleEntries,negativeIds,duplicateIds,stockIds.size(),stockMatchedEntries);
            // These counts are per OrderedCardPool-bearing ENTITY, not per
            // player ID. Never invent an owner from the physical GUID list.
            // The active model and factory identity expose PvP clone
            // divergence without mutating the deck or the stock target list.
            Obj* materializerModel=Utilities::DeckMaterializer::boundModelRoot &&
                Core::SceneViewContext::getGCHandle
                ? Core::SceneViewContext::getGCHandle(Utilities::DeckMaterializer::boundModelRoot) : nullptr;
            Obj* materializerFactory=Utilities::DeckMaterializer::boundFactoryRoot &&
                Core::SceneViewContext::getGCHandle
                ? Core::SceneViewContext::getGCHandle(Utilities::DeckMaterializer::boundFactoryRoot) : nullptr;
            const bool sameFactoryModel=materializerModel==model && materializerFactory &&
                Utilities::DeckMaterializer::cardFactoryEntityModel.IsValid() &&
                Utilities::DeckMaterializer::cardFactoryEntityModel[materializerFactory].Get()==model;
            for(const auto& [deckId,pool]:byDeckEntity) {
                LOG_INFO("CustomComponents/Pick3/DECK_MODEL_BOUNDARY: effect=%d group=%d model=%p initializerModel=%p sameFactoryModel=%d remote=%d deckEntity=%d total=%d live=%d virtual=%d stale=%d stockMatched=%d readOnly=1",
                    effectId,groupId,(void*)model,(void*)materializerModel,sameFactoryModel?1:0,
                    resolveRemoteSession && resolveRemoteSession()?1:0,deckId,pool.entries,pool.live,pool.missing,pool.stale,pool.stockMatched);
            }
            // The historic PHYSICAL_POOL/PHYSICAL_GAP aggregate counts ALL
            // ordered pools (both players, supers and temporary pools). Report
            // the actual STOCK-matched deck separately; otherwise a PvP pool
            // with 27 virtual cards can misleadingly appear to have 67 missing.
            int matchedDeck=-1,matchedDeckCount=0,matchedDeckPools=0;
            PoolBreakdown matchedBreakdown{};
            for(const auto& [deckId,pool]:byDeckEntity) {
                if(pool.stockMatched<=0) continue;
                ++matchedDeckPools;
                if(pool.stockMatched>matchedDeckCount) {
                    matchedDeck=deckId;
                    matchedDeckCount=pool.stockMatched;
                    matchedBreakdown=pool;
                }
            }
            LOG_INFO("CustomComponents/Pick3/DECK_TARGET_POOL: effect=%d group=%d matchedDeck=%d matchingPools=%d unique=%d selectedPoolTotal=%d selectedPoolLive=%d selectedPoolVirtual=%d selectedPoolStale=%d selectedPoolStockTargets=%d virtualSelectable=0 protocol=DEK1_REQUIRED",
                effectId,groupId,matchedDeck,matchedDeckPools,matchedDeckPools==1?1:0,
                matchedBreakdown.entries,matchedBreakdown.live,matchedBreakdown.missing,
                matchedBreakdown.stale,matchedDeckCount);
            LOG_INFO("CustomComponents/Pick3/PHYSICAL_POOL: effect=%d group=%d copies=%d virtualCopies=%d distinctGuids=%zu liveEligible=%zu transport=stock-entity-only",
                     effectId, groupId, deckEntries, virtualCopies, physicalGuidCounts.size(), stockIds.size());
            std::unordered_map<int,int> stockGuidCounts;
            for(const auto& c:candidates) if(c.cardGuid>0 && c.entityId>0) ++stockGuidCounts[c.cardGuid];
            int excludedCopies=0, excludedGuids=0;
            for(const auto& kv:physicalGuidCounts) {
                const int visible=stockGuidCounts[kv.first];
                const int excluded=std::max(0,kv.second-visible);
                excludedCopies+=excluded;
                if(excluded) ++excludedGuids;
                if(excluded)
                    LOG_INFO("CustomComponents/Pick3/PHYSICAL_GAP: effect=%d group=%d guid=%d physical=%d stockVisible=%d difference=%d eligibilityUnknown=1",
                        effectId,groupId,kv.first,kv.second,visible,excluded);
            }
            LOG_INFO("CustomComponents/Pick3/PHYSICAL_SUMMARY: effect=%d group=%d excludedCopies=%d affectedGuids=%d stockLive=%zu virtualTransport=DISABLED requiresReceiverContract=1",
                effectId,groupId,excludedCopies,excludedGuids,stockIds.size());
        }
    }
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
        // Three slots are filled with distinct ENTITY copies when there are
        // fewer than three distinct GUIDs, even if two options share a GUID.
        // Never fabricate a card when fewer than three physical copies exist.
        if (counts.size() < kMaximumChoices)
            LOG_INFO("CustomComponents/Pick3/WEIGHTED_POOL: GUID_FALLBACK effect=%d guidStacks=%zu liveEntities=%zu expectedSlots=%zu",
                     effectId, counts.size(), uniqueEntities.size(),
                     std::min(kMaximumChoices, uniqueEntities.size()));
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
    if (source->size == 1 || request.options.size() == 1) {
        LOG_INFO("CustomComponents/UnearthTrace: OFFER_DECISION effect=%d group=%d model=%p owner=%d stockCount=%d candidates=%zu options=%zu required=%d soleEntity=%d note=not-a-completion-signal",
                 effectId, groupId, (void*)model, effectOwner, source->size,
                 candidates.size(), request.options.size(), required,
                 candidates.size() == 1 ? candidates[0].entityId : -1);
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
    // Never override Effect.SourceId with a recently played card. A phase
    // trigger may happen immediately after an unrelated card play, especially
    // in PvE, and the last-play timestamp does not prove effect ownership.
    LOG_INFO("CustomComponents/Pick3/UI: SOURCE_PROVENANCE effect=%d sourceEntity=%d effectGuid=%d playedGuid=%d trusted=effect-source",
             effectId, blockingSourceId.IsValid()
                 ? blockingSourceId[static_cast<Obj*>(blocking)].Get() : -1,
             request.sourceCardGuid, lastPlayedCardGuid);
    if (request.options.size() < static_cast<std::size_t>(required)) {
        LOG_ERR("CustomComponents/Pick3/STEP2: insufficient offered targets required=%d actual=%zu effect=%d",
                required, request.options.size(), effectId);
        if (jsonMode == SelectionMode::CardDatabase)
            CancelDatabaseOverride(effectId, groupId, "insufficient-options");
        return result;
    }
    CC_DIAG("CustomComponents/Pick3/STEP2: valid=%d resolved=%zu sampled=%zu required=%d effect=%d selectionType=%d mode=%d",
             source->size, candidates.size(), request.options.size(), required, effectId, jsonSelectionType, static_cast<int>(jsonMode));
    if (!request.options.empty()) {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        PruneExpiredPendingOffersLocked();
        const auto key=OfferKey(effectId,request.effectGroupId);
        const bool duplicate=completedOffers.count(key) ||
            std::any_of(pendingOffers.begin(),pendingOffers.end(),[&](const PendingOffer& item) {
                return item.request.effectEntityId==effectId &&
                       item.request.effectGroupId==request.effectGroupId && item.model==model;
            });
        if(!duplicate) {
            PendingOffer offer;
            offer.model=model;
            offer.eligible=std::move(offerEligible);
            offer.eligiblePairs=std::move(offerEligiblePairs);
            offer.databaseProxy=offerProxy;
            offer.request=std::move(request);
            offer.cachedAt=std::chrono::steady_clock::now();
            pendingOffers.push_back(std::move(offer));
            if(pendingOffers.size()>kMaxPendingOffers) {
                LOG_INFO("CustomComponents/Pick3/IDENTITY: CACHE_BOUND evict=%d/%d",
                    pendingOffers.front().request.effectEntityId,pendingOffers.front().request.effectGroupId);
                pendingOffers.pop_front();
            }
            LOG_INFO("CustomComponents/Pick3/IDENTITY: CACHED effect=%d group=%d count=%zu activeOwner=%d/%d",
                effectId,groupId,pendingOffers.size(),ownerEffect,ownerGroup);
        }
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
    if (ownershipPhase == OwnershipPhase::Queued &&
        ownerEffect == out.effectEntityId && ownerGroup == out.effectGroupId)
        ownershipPhase = OwnershipPhase::Displayed;
    return true;
}
inline void Clear() {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    for (const auto& entry : runtimeOverrides) RestoreOverride(entry, "match-reset");
    runtimeOverrides.clear();
    for (const auto& entry : databaseQueryOverrides) RestoreDatabaseOverride(entry, "match-reset");
    databaseQueryOverrides.clear();
    pendingOffers.clear();
    nextRequest = PreviewRequest{};
    requestReady = false;
    ownershipPhase = OwnershipPhase::Idle;
    ownerEffect = ownerGroup = -1;
    completedOffers.clear();
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
