#pragma once
namespace CustomComponents::Pick3 {
inline bool MatchesPhysicalSelection(int expectedEntity,int expectedGuid,int actualEntity,int actualGuid) {
    return expectedEntity>0 && expectedGuid>0 && expectedEntity==actualEntity && expectedGuid==actualGuid;
}
struct SelectionResolution { bool matched=false; const char* origin="COMPATIBILITY"; bool transaction=false; };
// Each callback returns whether it owns the result. A false match can still
// be owned, so lower-priority history must never turn it into a positive.
template<class Transaction,class Listener,class Event,class Finder>
inline SelectionResolution ResolveSelectionByPriority(bool legacy,Transaction transaction,
        Listener listener,Event event,Finder finder) {
    bool result=false;
    if(transaction(result)) return {result,"TRANSACTION",true};
    if(listener(result)) return {result,"LISTENER_LINK",false};
    if(event(result)) return {result,"ACCEPTED_EVENT",false};
    if(finder(result)) return {result,"FINDER_SNAPSHOT",false};
    return {legacy,"COMPATIBILITY",false};
}
}
