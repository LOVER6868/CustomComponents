#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Scoped selection results shared by PickFromThree submission and downstream
// EntityQuery.Matches. CardDatabase GUIDs are NOT live Entity.Id values.
// No winner is exposed until an actual UI selection has been validated.
// Matches are keyed by originating effect entity, not a global last pick.
// The latest pick per effect persists within the current match. Match end clears it.

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>
#include "log.h"

#ifndef CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
#define CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE CUSTOMCOMPONENTS_SELECTION_DIAGNOSTICS
#endif

namespace CustomComponents::Pick3::Picked {

struct SelectedCard {
    int entityId = -1;
    int cardGuid = -1;
    int boundEntityId = -1; // Verified stock-produced card for a database selection.
};
struct Selection {
    int effectEntityId = -1;
    int effectGroupId = -1;
    std::vector<SelectedCard> cards;
};
inline std::mutex mutex;
inline std::vector<Selection> selections;

// 2.9: A deliberately ephemeral selection view used ONLY while the stock
// trigger validator evaluates a dependent GUID query. It is not a commit,
// does not update `selections`, and never outlives the validator call.
namespace InFlight {
struct Identity {
    const void* model = nullptr; // pointer comparison only; never retained beyond scope
    const void* context = nullptr;
    int effect = -1;
    int source = -1;
    int entity = -1;
    int guid = -1;
    bool valid = false;
};
inline thread_local Identity active{};
struct Scoped {
    Identity previous;
    explicit Scoped(Identity next) : previous(active) { active = next; }
    ~Scoped() { active = previous; }
    Scoped(const Scoped&) = delete;
    Scoped& operator=(const Scoped&) = delete;
};
inline bool Matches(const void* context, int effect, int candidateGuid) {
    return active.valid && context && context == active.context &&
           effect > 0 && effect == active.effect &&
           candidateGuid > 0 && candidateGuid == active.guid;
}
// Same exact stock-accepted event proof as GUID-domain matching, but an
// entity-domain match MUST use live Entity.Id. GUID equality alone is never
// sufficient to identify one of several copies of the same card.
inline bool MatchesEntity(const void* context, int effect, int candidateEntity) {
    return active.valid && context && context==active.context &&
           effect>0 && effect==active.effect &&
           candidateEntity>0 && candidateEntity==active.entity;
}
} // namespace InFlight


// 2.11: An exact event-to-listener handoff for deferred dependent queries.
// The producer provides a STOCK-ACCEPTED live target, not a guessed latest pick.
// The consumer opens a temporary scope only for the matching stock queue
// effect/source/target. Multiple outstanding links for a listener are unsafe:
// fail closed instead of using registration order as an implicit generation.
#ifndef CUSTOMCOMPONENTS_PF3_ENTITY_LINK_ROUTING
#define CUSTOMCOMPONENTS_PF3_ENTITY_LINK_ROUTING 1
#endif
namespace DeferredLink {
struct Link {
    int listener=-1, source=-1, target=-1, guid=-1;
    int selectionEffect=-1, selectionGroup=-1;
};
inline std::mutex lock;
inline std::vector<Link> pending;
inline constexpr std::size_t capacity=256;
inline thread_local Link active{};
inline thread_local bool activeValid=false;
inline bool Enqueue(int listener, int source, int target, int guid,
                    int selectionEffect, int selectionGroup) {
    if(listener<=0 || source<=0 || target<=0 || guid<=0 ||
       selectionEffect<=0 || selectionGroup<0) return false;
    std::lock_guard<std::mutex> guard(lock);
    if(pending.size()>=capacity) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ENTITY_LINK: phase=LINK_CAPACITY_REJECT listener=%d",listener);
        return false;
    }
    pending.push_back({listener,source,target,guid,selectionEffect,selectionGroup});
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ENTITY_LINK: phase=LINK_QUEUED listener=%d source=%d target=%d guid=%d pickedEffect=%d pickedGroup=%d",listener,source,target,guid,selectionEffect,selectionGroup);
    return true;
}
struct ScopedExecution {
    Link prior;
    bool priorValid=false;
    bool bound=false;
    explicit ScopedExecution(int listener, int source, int target)
        :prior(active),priorValid(activeValid) {
        active={};activeValid=false;
        if(listener<=0 || source<=0 || target<=0) return;
        std::lock_guard<std::mutex> guard(lock);
        std::size_t count=0,index=0;
        for(std::size_t i=0;i<pending.size();++i)
            if(pending[i].listener==listener && pending[i].source==source &&
               pending[i].target==target) {++count;index=i;}
        if(count!=1) {
            if(count>1) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ENTITY_LINK: phase=LINK_AMBIGUOUS listener=%d source=%d target=%d count=%zu fallback=LEGACY",listener,source,target,count);
            return;
        }
        active=pending[index];activeValid=true;bound=true;
        pending.erase(pending.begin()+static_cast<std::ptrdiff_t>(index));
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ENTITY_LINK: phase=LINK_EXECUTION_BOUND listener=%d source=%d target=%d guid=%d",listener,source,target,active.guid);
    }
    ~ScopedExecution() {
        if(bound) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ENTITY_LINK: phase=LINK_EXPIRED listener=%d",active.listener);
        active=prior;activeValid=priorValid;
    }
    ScopedExecution(const ScopedExecution&)=delete;
    ScopedExecution& operator=(const ScopedExecution&)=delete;
};
// An active linked listener has authoritative identity for BOTH outcomes.
// Return false only when no proven link exists; out=false is a legitimate
// negative result and must override a stale global-latest positive match.
inline bool Resolve(int listener, int candidate, bool guidDomain, bool& out) {
    if(!activeValid || listener<=0 || active.listener!=listener) return false;
    out=candidate>0 && candidate==(guidDomain?active.guid:active.target);
    return true;
}
inline bool MatchesEntity(int listener, int entity) {
    bool match=false;
    return Resolve(listener,entity,false,match) && match;
}
inline void Reset() {
    std::lock_guard<std::mutex> guard(lock);
    pending.clear();active={};activeValid=false;
}
} // namespace DeferredLink

