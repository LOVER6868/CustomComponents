#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Current scene's EntityViewMapper: a GC-rooted frontend reference shared by
// hand discards, lane returns, stat reconciliation, and lifecycle observations.
// Only numeric model identity is retained; never dereference it after a scene
// change. Capture and Release run at the existing frontend hook boundaries.
// This module owns no native hooks and never creates, removes, or changes views.

#include <BNM/Class.hpp>
#include <cstdint>

#include "diagnostics/DiagnosticLogging.hpp"
#include "log.h"

namespace CustomComponents::Core::SceneViewContext {
    using Obj = BNM::IL2CPP::Il2CppObject;
    using NewGCHandleFn = uint32_t (*)(Obj*, bool);
    using GetGCHandleFn = Obj* (*)(uint32_t);
    using FreeGCHandleFn = void (*)(uint32_t);

    // Provided by the existing CardView lifecycle initialization. These are
    // the same IL2CPP exports also used to root deferred CreateCardAsync work.
    inline NewGCHandleFn newGCHandle = nullptr;
    inline GetGCHandleFn getGCHandle = nullptr;
    inline FreeGCHandleFn freeGCHandle = nullptr;
    inline uint32_t mapperHandle = 0;
    inline std::uintptr_t modelIdentity = 0;

    inline void ConfigureGC(NewGCHandleFn create, GetGCHandleFn get,
                            FreeGCHandleFn release) {
        newGCHandle = create;
        getGCHandle = get;
        freeGCHandle = release;
    }

    inline Obj* RootedMapper() {
        return mapperHandle && getGCHandle ? getGCHandle(mapperHandle) : nullptr;
    }

    inline std::uintptr_t ModelIdentity() { return modelIdentity; }

    inline Obj* MapperForModel(std::uintptr_t identity) {
        Obj* mapper = RootedMapper();
        return mapper && identity && modelIdentity == identity ? mapper : nullptr;
    }

    inline void Release() {
        if (mapperHandle && freeGCHandle)
            freeGCHandle(mapperHandle);
        mapperHandle = 0;
        modelIdentity = 0;
    }

    inline void Capture(Obj* mapper, Obj* model) {
        if (!mapper || !model || !newGCHandle || !getGCHandle || !freeGCHandle) return;
        if (mapperHandle && RootedMapper() == mapper &&
            modelIdentity == reinterpret_cast<std::uintptr_t>(model)) return;
        Release();
        mapperHandle = newGCHandle(mapper, false);
        if (mapperHandle)
            modelIdentity = reinterpret_cast<std::uintptr_t>(model);
        if constexpr (CustomComponents::Diagnostics::kVerbose)
            CC_DIAG("CustomComponents/LANE-I6: CAPTURE frontendMapper=%p model=%p rooted=%d",
                     mapper, model, mapperHandle ? 1 : 0);
    }
} // namespace CustomComponents::Core::SceneViewContext
