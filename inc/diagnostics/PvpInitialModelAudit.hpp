#pragma once
// Read-only audit of the PvP InitialGameState model returned by the custom server.
// The server starts with GUID-only OrderedCardPool entries (EntityId == -1).
// Observe the actual model after stock initialization, never edit it here.

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include <string>
#include "log.h"

namespace CustomComponents::Diagnostics::PvpInitialModelAudit {
using Obj = BNM::IL2CPP::Il2CppObject;
template<class T> using MonoList = BNM::Structures::Mono::List<T>;

inline BNM::Field<Obj*> responseEntityModel;
inline BNM::Field<Obj*> responseGamePredictor;
inline BNM::Field<MonoList<Obj*>*> modelEntities;
inline BNM::Field<int> modelNextId;
inline BNM::Field<MonoList<Obj*>*> entityComponents;
inline BNM::Field<MonoList<Obj*>*> orderedPoolEntries;
inline BNM::Field<int> entryEntityId;
inline BNM::Method<Obj*> getEntityById;
using InitFn = void (*)(Obj*, Obj*, const void*);
inline InitFn originalInitializeGame = nullptr;
inline unsigned snapshots = 0;

inline Obj* FindOrderedPool(Obj* entity) {
    if (!entity) return nullptr;
    auto* components = entityComponents[entity].Get();
    if (!components || !components->items || components->size < 0 || components->size > 512)
        return nullptr;
    for (int i = 0; i < components->size; ++i) {
        Obj* component = components->items->m_Items[i];
        if (!component || !component->klass || !component->klass->name) continue;
        if (std::string(component->klass->name) == "OrderedCardPool") return component;
    }
    return nullptr;
}

inline void ReportPool(Obj* model, int entityId) {
    Obj* deck = nullptr;
    const auto exception = BNM::TryInvoke([&] { deck = getEntityById[model](entityId); });
    if (exception.IsValid() || !deck) {
        LOG_INFO("CustomComponents/PVP_INIT_AUDIT: poolEntity=%d found=0", entityId);
        return;
    }
    Obj* pool = FindOrderedPool(deck);
    if (!pool) {
        LOG_INFO("CustomComponents/PVP_INIT_AUDIT: poolEntity=%d orderedPool=0", entityId);
        return;
    }
    auto* entries = orderedPoolEntries[pool].Get();
    if (!entries || !entries->items || entries->size < 0 || entries->size > 10000) {
        LOG_INFO("CustomComponents/PVP_INIT_AUDIT: poolEntity=%d entriesInvalid=1", entityId);
        return;
    }
    int live = 0, virtualCards = 0, invalid = 0;
    for (int i = 0; i < entries->size; ++i) {
        Obj* entry = entries->items->m_Items[i];
        if (!entry) { ++invalid; continue; }
        const int id = entryEntityId[entry].Get();
        if (id > 0) ++live;
        else if (id == -1) ++virtualCards;
        else ++invalid;
    }
    LOG_INFO("CustomComponents/PVP_INIT_AUDIT: poolEntity=%d entries=%d live=%d virtual=%d invalid=%d", entityId, entries->size, live, virtualCards, invalid);
}

static void InitializeGameHook(Obj* self, Obj* response, const void* method) {
    // Always preserve stock initialization and its exceptions/results.
    originalInitializeGame(self, response, method);
    if (!self || !responseEntityModel.IsValid() || !modelEntities.IsValid() ||
        !modelNextId.IsValid() || !entityComponents.IsValid() ||
        !orderedPoolEntries.IsValid() || !entryEntityId.IsValid() ||
        !getEntityById.IsValid()) return;
    // Exactly one short report per initialized match; never touch card entities.
    if (++snapshots > 6) return;
    Obj* model = responseEntityModel[self].Get();
    Obj* predictor = responseGamePredictor.IsValid() ? responseGamePredictor[self].Get() : nullptr;
    if (!model) {
        LOG_INFO("CustomComponents/PVP_INIT_AUDIT: snapshot=%u model=0", snapshots);
        return;
    }
    auto* entities = modelEntities[model].Get();
    LOG_INFO("CustomComponents/PVP_INIT_AUDIT: snapshot=%u model=%p predictor=%p entities=%d nextId=%d writes=0",
             snapshots, model, predictor, entities ? entities->size : -1, modelNextId[model].Get());
    // These are the fixed deck IDs emitted by the referenced base server.
    // If a different server layout is used, a missing pool is only diagnostic.
    ReportPool(model, 1); // zombie main
    ReportPool(model, 2); // zombie super
    ReportPool(model, 6); // plant main
    ReportPool(model, 7); // plant super
}

inline bool Install() {
    BNM::Class executable("PvZCards.Multiplayer.Networking", "PvpInitResponseExecutable");
    BNM::Class model("PvZCards.Engine", "EntityModel");
    BNM::Class entity("PvZCards.Engine", "Entity");
    BNM::Class pool("PvZCards.Engine.Components", "OrderedCardPool");
    BNM::Class entry("PvZCards.Engine.Components.Data", "CardPoolEntry");
    if (!executable.IsValid() || !model.IsValid() || !entity.IsValid() ||
        !pool.IsValid() || !entry.IsValid()) return false;
    responseEntityModel = executable.GetField("<EntityModel>k__BackingField");
    responseGamePredictor = executable.GetField("<GamePredictor>k__BackingField");
    modelEntities = model.GetField("Entities");
    modelNextId = model.GetField("NextEntityId");
    entityComponents = entity.GetField("Components");
    orderedPoolEntries = pool.GetField("Entries");
    entryEntityId = entry.GetField("EntityId");
    getEntityById = model.GetMethod("GetEntityById", 1).cast<Obj*>();
    auto initializeGame = executable.GetMethod("InitializeGame", 1);
    if (!responseEntityModel.IsValid() || !modelEntities.IsValid() ||
        !modelNextId.IsValid() || !entityComponents.IsValid() ||
        !orderedPoolEntries.IsValid() || !entryEntityId.IsValid() ||
        !getEntityById.IsValid() || !initializeGame.IsValid()) return false;
    BNM::BasicHook(initializeGame, InitializeGameHook, originalInitializeGame);
    const bool ok = originalInitializeGame != nullptr;
    LOG_INFO("CustomComponents/PVP_INIT_AUDIT: install=%d readOnly=1 enabled=1", ok);
    return ok;
}
} // namespace CustomComponents::Diagnostics::PvpInitialModelAudit