// Patch 2 milestone A: a shadow history, not a substitute for the legacy
// synchronous query cache. The stock model-level commit boundary is still
// under investigation, so none of these records claims to be committed.
enum class SnapshotObservation : std::uint8_t {
    Staged, CallbackSuccessObserved, CallbackFailureObserved, ErasedBeforeCommitProof
};
struct SelectionSnapshot {
    std::uint64_t sequence = 0;  // match-local, never a network identifier
    Selection selection;
    SnapshotObservation observation = SnapshotObservation::Staged;
    const char* firstOrigin = "UNKNOWN"; // static literals only, no retained pointers to runtime objects
    std::uint32_t stageCalls = 1;
    std::uint32_t stockEnterObserved = 0;
    std::uint32_t stockReturnObserved = 0;
    std::uint32_t cancellationObserved = 0;
};
inline std::vector<SelectionSnapshot> snapshotHistory;
inline std::uint64_t nextSnapshotSequence = 1;
inline std::uint64_t droppedSnapshots = 0;
constexpr std::size_t kMaxSnapshotHistory = 1024;

inline const char* SnapshotObservationName(SnapshotObservation observation) {
    switch (observation) {
    case SnapshotObservation::Staged: return "STAGED";
    case SnapshotObservation::CallbackSuccessObserved: return "CALLBACK_SUCCESS_NOT_COMMIT_PROOF";
    case SnapshotObservation::CallbackFailureObserved: return "CALLBACK_FAILURE";
    case SnapshotObservation::ErasedBeforeCommitProof: return "ERASED";
    }
    return "UNKNOWN";
}

