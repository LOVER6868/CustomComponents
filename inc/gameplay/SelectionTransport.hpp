#pragma once
// Generic stock SelectTarget Play construction for future custom selectors.
// No hooks, no GUI dependencies, no implicit engine mutation.
// Caller owns validation, ownership, deduplication and completion semantics.
#include <cstdint>
#include <vector>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Exceptions.hpp>
#include "core/SceneViewContext.hpp"
#include "log.h"

namespace CustomComponents::SelectionTransport {
using Obj = BNM::IL2CPP::Il2CppObject;
using IntList = BNM::Structures::Mono::List<int>;
inline constexpr int kSelectTargetPlayType = 6;

struct TargetSelection {
    int playerId = -1;
    std::vector<int> entityIds;
    int playType = kSelectTargetPlayType;
    int cardId = -1;
    int orderInTargetLocation = 0;
};

struct PlayFields {
    BNM::Class* dto = nullptr;
    BNM::Field<IntList*>* targets = nullptr;
    BNM::Field<int>* type = nullptr;
    BNM::Field<int>* card = nullptr;
    BNM::Field<int>* player = nullptr;
    BNM::Field<int>* order = nullptr;
    bool Valid() const {
        return dto && dto->IsValid() && targets && targets->IsValid() &&
            type && type->IsValid() && card && card->IsValid() &&
            player && player->IsValid() && order && order->IsValid();
    }
};

struct Prepared {
    Obj* object = nullptr;
    std::uint32_t root = 0;
};

// Failure here is guaranteed pre-dispatch: caller can still retry.
inline bool Prepare(const PlayFields& api, const TargetSelection& action, Prepared& out) {
    out = {};
    if (!api.Valid() || action.playerId < 0 || action.entityIds.empty() ||
        action.entityIds.size() > 3 || action.playType != kSelectTargetPlayType ||
        !Core::SceneViewContext::newGCHandle || !Core::SceneViewContext::freeGCHandle)
        return false;
    for (int id : action.entityIds) if (id < 0) return false; // Player entity 0 is legal.
    Obj* play = api.dto->CreateNewInstance();
    if (!play) return false;
    std::uint32_t root = Core::SceneViewContext::newGCHandle(play, false);
    if (!root) return false;
    const auto fail = [&]() {
        Core::SceneViewContext::freeGCHandle(root);
        return false;
    };
    auto ctor = api.dto->GetMethod(".ctor", 0).cast<void>();
    if (!ctor.IsValid()) return fail();
    const auto ctorEx = BNM::TryInvoke([&] { ctor[play](); });
    if (ctorEx.IsValid()) {
        LOG_ERR("CustomComponents/SelectionTransport: CTOR_EXCEPTION class=%s message=%s",
                ctorEx.ClassName().c_str(), ctorEx.Message().c_str());
        return fail();
    }
    IntList* ids = (*api.targets)[play].Get();
    if (!ids) return fail();
    auto add = BNM::Class(static_cast<Obj*>(ids)).GetMethod("Add", 1).cast<void>();
    if (!add.IsValid()) return fail();
    for (int id : action.entityIds) {
        auto err = BNM::TryInvoke([&] { add[ids](id); });
        if (err.IsValid()) {
            LOG_ERR("CustomComponents/SelectionTransport: TARGET_ADD_EXCEPTION class=%s message=%s",
                    err.ClassName().c_str(), err.Message().c_str());
            return fail();
        }
    }
    (*api.type)[play].Set(action.playType);
    (*api.card)[play].Set(action.cardId);
    (*api.player)[play].Set(action.playerId);
    (*api.order)[play].Set(action.orderInTargetLocation);
    out = {play, root};
    return true;
}
// A returned Process() call means only that the local service returned;
// it does NOT prove acceptance by a remote server or the other client.
enum class DispatchResult { MissingMethod, ManagedException, Returned };
inline bool HasProcessMethod(Obj* service) {
    return service && BNM::Class(service).GetMethod("Process", 1).IsValid();
}
inline DispatchResult Dispatch(Obj* service, Obj* play) {
    if (!service || !play) return DispatchResult::MissingMethod;
    auto process = BNM::Class(service).GetMethod("Process", 1).cast<void>();
    if (!process.IsValid()) return DispatchResult::MissingMethod;
    const auto ex = BNM::TryInvoke([&] { process[service](play); });
    if (ex.IsValid()) {
        LOG_ERR("CustomComponents/SelectionTransport: PROCESS_EXCEPTION class=%s message=%s",
                ex.ClassName().c_str(), ex.Message().c_str());
        return DispatchResult::ManagedException;
    }
    return DispatchResult::Returned;
}
} // namespace CustomComponents::SelectionTransport
