#pragma once
// PvP phase 1: passive observation of stock remote SelectTarget actions.
// No target editing, extra service calls, prediction changes or PvP enablement.
#include <algorithm>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include "log.h"

namespace CustomComponents::Pick3::PvpReadOnlyTracer {
using Obj = BNM::IL2CPP::Il2CppObject;
using IntList = BNM::Structures::Mono::List<int>;
using ProcessFn = void(*)(Obj*, Obj*, const void*);
inline ProcessFn originalProcess = nullptr;
inline BNM::Field<int> fieldType, fieldCard, fieldPlayer;
inline BNM::Field<IntList*> fieldTargets;
using ExecuteFn = void(*)(Obj*, const void*);
inline ExecuteFn originalSelectExecute = nullptr;
inline bool active = false;
inline unsigned int totalRemoteCalls = 0;
inline unsigned int selectCommandCalls = 0;
static void SelectExecute(Obj* self, const void* method) {
    ++selectCommandCalls;
    if (selectCommandCalls <= 24) {
        LOG_INFO("CustomComponents/Pick3/PVP_TRACE: COMMAND_SELECT seq=%u", selectCommandCalls);
    }
    if (originalSelectExecute) originalSelectExecute(self, method);
}

inline unsigned int observedSelections = 0;

static void Process(Obj* self, Obj* play, const void* method) {
    if (active) {
        ++totalRemoteCalls;
        // First 32 calls of any type to distinguish no invocation from unexpected play types.
        if (!play) {
            if (totalRemoteCalls <= 32) LOG_INFO("CustomComponents/Pick3/PVP_TRACE: REMOTE_CALL seq=%u play=null", totalRemoteCalls);
        } else {
            const int type = fieldType[play].Get();
            if (type == 6 || totalRemoteCalls <= 32) {
                if (type == 6) ++observedSelections;
                auto* targets = fieldTargets[play].Get();
                const int count = targets ? targets->GetSize() : -1;
                int ids[4] = {-1,-1,-1,-1};
                if (targets && count >= 0 && count <= 4 && count <= targets->GetCapacity()) {
                    for (int i = 0; i < count; ++i) ids[i] = targets->GetData()[i];
                }
                LOG_INFO("CustomComponents/Pick3/PVP_TRACE: REMOTE_CALL seq=%u select=%d type=%d card=%d player=%d count=%d ids=%d,%d,%d,%d",
                         totalRemoteCalls, type == 6 ? 1 : 0, type, fieldCard[play].Get(),
                         fieldPlayer[play].Get(), count, ids[0], ids[1], ids[2], ids[3]);
            }
        }
    }
    if (originalProcess) originalProcess(self, play, method);
}
inline bool Initialize(bool request) {
    if (!request) return true;
    auto remote = BNM::Class("PvZCards.Multiplayer.Networking", "RemotePlayService");
    auto play = BNM::Class("PvZCards.Engine.Dtos", "Play");
    auto process = remote.GetMethod("Process", 1);
    fieldType = play.GetField("Type");
    fieldCard = play.GetField("CardId");
    fieldPlayer = play.GetField("PlayerId");
    fieldTargets = play.GetField("Targets");
    if (!remote.IsValid() || !play.IsValid() || !process.IsValid() ||
        !fieldType.IsValid() ||
        !fieldCard.IsValid() || !fieldPlayer.IsValid() || !fieldTargets.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/PVP_TRACE: NOT_READY metadata unavailable");
        return false;
    }
    BNM::BasicHook(process, Process, originalProcess);
    // Independent UI-command observation. No mutation or manual command invocation.
    auto selectCommand = BNM::Class("PvZCards.Game", "SelectTargetCommand");
    auto execute = selectCommand.GetMethod("Execute", 0);
    if (selectCommand.IsValid() && execute.IsValid()) {
        BNM::BasicHook(execute, SelectExecute, originalSelectExecute);
    }
    // Avoid hooking adjacent tiny success/failure methods (8-byte spacing in dump.cs).
    // Process returning is NOT proof of remote acceptance.
    active = originalProcess != nullptr;
    LOG_INFO("CustomComponents/Pick3/PVP_TRACE: READY enabled=%d process=%d readOnly=1 commandHook=%d completionHooks=0",
             active ? 1 : 0, originalProcess ? 1 : 0, originalSelectExecute ? 1 : 0);
    return active;
}
}