// Caller must hold mutex; no native pointers are retained in the history.
inline void LogSnapshotLocked(const char* phase, const SelectionSnapshot& snapshot) {
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
    const auto& sel = snapshot.selection;
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_HISTORY: phase=%s seq=%llu effect=%d group=%d status=%s origin=%s stageCalls=%u cards=%zu firstEntity=%d firstGuid=%d history=%zu dropped=%llu (NOT_MODEL_COMMIT_PROOF)",
        phase, (unsigned long long)snapshot.sequence, sel.effectEntityId, sel.effectGroupId,
        SnapshotObservationName(snapshot.observation), snapshot.firstOrigin, snapshot.stageCalls, sel.cards.size(),
        sel.cards.empty() ? -1 : sel.cards.front().entityId,
        sel.cards.empty() ? -1 : sel.cards.front().cardGuid,
        snapshotHistory.size(), (unsigned long long)droppedSnapshots);
#else
    (void)phase; (void)snapshot;
#endif
}

// The latest instance of this exact effect/group is the only record that may
// be annotated by a callback. A previous generation is never overwritten.
inline SelectionSnapshot* LatestSnapshotLocked(int effectId, int groupId) {
    for (auto it = snapshotHistory.rbegin(); it != snapshotHistory.rend(); ++it)
        if (it->selection.effectEntityId == effectId && it->selection.effectGroupId == groupId)
            return &*it;
    return nullptr;
}

// A duplicate submission may pass through TX and RX reconstruction while
// the same synchronous selection is still pending. Only reuse the *latest*
// staged snapshot with exactly the same effect, group and selected identities.
// A finished/erased entry or an interleaved selection starts a new generation.
inline bool SameSelectedCards(const std::vector<SelectedCard>& a,
                              const std::vector<SelectedCard>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].entityId != b[i].entityId ||
            a[i].cardGuid != b[i].cardGuid ||
            a[i].boundEntityId != b[i].boundEntityId) return false;
    return true;
}

inline void ObserveCompletion(int effectId, int groupId, bool success) {
    std::lock_guard<std::mutex> guard(mutex);
    auto* entry = LatestSnapshotLocked(effectId, groupId);
    if (!entry || entry->observation != SnapshotObservation::Staged) {
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_HISTORY: phase=CALLBACK_UNMATCHED effect=%d group=%d success=%d history=%zu latestState=%s", effectId, groupId, int(success), snapshotHistory.size(), entry ? SnapshotObservationName(entry->observation) : "MISSING");
#endif
        return;
    }
    entry->observation = success ? SnapshotObservation::CallbackSuccessObserved : SnapshotObservation::CallbackFailureObserved;
    LogSnapshotLocked("CALLBACK_OBSERVED", *entry);
}

// Recorded stock model entry/return is useful evidence for accepted in-flight
// selection; it is NOT proof that a model mutation was committed.
inline void ObserveStockModelBoundary(int effect, int group, bool returned,
                                      bool cancellationKnown, bool cancelled) {
    std::lock_guard<std::mutex> guard(mutex);
    auto* entry=LatestSnapshotLocked(effect,group);
    if (!entry || effect<=0 || group<0) {
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_HISTORY: phase=STOCK_BOUNDARY_MISSING effect=%d group=%d returned=%d reason=NO_HISTORY_FOR_STOCK_EFFECT",effect,group,int(returned));
#endif
        return;
    }
    if(returned) ++entry->stockReturnObserved;
    else ++entry->stockEnterObserved;
    if(cancellationKnown && cancelled) ++entry->cancellationObserved;
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_HISTORY: phase=STOCK_%s effect=%d group=%d seq=%llu state=%s entries=%u returns=%u cancellationKnown=%d cancelled=%d commitProven=0",returned?"RETURN_OBSERVED":"ENTER_OBSERVED",effect,group,(unsigned long long)entry->sequence,SnapshotObservationName(entry->observation),entry->stockEnterObserved,entry->stockReturnObserved,int(cancellationKnown),int(cancelled));
#endif
}

