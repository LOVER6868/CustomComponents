#pragma once
#ifndef CUSTOMCOMPONENTS_PF3_INDEPENDENT_LEGACY_QUERIES
#define CUSTOMCOMPONENTS_PF3_INDEPENDENT_LEGACY_QUERIES 1
#endif
namespace CustomComponents::Pick3 {
inline bool MatchesPhysicalSelection(int expectedEntity,int expectedGuid,int actualEntity,int actualGuid) {
    return expectedEntity>0 && expectedGuid>0 && expectedEntity==actualEntity && expectedGuid==actualGuid;
}
enum class ResolutionKind { Transaction, Listener, AcceptedEvent, Finder, IndependentLegacy, Unbound };
struct SelectionResolution {
    bool matched=false;
    const char* origin="COMPATIBILITY";
    bool transaction=false;
    ResolutionKind kind=ResolutionKind::IndependentLegacy;
};
// Each callback returns whether it owns the result. A false match can still
// be owned, so lower-priority history must never turn it into a positive.
template<class Transaction,class Listener,class Event,class Finder>
inline SelectionResolution ResolveSelectionByPriority(bool legacy,Transaction transaction,
        Listener listener,Event event,Finder finder,
        bool allowIndependentLegacy=CUSTOMCOMPONENTS_PF3_INDEPENDENT_LEGACY_QUERIES!=0) {
    bool result=false;
    if(transaction(result)) return {result,"TRANSACTION",true,ResolutionKind::Transaction};
    if(listener(result)) return {result,"LISTENER_LINK",false,ResolutionKind::Listener};
    if(event(result)) return {result,"ACCEPTED_EVENT",false,ResolutionKind::AcceptedEvent};
    if(finder(result)) return {result,"FINDER_SNAPSHOT",false,ResolutionKind::Finder};
    return allowIndependentLegacy
        ? SelectionResolution{legacy,"COMPATIBILITY",false,ResolutionKind::IndependentLegacy}
        : SelectionResolution{false,"UNBOUND",false,ResolutionKind::Unbound};
}
}
