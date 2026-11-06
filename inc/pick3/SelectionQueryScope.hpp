#include "diagnostics/DiagnosticLogging.hpp"
#include "core/HookRegistry.hpp"
#pragma once
// CustomComponents 2.5: read-only stock EntityFinder query-caller observation.
// No persistent pointer identity and no change to query return values.
// Effect::ToEntityQueryContext is intentionally NOT detoured: earlier trigger
// diagnostics report an unsafe managed call site; this build only resolves it.
#include <atomic>
#include <cstdint>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include "pick3/SelectionExecutionScope.hpp"
#include "log.h"

namespace CustomComponents::Pick3::QueryCaller {
using Obj = BNM::IL2CPP::Il2CppObject;
inline void (*originalRunQuery)(Obj*, Obj*, Obj*, Obj*, const void*) = nullptr;
inline BNM::Field<Obj*> finderContext;
inline BNM::Field<Obj*> finderModel;
inline BNM::Field<Obj*> queryEffectContext;
inline BNM::Field<Obj*> queryModel;
inline BNM::Field<int> effectContextId;
inline BNM::Field<int> effectContextSource;
inline BNM::Field<int> effectContextTarget;
inline std::atomic<unsigned> finderCalls{0};
inline std::atomic<unsigned> missingContext{0};
inline std::atomic<unsigned> queryMatches{0};
inline std::atomic<unsigned> queryMismatches{0};
inline std::atomic<unsigned> queryOrphans{0};
inline std::atomic<unsigned> selectionSamples{0};
inline constexpr unsigned kFinderCap = 256;
struct CallerState {
    Obj* context = nullptr;  // call-local only, not a persistent cache key
    Obj* model = nullptr;
    int effect = -1;
    unsigned depth = 0;
    unsigned event = 0;
};
inline thread_local CallerState active{};
struct Scope {
    CallerState prior;
    explicit Scope(CallerState current):prior(active){ current.depth=prior.depth+1;active=current; }
    ~Scope(){active=prior;}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
inline int ReadEffect(Obj* context) {
    if (!context || !queryEffectContext.IsValid() || !effectContextId.IsValid()) return -1;
    Obj* effect = queryEffectContext[context].Get();
    return effect ? effectContextId[effect].Get() : -1;
}
inline void ObserveQuery(Obj* context, const char* domain, int effect) {
    if constexpr(!Diagnostics::kSelectionTrace) return;
    const auto caller=active;
    if (!caller.depth) {
        const unsigned n=++queryOrphans;
        if(n<=48) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUERY_CALLER_MISSING domain=%s context=%p effect=%d reason=NO_RUNQUERY_SCOPE readOnly=1",domain,(void*)context,effect);
        return;
    }
    const bool same=context && caller.context==context && caller.effect==effect;
    const unsigned n=same?++queryMatches:++queryMismatches;
    if(n<=128) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=%s domain=%s event=%u queryContext=%p callerContext=%p contextEffect=%d callerEffect=%d callerModel=%p callerDepth=%u reason=%s readOnly=1",same?"QUERY_CALLER_MATCH":"CONTEXT_EFFECT_MISMATCH",domain,caller.event,(void*)context,(void*)caller.context,effect,caller.effect,(void*)caller.model,caller.depth,same?"IDENTICAL_CONTEXT_AND_EFFECT":"POINTER_OR_EFFECT_DIFFERENT");
}
inline void RunQuery(Obj* self, Obj* query, Obj* context, Obj* matches, const void* method) {
    const unsigned n=++finderCalls;
    Obj* model=self && finderModel.IsValid()?finderModel[self].Get():nullptr;
    Obj* stored=self && finderContext.IsValid()?finderContext[self].Get():nullptr;
    Obj* contextModel=context && queryModel.IsValid()?queryModel[context].Get():nullptr;
    const int effect=ReadEffect(context);
    const bool mismatch=stored && context && stored!=context;
    if(n<=kFinderCap) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=FINDER_ENTER event=%u finder=%p query=%p context=%p storedContext=%p finderModel=%p contextModel=%p effect=%d storedContextMatches=%d scopeEffect=%d scopeGroup=%d scopeSeq=%llu readOnly=1",n,(void*)self,(void*)query,(void*)context,(void*)stored,(void*)model,(void*)contextModel,effect,int(!mismatch),Provenance::active.effect,Provenance::active.group,(unsigned long long)Provenance::active.snapshot);
    if(!context && ++missingContext<=48) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=FINDER_CONTEXT_MISSING event=%u reason=NULL_CONTEXT readOnly=1",n);
    if(mismatch && n<=kFinderCap) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=FINDER_CONTEXT_OVERRIDE event=%u reason=STOCK_EXTERNAL_CONTEXT readOnly=1",n);
    Scope scope({context,contextModel?contextModel:model,effect,0,n});
    if(originalRunQuery) originalRunQuery(self,query,context,matches,method);
    else LOG_ERR("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=ORIGINAL_MISSING boundary=ENTITY_FINDER_RUNQUERY event=%u",n);
    if(n<=kFinderCap) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=FINDER_EXIT event=%u effect=%d context=%p restored=1 readOnly=1",n,effect,(void*)context);
}
inline void ObserveSelectState(const char* phase,int effect,int group,int selectPhase,int count,int first) {
    const unsigned n=++selectionSamples;
    if(n<=512) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=COMMIT_EVIDENCE stage=%s event=%u effect=%d group=%d selectPhase=%d targets=%d firstTarget=%d stockStateObserved=1 COMMIT_NOT_PROVEN=1 readOnly=1",phase,n,effect,group,selectPhase,count,first);
}
inline void Initialize() {
    auto finder=BNM::Class("PvZCards.Engine","EntityFinder");
    auto query=BNM::Class("PvZCards.Engine","EntityQueryContext");
    auto effect=BNM::Class("PvZCards.Engine","EffectContext");
    auto stockEffect=BNM::Class("PvZCards.Engine","Effect");
    finderContext=finder.GetField("QueryContext").cast<Obj*>();
    finderModel=finder.GetField("Model").cast<Obj*>();
    queryEffectContext=query.GetField("EffectContext").cast<Obj*>();
    queryModel=query.GetField("<Model>k__BackingField").cast<Obj*>();
    effectContextId=effect.GetField("EffectEntityId").cast<int>();
    effectContextSource=effect.GetField("SourceId").cast<int>();
    effectContextTarget=effect.GetField("TargetId").cast<int>();
    auto run=finder.GetMethod("RunQuery",3);
    auto toContext=stockEffect.GetMethod("ToEntityQueryContext",1);
    if(run.IsValid()) CustomComponents::Core::Hooks::Install(run,RunQuery,originalRunQuery);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUERY_CALLER_INIT version=2.5 runResolved=%d runInstalled=%d effectToQueryContextResolved=%d effectContextHooked=0 reason=PRIOR_UNSAFE_TRIGGER_CALL fieldFinderContext=%d fieldQueryContext=%d fieldEffectId=%d readOnly=1",int(run.IsValid()),int(originalRunQuery!=nullptr),int(toContext.IsValid()),int(finderContext.IsValid()),int(queryEffectContext.IsValid()),int(effectContextId.IsValid()));
    if(!run.IsValid() || !originalRunQuery) LOG_ERR("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=HOOK_FAILURE method=EntityFinder.RunQuery reason=%s queryResultsUnchanged=1",run.IsValid()?"DETOUR_NOT_INSTALLED":"METHOD_NOT_RESOLVED");
}
} // namespace CustomComponents::Pick3::QueryCaller