inline std::vector<SelectionSnapshot> CopySnapshotHistory() {
    std::lock_guard<std::mutex> guard(mutex);
    return snapshotHistory;
}
struct FinderHistory {
    unsigned matches=0;std::uint64_t sequence=0,newerSequence=0;int newerGroup=-1;
};
// Metadata only: finder scopes must not clone every selection's card vector.
inline FinderHistory InspectFinderHistory(int effect,int group) {
    FinderHistory result;
    std::lock_guard<std::mutex> guard(mutex);
    for(const auto& entry:snapshotHistory)if(entry.selection.effectEntityId==effect && entry.selection.effectGroupId==group){++result.matches;result.sequence=entry.sequence;}
    if(result.matches!=1){result.sequence=0;return result;}
    for(const auto& entry:snapshotHistory)if(entry.selection.effectEntityId==effect && entry.sequence>result.sequence && entry.sequence>result.newerSequence){result.newerSequence=entry.sequence;result.newerGroup=entry.selection.effectGroupId;}
    return result;
}
inline std::uint64_t UniqueObservedSequence(int effect,int group,unsigned* count=nullptr) {
    std::lock_guard<std::mutex> guard(mutex);
    unsigned matches=0;std::uint64_t sequence=0;
    for(const auto& entry:snapshotHistory) if(entry.selection.effectEntityId==effect && entry.selection.effectGroupId==group) {
        ++matches;sequence=entry.sequence;
    }
    if(count) *count=matches;
    return matches==1?sequence:0;
}

// Exact snapshot lookups are separate from legacy Matches()/MatchesLatestLiveCard().
// The caller MUST supply the observed generation sequence. An effect ID or
// effect/group pair alone is not sufficient when choices may repeat.
// CallbackSuccessObserved is NOT verified stock model commit; the caller can
// explicitly choose to require a positive callback before reading a snapshot.
enum class SnapshotLookup : std::uint8_t {
    Found, Missing, NotConfirmed, Rejected, InvalidIdentity
};
inline const char* SnapshotLookupName(SnapshotLookup result) {
    switch (result) {
    case SnapshotLookup::Found: return "FOUND";
    case SnapshotLookup::Missing: return "MISSING";
    case SnapshotLookup::NotConfirmed: return "NOT_CONFIRMED";
    case SnapshotLookup::Rejected: return "REJECTED";
    case SnapshotLookup::InvalidIdentity: return "INVALID_IDENTITY";
    }
    return "UNKNOWN";
}

inline SnapshotLookup CopyExactSnapshot(int effectId, int groupId,
                                       std::uint64_t sequence,
                                       SelectionSnapshot& out,
                                       bool requireSuccessCallback = true) {
    if (effectId <= 0 || groupId < 0 || sequence == 0)
        return SnapshotLookup::InvalidIdentity;
    std::lock_guard<std::mutex> guard(mutex);
    for (const auto& entry : snapshotHistory) {
        if (entry.sequence != sequence) continue;
        if (entry.selection.effectEntityId != effectId ||
            entry.selection.effectGroupId != groupId)
            return SnapshotLookup::InvalidIdentity;
        if (entry.observation == SnapshotObservation::CallbackFailureObserved ||
            entry.observation == SnapshotObservation::ErasedBeforeCommitProof)
            return SnapshotLookup::Rejected;
        if (requireSuccessCallback &&
            entry.observation != SnapshotObservation::CallbackSuccessObserved)
            return SnapshotLookup::NotConfirmed;
        out = entry; // Copies IDs and card GUIDs; no native runtime pointers.
        return SnapshotLookup::Found;
    }
    return SnapshotLookup::Missing;
}

inline SnapshotLookup MatchesExactSnapshot(int effectId, int groupId,
                                           std::uint64_t sequence,
                                           int candidateEntityId, int candidateGuid,
                                           bool guidDomain, bool& matched,
                                           bool requireSuccessCallback = true) {
    matched = false;
    SelectionSnapshot snapshot;
    const auto status = CopyExactSnapshot(effectId, groupId, sequence, snapshot,
                                          requireSuccessCallback);
    if (status != SnapshotLookup::Found) return status;
    for (const auto& card : snapshot.selection.cards) {
        if (guidDomain) {
            if (candidateGuid > 0 && candidateGuid == card.cardGuid)
                matched = true;
        } else if (candidateEntityId > 0 &&
                   (candidateEntityId == card.entityId ||
                    (card.entityId == -1 && candidateEntityId == card.boundEntityId))) {
            matched = true;
        }
    }
    return SnapshotLookup::Found;
}


