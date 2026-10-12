#pragma once

#include <cstdint>
#include <cstring>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "log.h"

namespace CustomComponents::Systems::TargetingPresentationRefreshSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;

    inline constexpr int kSelectTriggeredEffectPrimaryTarget = 6;
    inline constexpr int kSelectTriggeredEffectSecondaryTarget = 7;

    inline constexpr std::uintptr_t kEffectSourceIdOffset = 0x24;
    inline constexpr std::uintptr_t kEffectTargetIdOffset = 0x28;
    inline constexpr std::uintptr_t kEffectEffectEntityIdOffset = 0x30;
    inline constexpr std::uintptr_t kEffectGroupIdOffset = 0x3C;
    inline constexpr std::uintptr_t kChooseTargetPhaseOffset = 0x48;

    struct TargetingIdentity {
        Obj* holder = nullptr;
        Obj* effect = nullptr;
        int state = -1;
        int phase = -1;
        int sourceId = -1;
        int targetId = -1;
        int effectEntityId = -1;
        int effectGroupId = -1;
        bool valid = false;
    };

    inline TargetingIdentity lastTargetingIdentity{};

    inline BNM::Field<int> gameInputState;
    inline BNM::Field<Obj*> gameInputStateChangedSignal;

    inline BNM::Method<Obj*> getFirstEffectInQueue;
    inline BNM::Method<void> unbindOldCommands;
    inline BNM::Method<void> bindCommandsForState;
    inline BNM::Method<void> dispatchInputStateChanged;

    using UpdateFn = void (*)(void*, const void*);
    inline UpdateFn originalUpdate = nullptr;

    inline const char* TypeName(Obj* object) {
        if (!object)
            return "null";

        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "?";
    }

    inline bool IsTriggeredTargetState(int state) {
        return state == kSelectTriggeredEffectPrimaryTarget ||
               state == kSelectTriggeredEffectSecondaryTarget;
    }

    inline int ReadInt(Obj* object, std::uintptr_t offset) {
        if (!object)
            return -1;

        return *reinterpret_cast<int*>(
            reinterpret_cast<std::uint8_t*>(object) + offset
        );
    }

    inline TargetingIdentity ReadIdentity(
        Obj* holder,
        int state,
        Obj* effect
    ) {
        TargetingIdentity identity{};
        identity.holder = holder;
        identity.effect = effect;
        identity.state = state;

        if (!holder || !effect || !IsTriggeredTargetState(state))
            return identity;

        if (std::strcmp(TypeName(effect), "ChooseTargetBlockingEffect") != 0)
            return identity;

        identity.phase = ReadInt(effect, kChooseTargetPhaseOffset);
        identity.sourceId = ReadInt(effect, kEffectSourceIdOffset);
        identity.targetId = ReadInt(effect, kEffectTargetIdOffset);
        identity.effectEntityId =
            ReadInt(effect, kEffectEffectEntityIdOffset);
        identity.effectGroupId = ReadInt(effect, kEffectGroupIdOffset);
        identity.valid = true;
        return identity;
    }

    inline bool SameIdentity(
        const TargetingIdentity& a,
        const TargetingIdentity& b
    ) {
        if (!a.valid || !b.valid)
            return false;

        return
            a.holder == b.holder &&
            a.effect == b.effect &&
            a.state == b.state &&
            a.phase == b.phase &&
            a.sourceId == b.sourceId &&
            a.targetId == b.targetId &&
            a.effectEntityId == b.effectEntityId &&
            a.effectGroupId == b.effectGroupId;
    }

    inline bool RefreshStockPresentation(
        Obj* holder,
        int state
    ) {
        if (!holder || !IsTriggeredTargetState(state))
            return false;

        Obj* signal = gameInputStateChangedSignal[holder].Get();
        if (!signal)
            return false;

        // GameInputStateHolder.Update() at RVA 0x1F5932C normally does:
        //   UnbindOldCommands();
        //   BindCommandsForState(newState);
        //   GameInputState = newState;
        //   GameInputStateChangedSignal.Dispatch(newState);
        //
        // Its same-state early return skips all of that. Replay the skipped
        // presentation work while leaving GameInputState at the real 6/7 value.
        unbindOldCommands[holder]();
        bindCommandsForState[holder](state);
        dispatchInputStateChanged[signal](state);
        return true;
    }

    static void UpdateHook(
        void* self,
        const void* method
    ) {
        Obj* holder = static_cast<Obj*>(self);
        if (!holder) {
            originalUpdate(self, method);
            return;
        }

        const int stateBefore = gameInputState[holder].Get();

        originalUpdate(self, method);

        const int stateAfter = gameInputState[holder].Get();
        Obj* currentEffect = getFirstEffectInQueue[holder]();
        TargetingIdentity current =
            ReadIdentity(holder, stateAfter, currentEffect);

        const bool sameTargetState =
            stateBefore == stateAfter &&
            IsTriggeredTargetState(stateAfter);

        const bool blockerChanged =
            sameTargetState &&
            current.valid &&
            lastTargetingIdentity.valid &&
            lastTargetingIdentity.holder == holder &&
            !SameIdentity(lastTargetingIdentity, current);

        if (blockerChanged) {
            if (RefreshStockPresentation(holder, stateAfter)) {
                LOG_INFO(
                    "CustomComponents/TargetingPresentationRefresh: refreshed state=%d oldEffect=%p newEffect=%p oldEffectEntityId=%d newEffectEntityId=%d oldSourceId=%d newSourceId=%d",
                    stateAfter,
                    lastTargetingIdentity.effect,
                    current.effect,
                    lastTargetingIdentity.effectEntityId,
                    current.effectEntityId,
                    lastTargetingIdentity.sourceId,
                    current.sourceId
                );
            } else {
                LOG_ERR(
                    "CustomComponents/TargetingPresentationRefresh: refresh failed state=%d effect=%p",
                    stateAfter,
                    current.effect
                );
            }
        }

        if (current.valid) {
            lastTargetingIdentity = current;
        } else if (!IsTriggeredTargetState(stateAfter)) {
            lastTargetingIdentity = {};
        }
    }

    inline bool Initialize() {
        auto holderClass = BNM::Class(
            "PvZCards.Game",
            "GameInputStateHolder"
        );
        auto signalClass = BNM::Class(
            "PvZCards.Game",
            "GameInputStateChangedSignal"
        );

        if (!holderClass.IsValid() || !signalClass.IsValid()) {
            LOG_ERR(
                "CustomComponents/TargetingPresentationRefresh: metadata class lookup failed"
            );
            return false;
        }

        gameInputState = holderClass.GetField("GameInputState");
        gameInputStateChangedSignal =
            holderClass.GetField("<GameInputStateChangedSignal>k__BackingField");

        getFirstEffectInQueue =
            holderClass.GetMethod("GetFirstEffectInQueue", 0).cast<Obj*>();
        unbindOldCommands =
            holderClass.GetMethod("UnbindOldCommands", 0).cast<void>();
        bindCommandsForState =
            holderClass.GetMethod("BindCommandsForState", 1).cast<void>();

        // GameInputStateChangedSignal derives from Signal<GameInputState>.
        // BNM GetMethod includes parent methods.
        dispatchInputStateChanged =
            signalClass.GetMethod("Dispatch", 1).cast<void>();

        auto update = holderClass.GetMethod("Update", 0);

        const bool valid =
            gameInputState.IsValid() &&
            gameInputStateChangedSignal.IsValid() &&
            getFirstEffectInQueue.IsValid() &&
            unbindOldCommands.IsValid() &&
            bindCommandsForState.IsValid() &&
            dispatchInputStateChanged.IsValid() &&
            update.IsValid();

        if (!valid) {
            LOG_ERR(
                "CustomComponents/TargetingPresentationRefresh: metadata method/field lookup failed"
            );
            return false;
        }

        BNM::BasicHook(
            update,
            UpdateHook,
            originalUpdate
        );

        if (!originalUpdate) {
            LOG_ERR(
                "CustomComponents/TargetingPresentationRefresh: GameInputStateHolder.Update hook installation failed"
            );
            return false;
        }

        LOG_INFO(
            "CustomComponents/TargetingPresentationRefresh: ready (same-state 6/7 blocker refresh)"
        );
        return true;
    }

} // namespace CustomComponents::Systems::TargetingPresentationRefreshSystem
