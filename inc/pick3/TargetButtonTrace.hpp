#pragma once
// Read-only diagnostic detours. Each original is called exactly once.
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include "pick3/PhaseRecordTrace.hpp"
#include "pick3/PrePlayLifecycleTrace.hpp"
#include "core/SceneViewContext.hpp"
#include "log.h"
#include "diagnostics/TraceVerbosity.hpp"
#include "diagnostics/Pf3RepairTrace.hpp"
namespace CustomComponents::Pick3::TargetButtonTrace {
using Obj=BNM::IL2CPP::Il2CppObject;
using VoidFn=void(*)(Obj*,const void*);
using StateFn=void(*)(Obj*,int,const void*);
using EffectFn=Obj*(*)(Obj*,const void*);
using DetermineFn=int(*)(Obj*,const void*);
using StateFromEffectFn=int(*)(Obj*,Obj*,const void*);
using BoolFn=bool(*)(Obj*,const void*);
using AnimFn=void(*)(Obj*,int,int,int,const void*);
inline VoidFn oldChoose=nullptr,oldResume=nullptr,oldSurprise=nullptr,oldDone=nullptr,oldOptional=nullptr;
inline StateFn oldInput=nullptr,oldBind=nullptr;
inline EffectFn oldFirst=nullptr;
inline DetermineFn oldDetermine=nullptr;
inline StateFromEffectFn oldStateFromEffect=nullptr;
inline VoidFn oldUnbind=nullptr;
inline BoolFn oldCare=nullptr;
inline AnimFn oldAnim=nullptr;
inline void(*oldActivate)(Obj*,bool,const void*)=nullptr;
inline void(*oldTriggeredStart)(Obj*,Obj*,const void*)=nullptr;
inline BNM::Field<int> machineCurrentState;
inline BNM::Field<Obj*> viewAnimator;
inline unsigned firstPolls=0;
inline bool machineFieldDiscoveryLogged=false;
inline bool machineTypeDiscoveryLogged=false;
inline const char* StateName(int state) {
 switch(state) { case 0:return "Disabled"; case 1:return "OptionalEarlyEnd";
 case 2:return "Done"; case 3:return "Interrupted"; default:return "Unknown"; }
}
inline int ReadMachineState(Obj* machine) {
 if (!machine) return -99;
 // GenericStateMachine<EndTurnStates, EndTurnEvents> is a closed generic.
 // Resolve the field against the actual object; the open generic definition
 // doesn't necessarily carry usable field offsets in IL2CPP/BNM.
 if (!machineCurrentState.IsValid()) {
   BNM::Class concrete(machine);
   if (concrete.IsValid()) machineCurrentState=concrete.GetField("CurrentState").cast<int>();
   if (!machineFieldDiscoveryLogged || machineCurrentState.IsValid()) {
      LOG_INFO("CustomComponents/BUTTON_FSM: DISCOVER machine=%p concrete=%d currentField=%d",(void*)machine,int(concrete.IsValid()),int(machineCurrentState.IsValid()));
      machineFieldDiscoveryLogged=true;
   }
 }
 return machineCurrentState.IsValid()?machineCurrentState[machine].Get():-99;
}

inline BNM::Field<int> holderState, effectEntity, effectGroup, phaseSubturn;
inline BNM::Field<Obj*> medPhase,medMachine,medView,medResumeSignal,medTargetStartSignal;
inline unsigned seq=0;
// Keep a managed root of the mediator only while the match is present.
// A naked pointer can be invalid after changing a scene.
inline std::uint32_t recoveryMediatorRoot=0;
inline bool enableGuardedReconcile=true;
inline const void* lastCareMethod=nullptr;
inline const void* lastResumeMethod=nullptr;
inline unsigned chooseSerial=0;
inline unsigned recoveredSerial=0;
// Evidence from the actual stock mediator input callbacks. All manual
// targeting (ordinary and PF3) must be eligible, not just popup completion.
inline bool sawTargetingInput=false, returnedFromTargeting=false;
inline unsigned recoveryDebugBudget=0;
inline unsigned long long endsAtChoose=0,recordsAtChoose=0;
inline int readyRecoveryFrames=0;
inline void ClearRecovery() {
    if(recoveryMediatorRoot && Core::SceneViewContext::freeGCHandle)
        Core::SceneViewContext::freeGCHandle(recoveryMediatorRoot);
    recoveryMediatorRoot=0;
    readyRecoveryFrames=0;
    chooseSerial=recoveredSerial=0;
    sawTargetingInput=returnedFromTargeting=false;
    endsAtChoose=recordsAtChoose=0;
}
inline void RememberChoose(Obj* mediator) {
    ++chooseSerial;
    readyRecoveryFrames=0;
    sawTargetingInput=returnedFromTargeting=false;
    PF3_REPAIR_TRACE("CustomComponents/BUTTON179: CHOOSE_START serial=%u mediator=%p endRecords=%llu resumeRecords=%llu",
        chooseSerial,(void*)mediator,PhaseRecordTrace::endCount.load(),PhaseRecordTrace::resumeRecordCount.load());
    endsAtChoose=PhaseRecordTrace::endCount.load();
    recordsAtChoose=PhaseRecordTrace::resumeRecordCount.load();
    if(!mediator || !Core::SceneViewContext::newGCHandle) return;
    Obj* retained=recoveryMediatorRoot && Core::SceneViewContext::getGCHandle
        ? Core::SceneViewContext::getGCHandle(recoveryMediatorRoot) : nullptr;
    if(retained!=mediator) {
        if(recoveryMediatorRoot && Core::SceneViewContext::freeGCHandle)
            Core::SceneViewContext::freeGCHandle(recoveryMediatorRoot);
        recoveryMediatorRoot=Core::SceneViewContext::newGCHandle(mediator,false);
    }
}
// This does not change the FSM or fabricate a gameplay resume record. It
// requests the STOCK mediator's usual animation exactly once when a local
// phase end has been seen and no stock resume-record path has been scheduled.
// The stock phase-care predicate must explicitly approve the operation.

inline int lastId=-999,lastGroup=-999;
inline int currentState=-999;
inline Obj* lastFirst=nullptr;
// An effect identity transition in the same targeting state is a real stock
// command handoff, NOT permission to invoke a stale finder or open the UI.
inline int handoffId=-999, handoffGroup=-999;
inline unsigned handoffSeq=0;
inline bool handoffAwaitingDiscovery=false;
inline void ObserveFinderEntry(int id,int group) {
 if(handoffAwaitingDiscovery && id==handoffId && group==handoffGroup) {
   CC_HOOK_TRACE("CustomComponents/HANDOFF_TRACE: seq=%u stage=FINDER_AFTER_BIND effect=%d group=%d bindSeq=%u",++seq,id,group,handoffSeq);
   handoffAwaitingDiscovery=false;
 }
}

inline int Phase(Obj* self){if(!self||!medPhase.IsValid()||!phaseSubturn.IsValid())return -99;auto p=medPhase[self].Get();return p?phaseSubturn[p].Get():-98;}
inline void Snapshot(const char* tag,Obj* self){
 if constexpr (CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE == 0) return;
 Obj* machine=self&&medMachine.IsValid()?medMachine[self].Get():nullptr;
 Obj* view=self&&medView.IsValid()?medView[self].Get():nullptr;
 int state=ReadMachineState(machine);
 Obj* animator=view&&viewAnimator.IsValid()?viewAnimator[view].Get():nullptr;
 CC_HOOK_TRACE("CustomComponents/STATE_TRACE: seq=%u tag=%s mediator=%p subturn=%d machine=%p machineState=%d machineName=%s view=%p animator=%p lastInput=%d lastEffect=%d/%d",++seq,tag,(void*)self,Phase(self),(void*)machine,state,StateName(state),(void*)view,(void*)animator,currentState,lastId,lastGroup);
}
inline void TryRecoverAfterCompletedLocalPhase(bool noPickerTransaction, bool matchLocalService) {
    if (!enableGuardedReconcile || !noPickerTransaction || !matchLocalService || !oldResume || !oldCare ||
        !recoveryMediatorRoot || chooseSerial==recoveredSerial || chooseSerial==0) {
        readyRecoveryFrames=0;
        return;
    }
    // A stock phase-resume record supersedes us. Never schedule another.
    if (PhaseRecordTrace::resumeRecordCount.load()>recordsAtChoose) {
        recoveredSerial=chooseSerial;
        readyRecoveryFrames=0;
        PF3_REPAIR_TRACE("CustomComponents/BUTTON179: STOCK_RESUME_ALREADY_QUEUED serial=%u",chooseSerial);
        return;
    }
    const bool observedEndRecord=PhaseRecordTrace::endCount.load()>endsAtChoose;
    // Some stock manual target phases never emit a phase-end record. Only
    // recognize those via an actual target-state -> normal-state callback,
    // not from a timer or a PF3 popup closing.
    if (!observedEndRecord && !returnedFromTargeting) {
        readyRecoveryFrames=0;
        return;
    }
    // Never resume while phase-0/phase-1 targeting or another blocker is live.
    if (currentState!=3 && currentState!=4) {
        readyRecoveryFrames=0;
        return;
    }
    Obj* mediator=Core::SceneViewContext::getGCHandle
        ? Core::SceneViewContext::getGCHandle(recoveryMediatorRoot) : nullptr;
    if(!mediator || Phase(mediator)!=0) {
        readyRecoveryFrames=0;
        return;
    }
    Obj* machine=medMachine.IsValid()?medMachine[mediator].Get():nullptr;
    const int machineState=ReadMachineState(machine);
    // Only restore stock's Interrupted state; never force activation of a
    // disabled/done/optional button, nor alter the state-machine directly.
    const bool phaseCare=machineState==3 && oldCare(mediator,lastCareMethod);
    if (!machine || machineState!=3 || !phaseCare) {
        if (++recoveryDebugBudget<=72)
            PF3_REPAIR_TRACE("CustomComponents/BUTTON179: RESTORE_GUARD serial=%u state=%d fsm=%d care=%d subturn=%d",
                 chooseSerial,currentState,machineState,int(phaseCare),Phase(mediator));
        readyRecoveryFrames=0;
        return;
    }
    if (++readyRecoveryFrames<10) {
        if(readyRecoveryFrames==1)
            PF3_REPAIR_TRACE("CustomComponents/BUTTON179: RECOVERY_ELIGIBLE serial=%u endRecord=%d inputExit=%d fsm=%d holder=%d",
                chooseSerial,int(observedEndRecord),int(returnedFromTargeting),machineState,currentState);
        return;
    }
    recoveredSerial=chooseSerial; // mark before stock invocation to prevent reentry
    readyRecoveryFrames=0;
    PF3_REPAIR_TRACE("CustomComponents/BUTTON179: STOCK_MEDIATOR_RESTORE serial=%u endRecord=%d inputExit=%d resumeRecord=0 fsm=%d holder=%d",
        chooseSerial,int(observedEndRecord),int(returnedFromTargeting),machineState,currentState);
    oldResume(mediator,lastResumeMethod); // stock schedules its own animation
}

// Stock observer detours still forward each native invocation once.
// The opt-out recovery above runs ONLY after a completed target phase with
// no stock resume record, a stable eligible local phase and FSM Interrupted.
// Read-only hooks at stock decision boundaries. Never re-enter any finder,
// signal or state transition, and forward each invocation exactly once.
inline int Determine(Obj* s,const void* m){
 int before=s&&holderState.IsValid()?holderState[s].Get():-99;
 int decided=oldDetermine?oldDetermine(s,m):-99;
 PrePlayLifecycleTrace::OnDetermine(s,before,decided);
 if(decided!=before || decided==6 || decided==7)
   CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=DETERMINE holder=%p prior=%d decided=%d front=%d/%d",++seq,(void*)s,before,decided,lastId,lastGroup);
 return decided;
}
inline int EffectState(Obj* s,Obj* e,const void* m){
 int id=e&&effectEntity.IsValid()?effectEntity[e].Get():-1;
 int group=e&&effectGroup.IsValid()?effectGroup[e].Get():-1;
 int decided=oldStateFromEffect?oldStateFromEffect(s,e,m):-99;
 CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=EFFECT_TO_STATE holder=%p effect=%p id=%d group=%d decided=%d",++seq,(void*)s,(void*)e,id,group,decided);
 return decided;
}
inline void Unbind(Obj* s,const void* m){
 CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=UNBIND_ENTER holder=%p state=%d front=%d/%d",++seq,(void*)s,s&&holderState.IsValid()?holderState[s].Get():-99,lastId,lastGroup);
 if(oldUnbind)oldUnbind(s,m);
 CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=UNBIND_EXIT holder=%p",++seq,(void*)s);
}
inline void Choose(Obj* s,const void* m){RememberChoose(s);CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=TARGET_START_HANDLER mediator=%p signal=%p",++seq,(void*)s,(void*)(s&&medTargetStartSignal.IsValid()?medTargetStartSignal[s].Get():nullptr));Snapshot("BUTTON_CHOOSE_ENTER",s);if(oldChoose)oldChoose(s,m);Snapshot("BUTTON_CHOOSE_EXIT",s);}
inline void Resume(Obj* s,const void* m){
    lastResumeMethod=m;
    CC_HOOK_TRACE("CustomComponents/RESUME_TRACE: seq=%u step=ENTER effect=%d group=%d holderState=%d awaitingFinder=%d subturn=%d",++seq,lastId,lastGroup,currentState,handoffAwaitingDiscovery?1:0,Phase(s));
    CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=PHASE_RESUME_HANDLER mediator=%p signal=%p",++seq,(void*)s,(void*)(s&&medResumeSignal.IsValid()?medResumeSignal[s].Get():nullptr));
    Snapshot("BUTTON_RESUME_ENTER",s);
    const bool stockEligible=s && oldCare && oldCare(s,m);
    if(stockEligible && s && Core::SceneViewContext::getGCHandle &&
       Core::SceneViewContext::getGCHandle(recoveryMediatorRoot)==s) {
        recoveredSerial=chooseSerial;
        readyRecoveryFrames=0;
    }
    if(oldResume)oldResume(s,m);
    Snapshot("BUTTON_RESUME_EXIT",s);
    CC_HOOK_TRACE("CustomComponents/RESUME_TRACE: seq=%u step=EXIT effect=%d group=%d holderState=%d awaitingFinder=%d subturn=%d",++seq,lastId,lastGroup,currentState,handoffAwaitingDiscovery?1:0,Phase(s));
}
inline void Surprise(Obj* s,const void* m){Snapshot("BUTTON_SURPRISE_ENTER",s);if(oldSurprise)oldSurprise(s,m);Snapshot("BUTTON_SURPRISE_EXIT",s);}
inline void Input(Obj* s,int v,const void* m){
    if (chooseSerial!=recoveredSerial && chooseSerial!=0) {
        if (v==6 || v==7) {
            sawTargetingInput=true;
            returnedFromTargeting=false;
        } else if ((v==3 || v==4) && (sawTargetingInput || currentState==6 || currentState==7)) {
            returnedFromTargeting=true;
            PF3_REPAIR_TRACE("CustomComponents/BUTTON179: TARGET_INPUT_EXIT serial=%u previous=%d next=%d targetingObserved=%d",
                chooseSerial,currentState,v,int(sawTargetingInput));
        }
    }
    currentState=v;CC_HOOK_TRACE("CustomComponents/STATE_TRACE: seq=%u tag=BUTTON_INPUT_ENTER state=%d",++seq,v);Snapshot("BUTTON_INPUT_SNAPSHOT",s);if(oldInput)oldInput(s,v,m);Snapshot("BUTTON_INPUT_EXIT",s);}
inline void Bind(Obj* s,int v,const void* m){int previous=s&&holderState.IsValid()?holderState[s].Get():-99;PrePlayLifecycleTrace::OnBind(s,previous,v,false);CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=BIND_ENTER holder=%p newState=%d oldState=%d front=%d/%d",++seq,(void*)s,v,s&&holderState.IsValid()?holderState[s].Get():-99,lastId,lastGroup);if(oldBind)oldBind(s,v,m);
 PrePlayLifecycleTrace::OnBind(s,previous,v,true);
 if ((v==6||v==7) && previous==v && lastId>=0 &&
     (lastId!=handoffId || lastGroup!=handoffGroup)) {
   handoffId=lastId;handoffGroup=lastGroup;handoffSeq=++seq;
   handoffAwaitingDiscovery=true;
   CC_HOOK_TRACE("CustomComponents/HANDOFF_TRACE: seq=%u stage=SAME_STATE_BOUND effect=%d group=%d holder=%p state=%d action=observe_only",handoffSeq,handoffId,handoffGroup,(void*)s,v);
 }
 CC_HOOK_TRACE("CustomComponents/LIFECYCLE: seq=%u step=BIND_EXIT holder=%p newState=%d",++seq,(void*)s,v);}
inline Obj* First(Obj* s,const void* m){Obj* e=oldFirst?oldFirst(s,m):nullptr;int id=e&&effectEntity.IsValid()?effectEntity[e].Get():-1;int group=e&&effectGroup.IsValid()?effectGroup[e].Get():-1;if(id!=lastId||group!=lastGroup||e!=lastFirst){lastFirst=e;lastId=id;lastGroup=group;CC_HOOK_TRACE("CustomComponents/STATE_TRACE: seq=%u tag=HOLDER_FIRST_CHANGED effect=%p id=%d group=%d holderState=%d",++seq,(void*)e,id,group,s&&holderState.IsValid()?holderState[s].Get():-99);}return e;}
inline bool Care(Obj* s,const void* m){lastCareMethod=m;bool v=oldCare?oldCare(s,m):false;CC_HOOK_TRACE("CustomComponents/RESUME_TRACE: seq=%u step=CARE decision=%d effect=%d group=%d holderState=%d subturn=%d",++seq,v?1:0,lastId,lastGroup,currentState,Phase(s));CC_HOOK_TRACE("CustomComponents/STATE_TRACE: seq=%u tag=BUTTON_PHASE_CARE result=%d subturn=%d",++seq,v?1:0,Phase(s));return v;}
inline void Done(Obj* s,const void* m){CC_HOOK_TRACE("CustomComponents/STATE_TRACE: seq=%u tag=BUTTON_VIEW_DONE view=%p",++seq,(void*)s);if(oldDone)oldDone(s,m);}
inline void Optional(Obj* s,const void* m){CC_HOOK_TRACE("CustomComponents/STATE_TRACE: seq=%u tag=BUTTON_VIEW_OPTIONAL view=%p",++seq,(void*)s);if(oldOptional)oldOptional(s,m);}
inline void Anim(Obj* s,int a,int b,int c,const void* m){CC_HOOK_TRACE("CustomComponents/STATE_TRACE: seq=%u tag=BUTTON_ENABLE_ANIM sprite=%d playable=%d noPlayable=%d subturn=%d",++seq,a,b,c,Phase(s));if(oldAnim)oldAnim(s,a,b,c,m);}
inline void Activate(Obj* view,bool active,const void* m){
 CC_HOOK_TRACE("CustomComponents/BUTTON_DIAG: ACTIVATE_ENTER view=%p active=%d lastInput=%d effect=%d/%d",(void*)view,active,currentState,lastId,lastGroup);
 if(oldActivate)oldActivate(view,active,m);
 CC_HOOK_TRACE("CustomComponents/BUTTON_DIAG: ACTIVATE_EXIT view=%p active=%d",(void*)view,active);
}
inline void TriggeredStart(Obj* system,Obj* effect,const void* m){
 CC_HOOK_TRACE("CustomComponents/TRIGGER_DIAG: STOCK_TRIGGER_START_ENTER system=%p effect=%p currentEffect=%d/%d",(void*)system,(void*)effect,lastId,lastGroup);
 if(oldTriggeredStart)oldTriggeredStart(system,effect,m);
 CC_HOOK_TRACE("CustomComponents/TRIGGER_DIAG: STOCK_TRIGGER_START_EXIT system=%p",(void*)system);
}
inline void Setup(){
auto holder=BNM::Class("PvZCards.Game","GameInputStateHolder");auto med=BNM::Class("PvZCards.Game","EndTurnButtonViewMediator");auto view=BNM::Class("PvZCards.Game","EndTurnButtonView");auto effect=BNM::Class("PvZCards.Engine","Effect");
holderState=holder.GetField("GameInputState").cast<int>();effectEntity=effect.GetField("EffectEntityId").cast<int>();effectGroup=effect.GetField("EffectGroupId").cast<int>();
medResumeSignal=med.GetField("<PhaseResumeSignal>k__BackingField").cast<Obj*>();medTargetStartSignal=med.GetField("<ChooseTargetPhaseStartSignal>k__BackingField").cast<Obj*>();
medPhase=med.GetField("<VisualTurnPhaseModel>k__BackingField").cast<Obj*>();medMachine=med.GetField("SafetyMachine").cast<Obj*>();medView=med.GetField("<ButtonView>k__BackingField").cast<Obj*>();phaseSubturn=BNM::Class("PvZCards.Game","VisualTurnPhaseModel").GetField("SubTurn").cast<int>();
// A generic state machine may not be exposed as a runtime-resolvable closed generic;
// missing field is represented by -99 instead of reading unverified offsets.
// Runtime discovery happens in Snapshot after obtaining the actual closed
// GenericStateMachine instance. Do not assume the open generic field layout.
viewAnimator=view.GetField("ButtonAnimator").cast<Obj*>();
auto hook=[](auto method,auto detour,auto& old){if(method.IsValid())BNM::BasicHook(method,detour,old);};
hook(med.GetMethod("ChooseTargetStart",0),Choose,oldChoose);hook(med.GetMethod("ReturnToRegularPhase",0),Resume,oldResume);hook(med.GetMethod("ZombieSurprise",0),Surprise,oldSurprise);hook(med.GetMethod("InputStateChanged",1),Input,oldInput);hook(med.GetMethod("DoesButtonVisuallyCareAboutCurrentPhase",0),Care,oldCare);hook(med.GetMethod("AddButtonEnableEffectAnimation",3),Anim,oldAnim);
hook(holder.GetMethod("GetFirstEffectInQueue",0),First,oldFirst);
hook(holder.GetMethod("DetermineCurrentInputState",0),Determine,oldDetermine);
hook(holder.GetMethod("GetInputStateFromEffectType",1),EffectState,oldStateFromEffect);
hook(holder.GetMethod("UnbindOldCommands",0),Unbind,oldUnbind);
hook(holder.GetMethod("BindCommandsForState",1),Bind,oldBind);
hook(view.GetMethod("ActivateButton",1),Activate,oldActivate);
auto triggered=BNM::Class("PvZCards.Core.Systems","TriggeredAbilityStartSystem");
hook(triggered.GetMethod("ProcessEffect",1),TriggeredStart,oldTriggeredStart);
hook(view.GetMethod("SetDone",0),Done,oldDone);hook(view.GetMethod("SetOptionalEarlyEnd",0),Optional,oldOptional);
LOG_INFO("CustomComponents/UNIFIED_DIAG: machineField=%d animatorField=%d buttonActivate=%d stockTriggerStart=%d; no gameplay overrides",int(machineCurrentState.IsValid()),int(viewAnimator.IsValid()),int(oldActivate!=nullptr),int(oldTriggeredStart!=nullptr));
LOG_INFO("CustomComponents/LIFECYCLE: SETUP determine=%d effectState=%d unbind=%d resumeSignalField=%d targetStartSignalField=%d readOnly=1",!!oldDetermine,!!oldStateFromEffect,!!oldUnbind,int(medResumeSignal.IsValid()),int(medTargetStartSignal.IsValid()));
LOG_INFO("CustomComponents/STATE_TRACE: SETUP choose=%d resume=%d surprise=%d input=%d care=%d anim=%d first=%d bind=%d done=%d optional=%d",!!oldChoose,!!oldResume,!!oldSurprise,!!oldInput,!!oldCare,!!oldAnim,!!oldFirst,!!oldBind,!!oldDone,!!oldOptional);
}
}
