#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Production portrait-service capture: intercept CardAssetsTask.Start(),
// read its already injected dependencies, forward to the original ONCE and
// return the original ResourceLoadRequest untouched.
// The sprite-cache subscriber GC-roots the two concrete services;
// this hook itself neither constructs a cache nor requests extra assets.
// One optional native hook; does not change CardView/scene/gameplay ownership.

#include <atomic>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/MethodBase.hpp>
#include "log.h"

namespace CustomComponents::Presentation::Pick3CardAssetsCapture {
    using Obj = BNM::IL2CPP::Il2CppObject;
    // dump.cs: PvZCards.Game.CardAssetsTask.Start() : ResourceLoadRequest
    // ARM64 native signature: ResourceLoadRequest*(CardAssetsTask*, const MethodInfo*)
    using StartFn = Obj* (*)(Obj*, const void*);

    inline StartFn originalStart = nullptr;
    inline void (*onConcreteDependencies)(Obj* loader, Obj* provider) = nullptr;
    inline void (*onCardDataSnapshot)(int guid, Obj* data) = nullptr;
    inline BNM::Field<Obj*> cardDataField;
    inline BNM::Class taskClass;
    inline BNM::Class loaderInterface;
    inline BNM::Class providerInterface;
    inline BNM::Class idInterpreterClass;
    inline BNM::Class cardMapperClass;
    inline BNM::Field<Obj*> loaderField;
    inline BNM::Field<Obj*> providerField;
    inline BNM::Field<int> guidField;

    static Obj* StartCapture(Obj* self, const void* method) {
        // If the hook installed, the trampoline MUST be valid. Never substitute
        // a new/null resource load request for the game's actual return value.
        if (!originalStart) return nullptr;
        if (self && taskClass.IsValid() && BNM::IsA(self, taskClass)) {
            Obj* loader = loaderField.IsValid() ? loaderField[self].Get() : nullptr;
            Obj* provider = providerField.IsValid() ? providerField[self].Get() : nullptr;
            // Capture only the concrete injected services expected by the art cache.
            if (loader && provider && idInterpreterClass.IsValid() &&
                cardMapperClass.IsValid() && BNM::IsA(loader, idInterpreterClass) &&
                BNM::IsA(provider, cardMapperClass) && onConcreteDependencies)
                onConcreteDependencies(loader, provider);
        }

        Obj* request = originalStart(self, method);
        // Observe an object already initialized by the game's own Start().
        // Never call CardDataMapper or retain CardData pointers here.
        if (self && onCardDataSnapshot && guidField.IsValid() && cardDataField.IsValid()) {
            const int guid = guidField[self].Get();
            Obj* data = cardDataField[self].Get();
            if (guid > 0 && data) onCardDataSnapshot(guid, data);
        }
        return request;
    }

    inline bool Initialize() {
        if (originalStart) return true; // Never stack native hooks.
        taskClass = BNM::Class("PvZCards.Game", "CardAssetsTask");
        loaderInterface = BNM::Class("PvZCards", "ResourceLoader");
        providerInterface = BNM::Class("PvZCards.Core.Utilities", "CardDataProvider");
        idInterpreterClass = BNM::Class("PvZCards.Common", "IdInterpreterLoader");
        cardMapperClass = BNM::Class("PvZCards.Core.Utilities", "CardDataMapper");
        if (taskClass.IsValid()) {
            loaderField = taskClass.GetField("<ResourceLoader>k__BackingField");
            providerField = taskClass.GetField("<CardDataProvider>k__BackingField");
            guidField = taskClass.GetField("CardGuid");
            cardDataField = taskClass.GetField("CardData").cast<Obj*>();
        }
        const auto start = taskClass.IsValid() ? taskClass.GetMethod("Start", 0) : BNM::MethodBase{};
        const auto* info = start.IsValid() ? start.GetInfo() : nullptr;
        const bool signatureOk = info && info->parameters_count == 0 && info->methodPointer != nullptr;
        const bool fieldsOk = loaderField.IsValid() && providerField.IsValid() && guidField.IsValid() && cardDataField.IsValid();
        const bool interfacesOk = loaderInterface.IsValid() && providerInterface.IsValid();
        CC_DIAG("CustomComponents/Pick3/ASSETTASK: SETUP class=%d start=%d signature=%d fields=%d/%d/%d interfaces=%d/%d native=%p expectedRVA=0x1EEABC0",
                 taskClass.IsValid() ? 1 : 0, start.IsValid() ? 1 : 0,
                 signatureOk ? 1 : 0, loaderField.IsValid() ? 1 : 0,
                 providerField.IsValid() ? 1 : 0, guidField.IsValid() ? 1 : 0,
                 loaderInterface.IsValid() ? 1 : 0, providerInterface.IsValid() ? 1 : 0,
                 info ? reinterpret_cast<const void*>(info->methodPointer) : nullptr);
        if (!signatureOk || !fieldsOk || !interfacesOk) {
            LOG_ERR("CustomComponents/Pick3/ASSETTASK: metadata incomplete; hook NOT installed");
            return false;
        }
        BNM::BasicHook(start, StartCapture, originalStart);
        CC_DIAG("CustomComponents/Pick3/ART: capture hook installed=%d",
                 originalStart != nullptr ? 1 : 0);
        return originalStart != nullptr;
    }
}
