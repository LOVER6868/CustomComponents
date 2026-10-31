#pragma once
// Diagnostic ONLY: observe stock EntityModel.DeepCopyFrom; never create entities.
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Exceptions.hpp>
#include "log.h"

namespace CustomComponents::Diagnostics::PredictionIsolationAudit {
using Obj = BNM::IL2CPP::Il2CppObject;
template <typename T> using MonoList = BNM::Structures::Mono::List<T>;
inline BNM::Field<int> nextId;
inline BNM::Field<MonoList<Obj*>*> entities;
using DeepCopyFn = void (*)(Obj*, Obj*, Obj*, const void*);
inline DeepCopyFn original = nullptr;
inline int observedCopies = 0;

inline void OnDeepCopy(Obj* destination, Obj* source, Obj* manager, const void* method) {
    // Capture before copy. Reading these fields has no game-state side effects.
    const int before = destination && nextId.IsValid() ? nextId[destination].Get() : -1;
    original(destination, source, manager, method);
    // Limit reports to make ADB traces readable and avoid expensive logging.
    if (++observedCopies > 12 || !destination || !source || !nextId.IsValid() || !entities.IsValid()) return;
    auto* srcEntities = entities[source].Get();
    auto* dstEntities = entities[destination].Get();
    const int srcCount = srcEntities ? srcEntities->size : -1;
    const int dstCount = dstEntities ? dstEntities->size : -1;
    const int srcNext = nextId[source].Get();
    const int dstNext = nextId[destination].Get();
    LOG_INFO("CustomComponents/Pick3/CLONE_AUDIT: n=%d src=%p dst=%p distinct=%d srcEntities=%d dstEntities=%d srcNext=%d dstNext=%d dstBefore=%d countMatch=%d nextMatch=%d",
        observedCopies, static_cast<void*>(source), static_cast<void*>(destination), source != destination,
        srcCount, dstCount, srcNext, dstNext, before, srcCount == dstCount, srcNext == dstNext);
}

inline bool Install() {
    BNM::Class model("PvZCards.Engine", "EntityModel");
    if (!model) {
        LOG_ERR("CustomComponents/Pick3/CLONE_AUDIT: EntityModel missing");
        return false;
    }
    nextId = model.GetField("NextEntityId");
    entities = model.GetField("Entities");
    auto copy = model.GetMethod("DeepCopyFrom", 2);
    if (!nextId.IsValid() || !entities.IsValid() || !copy.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/CLONE_AUDIT: fields or DeepCopyFrom missing");
        return false;
    }
    BNM::BasicHook(copy, OnDeepCopy, original);
    const bool ok = original != nullptr;
    LOG_INFO("CustomComponents/Pick3/CLONE_AUDIT: hook=%d observerOnly=1 noCardCreation=1", ok);
    return ok;
}
}
