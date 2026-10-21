#pragma once
// Per-request managed Action receiver. Only a (slot, ticket) completion signal
// crosses the managed callback; never touch scene objects from a loader callback.
#include <atomic>
#include <cstdint>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>

namespace CustomComponents::Presentation::Pick3NativeCacheReceiver {
    using Obj = BNM::IL2CPP::Il2CppObject;
    inline constexpr int kSlots = 12;
    inline std::atomic<std::uint32_t> ticketBySlot[kSlots]{};
    inline std::atomic<bool> finishedBySlot[kSlots]{};

    inline void Arm(int index, std::uint32_t ticket) {
        finishedBySlot[index].store(false, std::memory_order_release);
        ticketBySlot[index].store(ticket, std::memory_order_release);
    }
    inline void Disarm(int index) {
        ticketBySlot[index].store(0, std::memory_order_release);
        finishedBySlot[index].store(false, std::memory_order_release);
    }
    inline bool Finished(int index, std::uint32_t ticket) {
        return ticketBySlot[index].load(std::memory_order_acquire) == ticket &&
            finishedBySlot[index].load(std::memory_order_acquire);
    }
    struct Receiver : Obj {
        std::int32_t slot = -1;
        std::uint32_t ticket = 0;
        BNM_CustomClass(
            Receiver,
            BNM::CompileTimeClassBuilder("Sprig.Pick3", "NativeSpriteCacheReceiver", "Assembly-CSharp.dll").Build(),
            BNM::CompileTimeClassBuilder("System", "Object", "mscorlib.dll").Build(),
            {}
        );
        BNM_CustomField(slot, BNM::Defaults::Get<std::int32_t>(), "slot");
        BNM_CustomField(ticket, BNM::Defaults::Get<std::uint32_t>(), "ticket");
        void OnLoaded() {
            // Reject callbacks for a slot reused by a later popup.
            if (slot >= 0 && slot < kSlots && ticket &&
                ticketBySlot[slot].load(std::memory_order_acquire) == ticket)
                finishedBySlot[slot].store(true, std::memory_order_release);
        }
        BNM_CustomMethod(OnLoaded, false, BNM::Defaults::Get<void>(), "OnLoaded");
    };
}
