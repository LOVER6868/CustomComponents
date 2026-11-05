#pragma once
// Patch 1.7.20: opt-in read-only observation of the stock pre-Play lifecycle.
// IMPORTANT: DetermineCurrentInputState and BindCommandsForState are ALREADY
// detoured by TargetButtonTrace. Those existing hooks call the observers here;
// NEVER install another hook on either RVA or on GameInputStateHolder.Update.
// No retained HeroesCommand, gesture, DragModel or managed list pointers.
// No IL2CPP method invocation, object allocation, state mutation or replay.
#include <cstdint>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include "log.h"

namespace CustomComponents::Pick3::PrePlayLifecycleTrace {
using Obj = BNM::IL2CPP::Il2CppObject;
using IntList = BNM::Structures::Mono::List<int>;
using ExecuteFn = void (*)(Obj*, const void*);
inline ExecuteFn originalHighlight = nullptr;
inline BNM::Field<int> holderState;
inline BNM::Field<Obj*> holderDrag;
inline BNM::Field<int> dragCard;
inline BNM::Field<bool> dragInHand;
inline BNM::Field<IntList*> dragTargets;
inline BNM::Field<int> highlightState;
inline BNM::Field<Obj*> highlightDrag;
inline bool enabled = false;
// Unity normally drives these from one input thread. Thread-local budgets and
// observations avoid shared mutable per-frame state if prediction runs elsewhere.
inline thread_local unsigned events = 0;
inline constexpr unsigned kMaxEvents = 640;
inline thread_local int lastObservedState = -99;
inline thread_local int lastObservedCard = -99;
inline thread_local unsigned playObservations = 0;
// Scope the already-installed StopCardDragCommand.Execute trampoline. This is
// NOT a retained command or a hook on the signal dispatcher. A Play observed
// with releaseDepth > 0 ran synchronously within stock StopDrag execution on
// the same thread; zero does not establish that it was dispatched elsewhere.
inline thread_local unsigned releaseDepth = 0;
inline thread_local unsigned releaseSerial = 0;
inline thread_local unsigned releasePlayCount = 0;
inline constexpr unsigned kMaxReleaseDepth = 16;
inline thread_local int lastHighlightCard = -99;
inline thread_local int lastHighlightState = -99;
inline thread_local int lastHighlightCount = -99;
inline thread_local int lastHighlightFirst = -99;
inline thread_local int lastHighlightSecond = -99;

inline int Card(Obj* drag) {
    return drag && dragCard.IsValid() ? dragCard[drag].Get() : -1;
}
inline int Count(Obj* drag, int& first, int& second) {
    first = second = -1;
    if (!drag || !dragTargets.IsValid()) return -1;
    auto* list = dragTargets[drag].Get();
    // The field is System.Collections.Generic.List<int>, never an opaque
    // IEnumerable or a pooled iterator. Only inspect it synchronously.
    if (!list || list->size < 0 || list->size > 128 ||
        (list->size && !list->items)) return -1;
    if (list->size >= 1) first = list->items->m_Items[0];
    if (list->size >= 2) second = list->items->m_Items[1];
    return list->size;
}
inline bool CanLog() { return enabled && events < kMaxEvents; }
inline bool IsTargeting(int state) { return state == 5 || state == 6 || state == 7; }
inline void Log(const char* stage, int prior, int next, Obj* drag,
                int stateField = -99) {
    if (!CanLog()) return;
    const int card = Card(drag);
    int first=-1, second=-1;
    const int count=Count(drag,first,second);
    const int hand=drag && dragInHand.IsValid() ? int(dragInHand[drag].Get()) : -1;
    LOG_INFO("CustomComponents/Pick3/PREPLAY1720: n=%u stage=%s prior=%d next=%d field=%d "
             "dragCard=%d inHand=%d targetCount=%d targetsFirst=[%d,%d] "
             "readOnly=1 retainedCommand=0",
             ++events,stage,prior,next,stateField,card,hand,count,first,second);
}
inline void OnDetermine(Obj* holder, int before, int decided) {
    if (!enabled || !holder || !holderDrag.IsValid()) return;
    Obj* drag = holderDrag[holder].Get();
    const int card = Card(drag);
    // Always log transitions involving state 5, plus the first observed
    // decision for a new dragged entity. No expensive GUID lookup in this hook.
    if (IsTargeting(before) || IsTargeting(decided) ||
        decided != before || (card >= 0 && card != lastObservedCard)) {
        if (decided != lastObservedState || before != decided ||
            card != lastObservedCard) {
            Log("DETERMINE",before,decided,drag,before);
            lastObservedState = decided;
            lastObservedCard = card;
        }
    }
}
inline void OnBind(Obj* holder, int prior, int state, bool after) {
    if (!enabled || !holder || !holderDrag.IsValid()) return;
    Obj* drag = holderDrag[holder].Get();
    if (IsTargeting(prior) || IsTargeting(state) || Card(drag) >= 0) {
        const int actual = holderState.IsValid() ? holderState[holder].Get() : -99;
        Log(after ? "BIND_EXIT" : "BIND_ENTER",prior,state,drag,actual);
    }
}
// A separate, long stock method (RVA 0x1F6F04C in the provided dump).
// Execute is observed exactly once, before and after the original. Do not
// call HighlightPlayableLocationsOrTargetsForCardCommand.Execute manually.
inline void Highlight(Obj* self, const void* method) {
    int beforeCard=-1, beforeState=-99;
    bool entering=false;
    if (enabled && self && highlightDrag.IsValid()) {
        Obj* drag=highlightDrag[self].Get();
        beforeCard=Card(drag);
        beforeState=highlightState.IsValid() ? highlightState[self].Get() : -99;
        entering=(IsTargeting(beforeState) || beforeCard>=0) &&
            (beforeCard!=lastHighlightCard || beforeState!=lastHighlightState);
        if (entering) Log("HIGHLIGHT_ENTER",beforeState,beforeState,drag,beforeState);
    }
    if (originalHighlight) originalHighlight(self,method);
    if (enabled && self && highlightDrag.IsValid()) {
        Obj* drag=highlightDrag[self].Get();
        const int afterState=highlightState.IsValid() ? highlightState[self].Get() : -99;
        const int afterCard=Card(drag);
        int first=-1, second=-1;
        const int count=Count(drag,first,second);
        // Changes in the legal target list are significant; identical
        // per-frame highlight refreshes are deliberately coalesced.
        const bool changed=afterState!=lastHighlightState ||
            afterCard!=lastHighlightCard || count!=lastHighlightCount ||
            first!=lastHighlightFirst || second!=lastHighlightSecond;
        if ((entering || changed) &&
            (IsTargeting(afterState) || afterCard>=0))
            Log("HIGHLIGHT_EXIT",beforeState,afterState,drag,afterState);
        lastHighlightCard=afterCard;
        lastHighlightState=afterState;
        lastHighlightCount=count;
        lastHighlightFirst=first;
        lastHighlightSecond=second;
    }
}
inline void OnStopDrag(int card, bool after) {
    // These calls surround the existing single originalTrickStopDrag invocation.
    // Maintain scope even after the trace budget is exhausted; clamp excessive
    // nesting so bad metadata cannot wrap the depth counter.
    if (!enabled) return;
    if (!after) {
        if (releaseDepth < kMaxReleaseDepth) ++releaseDepth;
        if (releaseDepth == 1) { ++releaseSerial; releasePlayCount = 0; }
    }
    if (CanLog()) {
        LOG_INFO("CustomComponents/Pick3/PREPLAY1721: n=%u stage=%s card=%d "
                 "releaseDepth=%u releaseSerial=%u playsInside=%u "
                 "readOnly=1 replay=0", ++events,
                 after ? "RELEASE_SCOPE_EXIT" : "RELEASE_SCOPE_ENTER",
                 card,releaseDepth,releaseSerial,releasePlayCount);
    }
    // Preserve the existing 1.7.20 event names for log correlation.
    if (CanLog() && card >= 0)
        LOG_INFO("CustomComponents/Pick3/PREPLAY1720: n=%u stage=%s card=%d "
                 "readOnly=1 replay=0",++events,
                 after ? "STOP_DRAG_EXIT" : "STOP_DRAG_ENTER",card);
    if (after && releaseDepth) --releaseDepth;
}
inline void OnPlay(int card, int target, int handled, bool after) {
    if (!CanLog() || card < 0) return;
    // The BEFORE callback must run AFTER any pre-existing 1.7.19 correction,
    // immediately before originalPlayCardExecute. It reports the outbound
    // gesture; it does not modify the gesture or the Play service.
    ++playObservations;
    if (!after && releaseDepth) ++releasePlayCount;
    if (CanLog())
        LOG_INFO("CustomComponents/Pick3/PREPLAY1721: n=%u stage=%s card=%d target=%d "
                 "handled=%d releaseDepth=%u releaseSerial=%u "
                 "insideStopDrag=%d readOnly=1", ++events,
                 after ? "PLAY_RETURN" : "PLAY_ENTER",card,target,handled,
                 releaseDepth,releaseSerial,int(releaseDepth>0));
    if (!CanLog()) return;
    LOG_INFO("CustomComponents/Pick3/PREPLAY1720: n=%u stage=%s card=%d target=%d "
             "handled=%d playObservation=%u stockForward=1 readOnly=1",
             ++events,after ? "PLAY_EXEC_RETURN" : "PLAY_EXEC_ENTER",card,target,
             handled,playObservations);
}
inline bool Configure(bool requested) {
    enabled = false;
    events = playObservations = 0;
    releaseDepth = releaseSerial = releasePlayCount = 0;
    lastObservedState = lastObservedCard = -99;
    lastHighlightCard=lastHighlightState=lastHighlightCount=-99;
    lastHighlightFirst=lastHighlightSecond=-99;
    if (!requested) {
        LOG_INFO("CustomComponents/Pick3/PREPLAY1720: SETUP enabled=0 duplicateStateHooks=0");
        return true;
    }
    auto holder=BNM::Class("PvZCards.Game","GameInputStateHolder");
    auto drag=BNM::Class("PvZCards.Game","DragModel");
    auto highlight=BNM::Class("PvZCards.Game","HighlightPlayableLocationsOrTargetsForCardCommand");
    holderState=holder.GetField("GameInputState").cast<int>();
    holderDrag=holder.GetField("<DragModel>k__BackingField").cast<Obj*>();
    dragCard=drag.GetField("CardEntityId").cast<int>();
    dragInHand=drag.GetField("InHand").cast<bool>();
    dragTargets=drag.GetField("Targetids").cast<IntList*>();
    highlightState=highlight.GetField("<GameInputState>k__BackingField").cast<int>();
    highlightDrag=highlight.GetField("<DragModel>k__BackingField").cast<Obj*>();
    auto execute=highlight.GetMethod("Execute",0);
    // Fail closed: never activate a partial probe with unknown field layouts.
    const bool valid=holderState.IsValid() && holderDrag.IsValid() &&
        dragCard.IsValid() && dragInHand.IsValid() && dragTargets.IsValid() &&
        highlightState.IsValid() && highlightDrag.IsValid() && execute.IsValid();
    if (!valid) {
        LOG_ERR("CustomComponents/Pick3/PREPLAY1720: metadata missing; tracing disabled; gameplay unaffected");
        return false;
    }
    // This method is not hooked by any existing CustomComponents module.
    // Configure is expected once at startup; avoid ever stacking ourselves.
    if (!originalHighlight) BNM::BasicHook(execute,Highlight,originalHighlight);
    enabled = originalHighlight != nullptr;
    LOG_INFO("CustomComponents/Pick3/PREPLAY1720: SETUP enabled=%d highlight=%d "
             "sharedDetermine=1 sharedBind=1 duplicateStateHooks=0 "
             "commandReplay=0 gestureMutation=0 budget=%u",
             int(enabled),int(originalHighlight!=nullptr),kMaxEvents);
    return enabled;
}
} // namespace CustomComponents::Pick3::PrePlayLifecycleTrace
