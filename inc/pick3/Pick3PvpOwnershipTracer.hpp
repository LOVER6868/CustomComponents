#pragma once
// Phase 3: read-only target model and subturn teardown observation.
// Avoids tiny adjacent StartSubTurn/GetCurrentSubTurn IL2CPP method detours.
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include "log.h"

namespace CustomComponents::Pick3::PvpOwnershipTracer {
using Obj = BNM::IL2CPP::Il2CppObject;
using CmdFn = void(*)(Obj*, const void*);
using EndFn = void(*)(Obj*, const void*);
inline CmdFn originalUpdate = nullptr;
inline EndFn originalEnd = nullptr;
inline BNM::Field<Obj*> selectionModelField;
inline BNM::Field<int> selectionCardField;
inline BNM::Field<int> subturnField;
inline BNM::Field<int> turnField;
inline BNM::Field<Obj*> gameModelField;
inline BNM::Field<int> localPlayerField;
inline BNM::Field<int> opponentPlayerField;
inline unsigned int updateCalls = 0;
inline unsigned int endCalls = 0;

static void Update(Obj* self, const void* method) {
    const unsigned int seq = ++updateCalls;
    Obj* selection = (self && selectionModelField.IsValid()) ? selectionModelField[self].Get() : nullptr;
    int before = (selection && selectionCardField.IsValid()) ? selectionCardField[selection].Get() : -1;
    if (originalUpdate) originalUpdate(self, method);
    selection = (self && selectionModelField.IsValid()) ? selectionModelField[self].Get() : nullptr;
    const int after = (selection && selectionCardField.IsValid()) ? selectionCardField[selection].Get() : -1;
    if (seq <= 96 || before != after) {
        LOG_INFO("CustomComponents/Pick3/PVP_OWNER: TARGET_UPDATE seq=%u cardBefore=%d cardAfter=%d model=%d", seq, before, after, selection ? 1 : 0);
    }
}

static void End(Obj* self, const void* method) {
    const unsigned int seq = ++endCalls;
    const int before = (self && subturnField.IsValid()) ? subturnField[self].Get() : -1;
    const int turnBefore = (self && turnField.IsValid()) ? turnField[self].Get() : -1;
    Obj* model = (self && gameModelField.IsValid()) ? gameModelField[self].Get() : nullptr;
    const int local = (model && localPlayerField.IsValid()) ? localPlayerField[model].Get() : -1;
    const int opponent = (model && opponentPlayerField.IsValid()) ? opponentPlayerField[model].Get() : -1;
    if (originalEnd) originalEnd(self, method);
    const int after = (self && subturnField.IsValid()) ? subturnField[self].Get() : -1;
    if (seq <= 96) {
        LOG_INFO("CustomComponents/Pick3/PVP_OWNER: SUBTURN_END seq=%u before=%d after=%d turn=%d local=%d opponent=%d",
                 seq, before, after, turnBefore, local, opponent);
    }
}

inline bool Initialize(bool enabled) {
    if (!enabled) return true;
    auto updateClass = BNM::Class("PvZCards.Game", "UpdateTargetSelectionModelCommand");
    auto selectionClass = BNM::Class("PvZCards.Game", "TargetSelectionModel");
    auto phaseClass = BNM::Class("PvZCards.Game", "VisualTurnPhaseModel");
    auto modelClass = BNM::Class("PvZCards.Core.Utilities", "GameModel");
    if (!updateClass.IsValid() || !selectionClass.IsValid() || !phaseClass.IsValid() || !modelClass.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/PVP_OWNER: NOT_READY missing classes");
        return false;
    }
    selectionModelField = updateClass.GetField("<TargetSelectionModel>k__BackingField");
    selectionCardField = selectionClass.GetField("CardId");
    subturnField = phaseClass.GetField("SubTurn");
    turnField = phaseClass.GetField("TurnPhase");
    gameModelField = phaseClass.GetField("<GameModel>k__BackingField");
    localPlayerField = modelClass.GetField("LocalPlayerId");
    opponentPlayerField = modelClass.GetField("OpponentPlayerId");
    auto updateMethod = updateClass.GetMethod("Execute", 0);
    auto endMethod = phaseClass.GetMethod("EndSubTurn", 0);
    if (!selectionModelField.IsValid() || !selectionCardField.IsValid() || !subturnField.IsValid() ||
        !turnField.IsValid() || !gameModelField.IsValid() || !localPlayerField.IsValid() ||
        !opponentPlayerField.IsValid() || !updateMethod.IsValid() || !endMethod.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/PVP_OWNER: NOT_READY methods or fields missing");
        return false;
    }
    BNM::BasicHook(updateMethod, Update, originalUpdate);
    BNM::BasicHook(endMethod, End, originalEnd);
    LOG_INFO("CustomComponents/Pick3/PVP_OWNER: READY update=%d end=%d readOnly=1 startHook=0",
             originalUpdate ? 1 : 0, originalEnd ? 1 : 0);
    return originalUpdate && originalEnd;
}
} // namespace CustomComponents::Pick3::PvpOwnershipTracer
