#pragma once
// Completion observation for any BaseService selection. No native detours.
// Managed delegates are combined with existing handlers and removed individually.
#include "gameplay/SelectionCompletionState.hpp"
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Delegates.hpp>
#include "gameplay/SelectionTransport.hpp"

namespace CustomComponents::SelectionCompletion {
using Obj = BNM::IL2CPP::Il2CppObject;
inline CompletionState completion;
inline std::uint64_t activeTicket = 0;
// Optional read-only diagnostics observer; does not alter completion status.
inline void (*networkObserver)(bool, int) = nullptr;
struct Receiver : Obj {
    std::uint64_t ticket = 0;
    BNM_CustomClass(Receiver,
        BNM::CompileTimeClassBuilder("Sprig.Selection", "CompletionReceiver", "Assembly-CSharp.dll").Build(),
        BNM::CompileTimeClassBuilder("System", "Object", "mscorlib.dll").Build(), {});
    // Publish the callback token in BNM metadata to match this native receiver.
    // BNM already reserves sizeof(Receiver) for new class instances; this is
    // metadata/ABI consistency, not a proven out-of-bounds crash fix.
    BNM_CustomField(ticket, BNM::Defaults::Get<std::uint64_t>(), "ticket");
    void Finish(Result value, int reason) {
        const auto before = completion.Status();
        const bool accepted = completion.Complete(ticket, value, reason);
        LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=SERVICE_CALLBACK ticket=%llu activeTicket=%llu callbackResult=%d reason=%d statusBefore=%d accepted=%d statusAfter=%d",
            (unsigned long long)ticket, (unsigned long long)activeTicket,
            int(value), reason, int(before), int(accepted), int(completion.Status()));
    }
    void OnSuccess() {
        // Old managed service delegates may still fire; never publish telemetry
        // or alter completion for a retired selection ticket.
        if (!ticket || ticket != activeTicket) return;
        if (networkObserver) networkObserver(true,0);
        Finish(Result::Success, 0);
    }
    void OnFailure(int reason) {
        if (!ticket || ticket != activeTicket) return;
        if (networkObserver) networkObserver(false,reason);
        Finish(Result::Failure, reason);
    }
    BNM_CustomMethod(OnSuccess, false, BNM::Defaults::Get<void>(), "OnSuccess");
    BNM_CustomMethod(OnFailure, false, BNM::Defaults::Get<void>(), "OnFailure",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "ServiceFailure", "Assembly-CSharp.dll").Build());
};
inline BNM::Field<Obj*> successField, failureField;
inline BNM::Method<Obj*> combine, remove;
inline BNM::Class receiverClass, actionClass, failureActionClass;
inline std::vector<std::uint32_t> roots;
inline Obj *service = nullptr, *successDelegate = nullptr, *failureDelegate = nullptr;
inline bool hadSuccessHandler = false;
inline Obj* Root(Obj* obj) {
    if (!obj) return nullptr;
    auto handle = Core::SceneViewContext::newGCHandle(obj, false);
    if (!handle) return nullptr;
    roots.push_back(handle); return obj;
}
inline Result Status() { return completion.Status(); }
inline void Detach() {
    // Invalidate callbacks before releasing their receivers, including a delegate
    // already captured by an asynchronous service. Its managed owner keeps it alive.
    completion.Invalidate();
    activeTicket = 0;
    // PATCH 1.6.5: Do not synchronously call Delegate.Remove here.
    // On the matched phone crash, the native PC at libsprig+0x138A04 is
    // BNM::TryInvoke within the managed Delegate.Remove/setter path, reached
    // immediately after RETIRE_BOUNDARY: BEFORE and before AFTER_DETACH.
    // BNM::TryInvoke cannot intercept a native SIGSEGV (null + 0x38).
    // The service's managed delegate chain retains our receiver/delegates
    // until that service is collected. Retired callbacks are inert because
    // activeTicket was invalidated above. This has a bounded-to-service-lifetime
    // accumulation tradeoff; proper deferred unsubscription needs proof that
    // Delegate.Remove works on the correct service/managed thread.
    LOG_INFO("CustomComponents/Pick3/PATCH165_DETACH: skip_native_delegate_remove service=%p success=%p failure=%p retiredTicket=1",
             (void*)service, (void*)successDelegate, (void*)failureDelegate);
    service = successDelegate = failureDelegate = nullptr;
    for (auto handle : roots) Core::SceneViewContext::freeGCHandle(handle);
    roots.clear();
}
inline bool Initialize() {
    auto base = BNM::Class("PvZCards.Engine", "BaseService");
    successField = base.GetField("<OnSuccess>k__BackingField");
    failureField = base.GetField("<OnFailure>k__BackingField");
    auto delegate = BNM::Class("System", "Delegate");
    combine = delegate.GetMethod("Combine", 2).cast<Obj*>();
    remove = delegate.GetMethod("Remove", 2).cast<Obj*>();
    receiverClass = BNM::Class("Sprig.Selection", "CompletionReceiver");
    actionClass = BNM::Class("System", "Action");
    auto reason = BNM::Class("PvZCards.Engine", "ServiceFailure");
    failureActionClass = BNM::Class("System", "Action`1").GetGeneric({reason.GetCompileTimeClass()});
    const bool ok = successField.IsValid() && failureField.IsValid() && combine.IsValid() &&
        remove.IsValid() && receiverClass.IsValid() && actionClass.IsValid() && failureActionClass.IsValid();
    LOG_INFO("CustomComponents/SelectionCompletion: SETUP ready=%d success=%d failure=%d combine=%d remove=%d receiver=%d action=%d failureAction=%d",
        ok ? 1 : 0, successField.IsValid() ? 1 : 0, failureField.IsValid() ? 1 : 0,
        combine.IsValid() ? 1 : 0, remove.IsValid() ? 1 : 0, receiverClass.IsValid() ? 1 : 0,
        actionClass.IsValid() ? 1 : 0, failureActionClass.IsValid() ? 1 : 0);
    return ok;
}
inline bool Begin(Obj* target) {
    if (Status() != Result::Idle || !target) return false;
    service = Root(target);
    auto* receiver = reinterpret_cast<Receiver*>(Root(receiverClass.CreateNewInstance()));
    if (!service || !receiver) { Detach(); return false; }
    activeTicket = completion.Begin();
    if (!activeTicket) { Detach(); return false; }
    receiver->ticket = activeTicket;
    auto make = [&](BNM::Class& type, const char* method, int argc) -> Obj* {
        auto callback = receiverClass.GetMethod(method, argc);
        if (!callback.IsValid()) return nullptr;
        callback.SetInstance(receiver);
        auto* shell = reinterpret_cast<BNM::DelegateBase*>(Root(type.CreateNewInstance()));
        return shell ? Root(reinterpret_cast<Obj*>(shell->Create(callback))) : nullptr;
    };
    successDelegate = make(actionClass, "OnSuccess", 0);
    failureDelegate = make(failureActionClass, "OnFailure", 1);
    if (!successDelegate || !failureDelegate) { Detach(); return false; }
    hadSuccessHandler = successField[target].Get() != nullptr;
    LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=DELEGATES_BEFORE service=%p existingSuccess=%d existingFailure=%d ticket=%llu",
        (void*)target, int(hadSuccessHandler), int(failureField[target].Get()!=nullptr),
        (unsigned long long)activeTicket);

    bool attached = false;
    const auto ex = BNM::TryInvoke([&] {
        Obj* success = Root(combine(successField[target].Get(), successDelegate));
        Obj* failed = Root(combine(failureField[target].Get(), failureDelegate));
        if (!success || !failed) return;
        successField[target].Set(success); failureField[target].Set(failed); attached = true;
    });
    if (ex.IsValid() || !attached) {
        LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=DELEGATES_FAILED service=%p exception=%d attached=%d",
            (void*)target, int(ex.IsValid()), int(attached));
        Detach(); return false;
    }
    LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=DELEGATES_ATTACHED service=%p success=%d failure=%d ticket=%llu",
        (void*)target, int(successField[target].Get()!=nullptr),
        int(failureField[target].Get()!=nullptr), (unsigned long long)activeTicket);
    return true;
}
inline void MarkUncertain() {
    completion.Complete(activeTicket, Result::Uncertain);
}
} // namespace CustomComponents::SelectionCompletion
