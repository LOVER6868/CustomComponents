#pragma once
// Single owner of PvZCards.Game.CardViewMapper.MapCardView.
//
// Fixed order (preserved from DiscardFromHandPresentationSystem):
//   1. Hand-discard policy optionally requests stock replacement.
//   2. Lifecycle coordinator observes mapping before stock registration.
//   3. The original CardViewMapper.MapCardView runs exactly once.
//   4. Lifecycle coordinator observes mapping and updates outstanding work.
//
// The owner has no knowledge of discard tickets, view IDs, or scene lifetime.
// All callbacks are provided by existing feature systems during their original
// initialization point. Do not install another hook for this stock method.

#include <BNM/MethodBase.hpp>
#include "log.h"

namespace CustomComponents::Core::CardViewMappingCoordinator {
    using Obj = BNM::IL2CPP::Il2CppObject;
    using MapFn = void (*)(void*, Obj*, Obj*, bool, const void*);
    using ReplacementPolicy = bool (*)(void*, Obj*, Obj*, bool);
    using MapObserver = void (*)(void*, Obj*, Obj*, bool);

    inline MapFn originalMapCardView = nullptr;
    inline ReplacementPolicy chooseReplacement = nullptr;
    inline MapObserver beforeMap = nullptr;
    inline MapObserver afterMap = nullptr;

    static void MapCardViewHook(void* self, Obj* newView, Obj* cardData,
                                bool replaceViewObject, const void* method) {
        if (!originalMapCardView) return;
        const bool useReplacement = chooseReplacement
            ? chooseReplacement(self, newView, cardData, replaceViewObject)
            : replaceViewObject;
        if (beforeMap) beforeMap(self, newView, cardData, useReplacement);
        originalMapCardView(self, newView, cardData, useReplacement, method);
        if (afterMap) afterMap(self, newView, cardData, useReplacement);
    }

    inline bool Initialize(const BNM::MethodBase& mapCardView,
                           ReplacementPolicy replacementPolicy,
                           MapObserver mapBefore,
                           MapObserver mapAfter) {
        if (!mapCardView.IsValid() || !replacementPolicy || !mapBefore || !mapAfter) {
            LOG_ERR("CustomComponents/CardViewMapping: required method or callbacks missing");
            return false;
        }
        if (originalMapCardView) {
            // A second installation would stack native hooks on the same RVA.
            return chooseReplacement == replacementPolicy &&
                   beforeMap == mapBefore && afterMap == mapAfter;
        }
        chooseReplacement = replacementPolicy;
        beforeMap = mapBefore;
        afterMap = mapAfter;
        BNM::BasicHook(mapCardView, MapCardViewHook, originalMapCardView);
        if (!originalMapCardView) {
            chooseReplacement = nullptr;
            beforeMap = nullptr;
            afterMap = nullptr;
            LOG_ERR("CustomComponents/CardViewMapping: MapCardView hook failed");
            return false;
        }
        return true;
    }
} // namespace CustomComponents::Core::CardViewMappingCoordinator