// Legacy/current-view diagnostic (not snapshot commit proof).
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE

inline void TraceLedger(const char* phase, int effectId, int groupId, int candidateId = -1) {
    std::lock_guard<std::mutex> guard(mutex);
    int sameEffect = 0, exactGroup = 0;
    for (const auto& item : selections) {
        if (item.effectEntityId == effectId) {
            ++sameEffect;
            if (item.effectGroupId == groupId) ++exactGroup;
        }
    }
    const auto* last = selections.empty() ? nullptr : &selections.back();
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_SNAPSHOT: phase=%s effect=%d group=%d candidate=%d remembered=%zu sameEffect=%d exactGroup=%d latestEffect=%d latestGroup=%d latestCount=%zu latestFirstEntity=%d latestFirstGuid=%d (OBSERVATION_ONLY)",
        phase, effectId, groupId, candidateId, selections.size(), sameEffect, exactGroup,
        last ? last->effectEntityId : -1, last ? last->effectGroupId : -1,
        last ? last->cards.size() : 0,
        last && !last->cards.empty() ? last->cards.front().entityId : -1,
        last && !last->cards.empty() ? last->cards.front().cardGuid : -1);
}
#else
inline void TraceLedger(const char*, int, int, int = -1) {}
#endif

inline bool Store(int effectId, int groupId, const std::vector<SelectedCard>& cards,
                  const char* origin = "LEGACY", std::uint64_t* stagedSequence = nullptr) {
    if (stagedSequence) *stagedSequence=0;
    if (effectId <= 0 || groupId < 0 || cards.empty() || cards.size() > 3)
        return false;
    for (const auto& card : cards) {
        if (card.entityId == 0 || card.entityId < -1 || card.cardGuid <= 0) return false;
    }
    std::lock_guard<std::mutex> guard(mutex);
    // Exactly one latest result per source effect. Repeated activations may
    // allocate new group IDs, so matching both effect and group would leave
    // stale entries and eventually hit the old 64-entry rejection limit.
    // Keep the result in process memory; downstream queries still resolve by
    // effect ID so an unrelated effect cannot consume another one's choice.
    selections.erase(std::remove_if(selections.begin(), selections.end(),
        [&](const Selection& old) { return old.effectEntityId == effectId; }),
        selections.end());
    // Bound the cache if many distinct source effects fire within one match.
    // Replace the oldest result rather than failing a valid new selection.
    constexpr std::size_t kMaxRememberedEffects = 64;
    if (selections.size() >= kMaxRememberedEffects)
        selections.erase(selections.begin());
    selections.push_back({effectId, groupId, cards});
    // Legacy current selection semantics above remain unchanged. The shadow
    // ledger deduplicates only the immediately preceding unfinished choice.
    const char* safeOrigin = origin ? origin : "UNKNOWN";
    if (!snapshotHistory.empty()) {
        auto& last = snapshotHistory.back();
        if (last.observation == SnapshotObservation::Staged &&
            last.selection.effectEntityId == effectId &&
            last.selection.effectGroupId == groupId &&
            SameSelectedCards(last.selection.cards, cards)) {
            ++last.stageCalls;
            if (stagedSequence) *stagedSequence=last.sequence;
            LogSnapshotLocked("REUSE_STAGED", last);
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_HISTORY: phase=REUSE_ORIGIN seq=%llu origin=%s firstOrigin=%s effect=%d group=%d",
                (unsigned long long)last.sequence, safeOrigin, last.firstOrigin, effectId, groupId);
#endif
            return true;
        }
    }
    // Append a new choice generation, never infer stock commit from this.
    // The status is observational; no model commit is inferred here.
    if (snapshotHistory.size() >= kMaxSnapshotHistory) {
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_HISTORY: phase=EVICT_OLDEST seq=%llu effect=%d group=%d capacity=%zu",
            (unsigned long long)snapshotHistory.front().sequence,
            snapshotHistory.front().selection.effectEntityId,
            snapshotHistory.front().selection.effectGroupId, kMaxSnapshotHistory);
