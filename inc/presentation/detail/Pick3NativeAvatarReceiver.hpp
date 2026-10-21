#pragma once
// Managed Action<CardView> receiver for isolated first-option native-avatar test.
// The callback only GC-roots its result and sets an atomic completion flag.
// All Unity/Animator cleanup runs later on the existing presentation tick.
#include <atomic>
#include <cstdint>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "core/SceneViewContext.hpp"
#include "log.h"

namespace CustomComponents::Presentation::Pick3NativeAvatarReceiver {
    using Obj = BNM::IL2CPP::Il2CppObject;
    constexpr int kSlots = 8;
    inline std::atomic<std::uint32_t> tickets[kSlots]{};
    inline std::atomic<std::uint32_t> cardHandles[kSlots]{};
    inline std::atomic<bool> completed[kSlots]{};

    inline void Arm(int index, std::uint32_t ticket) {
        cardHandles[index].store(0, std::memory_order_release);
        completed[index].store(false, std::memory_order_release);
        tickets[index].store(ticket, std::memory_order_release);
    }
    inline void Disarm(int index) {
        tickets[index].store(0, std::memory_order_release);
        completed[index].store(false, std::memory_order_release);
        cardHandles[index].store(0, std::memory_order_release);
    }
    inline bool Done(int index, std::uint32_t ticket) {
        return completed[index].load(std::memory_order_acquire) &&
               tickets[index].load(std::memory_order_acquire)==ticket;
    }
    struct Receiver : Obj {
        std::int32_t slot = -1;
        std::uint32_t ticket = 0;
        BNM_CustomClass(Receiver,
            BNM::CompileTimeClassBuilder("Sprig.Pick3", "NativeAvatarLoadReceiver", "Assembly-CSharp.dll").Build(),
            BNM::CompileTimeClassBuilder("System", "Object", "mscorlib.dll").Build(), {});
        BNM_CustomField(slot, BNM::Defaults::Get<std::int32_t>(), "slot");
        BNM_CustomField(ticket, BNM::Defaults::Get<std::uint32_t>(), "ticket");
        void OnLoaded(Obj* card) {
            LOG_INFO("CustomComponents/Pick3/ADB_TEST: CALLBACK_ENTRY slot=%d ticket=%u card=%p",slot,ticket,card);
            if (slot < 0 || slot >= kSlots || !ticket ||
                tickets[slot].load(std::memory_order_acquire)!=ticket) return;
            // A canceled callback can arrive after the UI has disappeared;
            // retain the managed card until the main-thread retirement tick.
            std::uint32_t root = card && CustomComponents::Core::SceneViewContext::newGCHandle
                ? CustomComponents::Core::SceneViewContext::newGCHandle(card, false) : 0;
            cardHandles[slot].store(root, std::memory_order_release);
            completed[slot].store(true, std::memory_order_release);
            LOG_INFO("CustomComponents/Pick3/ADB_TEST: CALLBACK_EXIT slot=%d root=%u",slot,root);
        }
        BNM_CustomMethod(OnLoaded, false, BNM::Defaults::Get<void>(), "OnLoaded",
            BNM::CompileTimeClassBuilder("PvZCards.Game", "CardView", "Assembly-CSharp.dll").Build());
    };
}
