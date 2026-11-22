#include "diagnostics/DiagnosticLogging.hpp"
#include "core/HookRegistry.hpp"
#pragma once
// Stock record/animation boundary trace ONLY. No synthetic record/signal,
// state machine transitions, or repetition of Process().
#include <atomic>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "log.h"
#include "diagnostics/TraceVerbosity.hpp"
namespace CustomComponents::Pick3::PhaseRecordTrace {
using Obj=BNM::IL2CPP::Il2CppObject;
using RecordFn=void(*)(Obj*,Obj*,const void*);
using VoidFn=void(*)(Obj*,const void*);
inline RecordFn originalEnd=nullptr,originalResumeRecord=nullptr,originalFinish=nullptr;
inline VoidFn originalDispatch=nullptr;
inline BNM::Method<int> recordEffectId,recordGroupId;
inline std::atomic<unsigned long long> seq{0};
inline std::atomic<unsigned long long> endCount{0},resumeRecordCount{0},dispatchCount{0},finishCount{0};
inline void End(Obj* processor, Obj* record, const void* method) {
    // The counts are read by live recovery logic; never gate them behind logs.
    const auto ends = endCount.fetch_add(1) + 1;
    if constexpr (CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE != 0) {
        int effect = -1, group = -1;
        if (record && recordEffectId.IsValid() && recordGroupId.IsValid()) {
            auto check = BNM::TryInvoke([&] {
                effect = recordEffectId[record]();
                group = recordGroupId[record]();
            });
            if (check.IsValid()) { effect = -1; group = -1; }
        }
        CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=CHOOSE_END_ENTER processor=%p record=%p effect=%d group=%d ends=%llu resumes=%llu dispatches=%llu",
            seq.fetch_add(1)+1, (void*)processor, (void*)record, effect, group, ends,
            resumeRecordCount.load(), dispatchCount.load());
    }
    if (originalEnd) originalEnd(processor, record, method);
    CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=CHOOSE_END_EXIT processor=%p",
        seq.fetch_add(1)+1, (void*)processor);
}
inline void ResumeRecord(Obj* processor, Obj* record, const void* method) {
    const auto resumes = resumeRecordCount.fetch_add(1) + 1;
    CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=RESUME_RECORD_ENTER processor=%p record=%p ends=%llu resumes=%llu dispatches=%llu",
        seq.fetch_add(1)+1, (void*)processor, (void*)record,
        endCount.load(), resumes, dispatchCount.load());
    if (originalResumeRecord) originalResumeRecord(processor, record, method);
    CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=RESUME_RECORD_EXIT processor=%p",
        seq.fetch_add(1)+1, (void*)processor);
}
inline void Dispatch(Obj* processor, const void* method) {
    const auto dispatches = dispatchCount.fetch_add(1) + 1;
    CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=RESUME_DISPATCH_ENTER processor=%p ends=%llu resumes=%llu dispatches=%llu",
        seq.fetch_add(1)+1, (void*)processor, endCount.load(), resumeRecordCount.load(), dispatches);
    if (originalDispatch) originalDispatch(processor, method);
    CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=RESUME_DISPATCH_EXIT processor=%p",
        seq.fetch_add(1)+1, (void*)processor);
}
inline void Finish(Obj* service, Obj* blockingEffect, const void* method) {
    const auto finishes = finishCount.fetch_add(1) + 1;
    CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=LOCAL_FINISH_ENTER service=%p effect=%p count=%llu",
        seq.fetch_add(1)+1, (void*)service, (void*)blockingEffect, finishes);
    if (originalFinish) originalFinish(service, blockingEffect, method);
    CC_HOOK_TRACE("CustomComponents/PHASE_RECORD: seq=%llu stage=LOCAL_FINISH_EXIT service=%p",
        seq.fetch_add(1)+1, (void*)service);
}
inline void Setup() {
    auto processor=BNM::Class("PvZCards.Game","PhaseStartRecordProcessor");
    auto svc=BNM::Class("PvZCards.Core.Services","LocalSelectTargetService");
    auto recordClass=BNM::Class("PvZCards.Engine.Records","EffectRecord");
    recordEffectId=recordClass.GetMethod("get_EffectEntityId",0).cast<int>();
    recordGroupId=recordClass.GetMethod("get_EffectGroupId",0).cast<int>();
    auto hook=[](auto m,auto fn,auto& old){if(m.IsValid())CustomComponents::Core::Hooks::Install(m,fn,old);};
    hook(processor.GetMethod("ProcessChooseTargetPhaseEndRecord",1),End,originalEnd);
    hook(processor.GetMethod("ProcessPhaseResumeRecord",1),ResumeRecord,originalResumeRecord);
    hook(processor.GetMethod("DispatchPhaseResumeSignal",0),Dispatch,originalDispatch);
    hook(svc.GetMethod("FinishChooseTargetPhase",1),Finish,originalFinish);
    CC_SELECTION_DIAG("CustomComponents/PHASE_RECORD: SETUP chooseEnd=%d resumeRecord=%d dispatch=%d localFinish=%d recordFields=%d readOnly=1",!!originalEnd,!!originalResumeRecord,!!originalDispatch,!!originalFinish,!!(recordEffectId.IsValid()&&recordGroupId.IsValid()));
}
}