#endif
        snapshotHistory.erase(snapshotHistory.begin());
        ++droppedSnapshots;
    }
    const std::uint64_t seq = nextSnapshotSequence++;
    if (stagedSequence) *stagedSequence=seq;
    snapshotHistory.push_back({seq, {effectId, groupId, cards}, SnapshotObservation::Staged, safeOrigin, 1});
    LogSnapshotLocked("STORE_STAGED", snapshotHistory.back());
    return true;
}

inline void Erase(int effectId, int groupId) {
    std::lock_guard<std::mutex> guard(mutex);
    if (auto* snapshot = LatestSnapshotLocked(effectId, groupId);
        snapshot && snapshot->observation != SnapshotObservation::ErasedBeforeCommitProof) {
        snapshot->observation = SnapshotObservation::ErasedBeforeCommitProof;
        LogSnapshotLocked("ERASE_CURRENT", *snapshot);
    }
    selections.erase(std::remove_if(selections.begin(), selections.end(),
        [&](const Selection& old) {
            return old.effectEntityId == effectId && old.effectGroupId == groupId;
        }), selections.end());
}

// Cross-effect lookup for post-pick effects such as Captain Cucumber.
// The newest successful selection is authoritative until replaced or match end.
// Database choices do not have a live Entity.Id, so those compare card GUIDs.
inline bool MatchesLatestLiveCard(int entityId, int cardGuid) {
    if (entityId <= 0 && cardGuid <= 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    if (selections.empty()) return false;
    const Selection& latest = selections.back();
    for (const auto& card : latest.cards) {
        if (card.entityId > 0 && entityId > 0 && card.entityId == entityId) return true;
        if (card.entityId == -1 && card.boundEntityId > 0 &&
            entityId > 0 && card.boundEntityId == entityId) return true;
    }
    return false;
}

// During stock GUID lookup, the query supplies the originating effect ID.
// This must be observed inside the finder's call stack, not inferred from
// a matching GUID or the newest selection.
inline thread_local int finderSelectionEffect = -1;
inline thread_local int finderSelectionGuid = -1;
inline thread_local unsigned finderSelectionMatches = 0;
inline thread_local bool finderObserving = false;

inline void ObserveGuidMatch(int effectId, int guid) {
    if (!finderObserving || effectId <= 0 || guid <= 0) return;
    ++finderSelectionMatches;
    if (finderSelectionMatches == 1) {
        finderSelectionEffect = effectId;
        finderSelectionGuid = guid;
    } else if (finderSelectionEffect != effectId || finderSelectionGuid != guid) {
        finderSelectionEffect = -1; // Ambiguous provenance, fail closed.
    }
}

inline bool BindCreatedExact(int effect,int group,std::uint64_t sequence,int guid,int createdId) {
    if(effect<=0 || group<0 || !sequence || guid<=0 || createdId<=0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    for(auto& snapshot:snapshotHistory) {
        if(snapshot.sequence!=sequence || snapshot.selection.effectEntityId!=effect ||
           snapshot.selection.effectGroupId!=group) continue;
        if(snapshot.observation==SnapshotObservation::CallbackFailureObserved ||
           snapshot.observation==SnapshotObservation::ErasedBeforeCommitProof) return false;
        SelectedCard* match=nullptr;
        for(auto& card:snapshot.selection.cards) if(card.entityId==-1 && card.cardGuid==guid) {
            if(match) return false;
            match=&card;
        }
        if(!match || (match->boundEntityId>0 && match->boundEntityId!=createdId)) return false;
        match->boundEntityId=createdId;
        // Update the compatibility cache only if it still represents this generation.
        const auto* latest=LatestSnapshotLocked(effect,group);
        if(latest && latest->sequence==sequence)
            for(auto& current:selections) if(current.effectEntityId==effect && current.effectGroupId==group)
                for(auto& card:current.cards) if(card.entityId==-1 && card.cardGuid==guid &&
                    (card.boundEntityId<=0 || card.boundEntityId==createdId)) card.boundEntityId=createdId;
        LogSnapshotLocked("EXACT_CREATED_BIND",snapshot);
        return true;
    }
    return false;
}

inline bool BindCreated(int sourceEffect, int guid, int createdId) {
    if (sourceEffect <= 0 || guid <= 0 || createdId <= 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    for (auto& entry : selections) {
        if (entry.effectEntityId != sourceEffect) continue;
        for (auto& card : entry.cards) {
            if (card.cardGuid == guid && card.entityId == -1) {
                if (card.boundEntityId > 0 && card.boundEntityId != createdId)
                    return false; // Do not overwrite a distinct produced instance.
                card.boundEntityId = createdId;
                // History is diagnostic, and a created-card binding must not
                // accidentally attach to a different activation with same GUID.
                if (auto* snapshot = LatestSnapshotLocked(entry.effectEntityId, entry.effectGroupId)) {
                    for (auto& recorded : snapshot->selection.cards) {
                        if (recorded.entityId == -1 && recorded.cardGuid == guid && recorded.boundEntityId <= 0) {
                            recorded.boundEntityId = createdId;
                            LogSnapshotLocked("CREATED_BIND_OBSERVED", *snapshot);
                            break;
                        }
                    }
                }
                return true;
            }
        }
    }
    return false;
}

// Resolve the single, actual live entity from this exact transaction.
// This never returns a CardDatabase definition GUID or synthesized entity.
inline bool TryGetSingleLiveEntity(int effectId, int groupId, int& entityId, int& cardGuid) {
    entityId = -1;
    cardGuid = -1;
    if (effectId <= 0 || groupId < 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    for (const auto& entry : selections) {
        if (entry.effectEntityId != effectId || entry.effectGroupId != groupId) continue;
        if (entry.cards.size() != 1) return false;
        const auto& card = entry.cards.front();
        if (card.entityId <= 0 || card.cardGuid <= 0 || card.boundEntityId != -1)
            return false;
        entityId = card.entityId;
        cardGuid = card.cardGuid;
        return true;
    }
    return false;
}

// Resolve a single database-created entity for this exact completed selection.
// Never substitute a regular PickFromThree target or a CardDatabase GUID.
// Multiple cards, missing binding, or mismatched effect/group fail closed.
inline bool TryGetSingleBoundDatabaseEntity(int effectId, int groupId, int& entityId) {
    entityId = -1;
    if (effectId <= 0 || groupId < 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    for (const auto& entry : selections) {
        if (entry.effectEntityId != effectId || entry.effectGroupId != groupId) continue;
        if (entry.cards.size() != 1) return false;
        const SelectedCard& card = entry.cards.front();
        if (card.entityId != -1 || card.cardGuid <= 0 || card.boundEntityId <= 0)
            return false;
        entityId = card.boundEntityId;
        return true;
    }
    return false;
}

inline void ResetMatch() {
    DeferredLink::Reset();
    // Never carry a result into another match, where entity IDs can be reused.
    // During a match results are only replaced by Store(), not consumed
    // or cleared when the preview closes.
    std::lock_guard<std::mutex> guard(mutex);
    selections.clear();
    snapshotHistory.clear();
    nextSnapshotSequence = 1;
    droppedSnapshots = 0;
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_HISTORY: phase=RESET_MATCH history=0 current=0");
#endif
}

// Query evaluation can know its originating effect ID, but some stock
// CardFromSubsetFinder contexts do not expose an effect group. If there are
// multiple groups for an effect, NEVER guess which group was picked.
inline bool Matches(int effectId, int id, bool guidDomain) {
    if (effectId <= 0 || id <= 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    const Selection* selected = nullptr;
    for (const auto& entry : selections) {
        if (entry.effectEntityId != effectId) continue;
        if (selected) return false; // ambiguous: fail closed
        selected = &entry;
    }
    if (!selected) return false;
    for (const auto& card : selected->cards)
        if ((guidDomain ? card.cardGuid : card.entityId) == id) return true;
    return false;
}

} // namespace CustomComponents::Pick3::Picked
