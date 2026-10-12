#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <string>

#include "log.h"

namespace CustomComponents::Utilities::DeckMaterializer {

using Obj = BNM::IL2CPP::Il2CppObject;
template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

// DeckInitializer -> CardPoolInitializer -> DeckFactory -> CardEntityFactory.
// All are stock, dependency-injected live objects. We do not construct any
// engine utility ourselves.
inline BNM::Field<Obj*> deckInitializerEntityModel;
inline BNM::Field<Obj*> deckInitializerCardPoolInitializer;
inline BNM::Field<Obj*> cardPoolInitializerDeckFactory;
inline BNM::Field<Obj*> deckFactoryCardEntityFactory;

inline BNM::Field<MonoList<Obj*>*> entityComponents;
inline BNM::Field<int> entityId;
inline BNM::Field<int> deckAssociationDeckEntityId;
inline BNM::Field<MonoList<Obj*>*> orderedPoolEntries;
inline BNM::Field<int> poolEntryGuid;
inline BNM::Field<int> poolEntryEntityId;

inline BNM::Method<Obj*> modelGetEntityById;
inline BNM::Method<Obj*> createCardWithDefaultOwner;



using InitializeDeckFn = void (*)(void*, Obj*, Obj*, const void*);
inline InitializeDeckFn originalInitializeDeck = nullptr;


template<typename T>
inline T* FindComponent(Obj* entity, const char* wantedName) {
    if (!entity)
        return nullptr;

    auto* components = entityComponents[entity].Get();
    if (!components || !components->items)
        return nullptr;

    for (int i = 0; i < components->size; ++i) {
        Obj* component = components->items->m_Items[i];
        if (!component)
            continue;

        auto* klass = BNM::Class(component).GetClass();
        if (klass && klass->name && std::string(klass->name) == wantedName)
            return reinterpret_cast<T*>(component);
    }

    return nullptr;
}

// -----------------------------------------------------------------------------
// Entity-backed deck lifetime
//
// Stock RemoveExtraCardsInDeckSystem handles MulliganPhaseEndEffect by
// destroying every materialized card still in a deck and resetting each
// CardPoolEntry.EntityId to -1. That is correct for the vanilla virtual-deck
// model, but it destroys the identity/state required by CustomComponents.
//
// Phase 0 deliberately suppresses that one stock cleanup system. The existing
// CardPoolEntry objects and their real EntityIds then survive the final mulligan
// shuffle and continue to represent the cards in the deck.
// -----------------------------------------------------------------------------

using RemoveExtraCardsProcessFn = void (*)(void*, Obj*, const void*);
inline RemoveExtraCardsProcessFn originalRemoveExtraCardsProcess = nullptr;

static void RemoveExtraCardsProcessHook(
    void* self,
    Obj* effect,
    const void* method
) {
    (void)self;
    (void)effect;
    (void)method;

    // Intentionally do NOT call originalRemoveExtraCardsProcess().
    //
    // Stock behavior:
    //   RemoveExtraCardsInDeck(deck)
    //     -> TryDestroyCard(entry)
    //        -> EntityDestroyer destroys entry.EntityId
    //        -> entry.EntityId = -1
    //
    // Our deck model requires those entities and IDs to remain alive.
}


inline void MaterializePlayerDeck(void* initializer, Obj* playerEntity) {
    if (!initializer || !playerEntity) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: invalid InitializeDeck context initializer=%p playerEntity=%p",
            initializer, playerEntity
        );
        return;
    }

    Obj* model = deckInitializerEntityModel[static_cast<Obj*>(initializer)].Get();
    Obj* poolInitializer = deckInitializerCardPoolInitializer[static_cast<Obj*>(initializer)].Get();
    Obj* deckFactory = poolInitializer ? cardPoolInitializerDeckFactory[poolInitializer].Get() : nullptr;
    Obj* cardFactory = deckFactory ? deckFactoryCardEntityFactory[deckFactory].Get() : nullptr;

    if (!model || !poolInitializer || !deckFactory || !cardFactory) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: stock object graph unavailable initializer=%p Model=%p CardPoolInitializer=%p DeckFactory=%p CardFactory=%p",
            initializer, model, poolInitializer, deckFactory, cardFactory
        );
        return;
    }

    Obj* deckAssociation = FindComponent<Obj>(playerEntity, "DeckAssociation");
    if (!deckAssociation) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: playerEntity=%p has no DeckAssociation",
            playerEntity
        );
        return;
    }

    const int deckId = deckAssociationDeckEntityId[deckAssociation].Get();
    if (deckId <= 0) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: invalid DeckAssociation.DeckEntityId=%d playerEntity=%p",
            deckId, playerEntity
        );
        return;
    }

    Obj* deckEntity = modelGetEntityById[model](deckId);
    if (!deckEntity) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: EntityModel.GetEntityById(%d) returned null",
            deckId
        );
        return;
    }

    Obj* orderedPool = FindComponent<Obj>(deckEntity, "OrderedCardPool");
    if (!orderedPool) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: DeckId=%d entity=%p has no OrderedCardPool",
            deckId, deckEntity
        );
        return;
    }

    auto* entries = orderedPoolEntries[orderedPool].Get();
    if (!entries || !entries->items) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: DeckId=%d OrderedCardPool=%p has null Entries",
            deckId, orderedPool
        );
        return;
    }

    int already = 0;
    int created = 0;
    int failed = 0;

    for (int i = 0; i < entries->size; ++i) {
        Obj* entry = entries->items->m_Items[i];
        if (!entry) {
            ++failed;
            LOG_ERR(
                "CustomComponents/DeckMaterializer: NULL ENTRY DeckId=%d index=%d",
                deckId, i
            );
            continue;
        }

        const int guid = poolEntryGuid[entry].Get();
        const int oldEntityId = poolEntryEntityId[entry].Get();

        if (oldEntityId > 0) {
            ++already;
            continue;
        }

        if (guid <= 0) {
            ++failed;
            LOG_ERR(
                "CustomComponents/DeckMaterializer: INVALID GUID DeckId=%d index=%d Guid=%d EntityId=%d",
                deckId, i, guid, oldEntityId
            );
            continue;
        }

        // Stock path: creates/loads the full card entity, creates its grantable
        // triggered-ability entities, and applies stock faction ownership.
        Obj* card = createCardWithDefaultOwner[cardFactory](guid);
        if (!card) {
            ++failed;
            LOG_ERR(
                "CustomComponents/DeckMaterializer: CREATE FAILED DeckId=%d index=%d Guid=%d",
                deckId, i, guid
            );
            continue;
        }

        const int newEntityId = entityId[card].Get();
        if (newEntityId <= 0) {
            ++failed;
            LOG_ERR(
                "CustomComponents/DeckMaterializer: CREATED INVALID ENTITY DeckId=%d index=%d Guid=%d card=%p EntityId=%d",
                deckId, i, guid, card, newEntityId
            );
            continue;
        }

        // Preserve the existing CardPoolEntry and list order. Only transition
        // the entry from (Guid, -1) to (Guid, real EntityId).
        poolEntryEntityId[entry] = newEntityId;
        ++created;
    }

    LOG_INFO(
        "CustomComponents/DeckMaterializer: COMPLETE DeckId=%d entries=%d created=%d already=%d failed=%d",
        deckId, entries->size, created, already, failed
    );
}

static void InitializeDeckHook(
    void* self,
    Obj* playerData,
    Obj* playerEntity,
    const void* method
) {
    // Runtime tracing proved that the normal starting deck's AddCardGuids call
    // completes inside InitializeDeck. Materialize only after stock has finished
    // constructing and associating the player's deck.
    originalInitializeDeck(self, playerData, playerEntity, method);
    MaterializePlayerDeck(self, playerEntity);
}

inline bool Initialize() {
    auto deckInitializer = BNM::Class("PvZCards.Core.Utilities", "DeckInitializer");
    auto cardPoolInitializer = BNM::Class("PvZCards.Core.Utilities", "CardPoolInitializer");
    auto deckFactory = BNM::Class("PvZCards.Core.Utilities", "DeckFactory");
    auto cardFactory = BNM::Class("PvZCards.Core.Utilities", "CardEntityFactory");
    auto model = BNM::Class("PvZCards.Engine", "EntityModel");
    auto entity = BNM::Class("PvZCards.Engine", "Entity");
    auto deckAssociation = BNM::Class("PvZCards.Engine.Components", "DeckAssociation");
    auto orderedPool = BNM::Class("PvZCards.Engine.Components", "OrderedCardPool");
    auto poolEntry = BNM::Class("PvZCards.Engine.Components.Data", "CardPoolEntry");
    auto removeExtraCardsSystem =
        BNM::Class("PvZCards.Core.Systems", "RemoveExtraCardsInDeckSystem");

    if (!deckInitializer.IsValid() || !cardPoolInitializer.IsValid() ||
        !deckFactory.IsValid() || !cardFactory.IsValid() || !model.IsValid() ||
        !entity.IsValid() || !deckAssociation.IsValid() || !orderedPool.IsValid() ||
        !poolEntry.IsValid() || !removeExtraCardsSystem.IsValid()) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: required classes missing DeckInitializer=%d CardPoolInitializer=%d DeckFactory=%d CardFactory=%d Model=%d Entity=%d DeckAssociation=%d OrderedCardPool=%d CardPoolEntry=%d RemoveExtraCardsInDeckSystem=%d",
            deckInitializer.IsValid(), cardPoolInitializer.IsValid(), deckFactory.IsValid(),
            cardFactory.IsValid(), model.IsValid(), entity.IsValid(), deckAssociation.IsValid(),
            orderedPool.IsValid(), poolEntry.IsValid(), removeExtraCardsSystem.IsValid()
        );
        return false;
    }

    deckInitializerEntityModel = deckInitializer.GetField("EntityModel");
    deckInitializerCardPoolInitializer = deckInitializer.GetField("CardPoolInitializer");
    cardPoolInitializerDeckFactory = cardPoolInitializer.GetField("DeckFactory");
    deckFactoryCardEntityFactory = deckFactory.GetField("<CardEntityFactory>k__BackingField");

    entityComponents = entity.GetField("Components");
    entityId = entity.GetField("<Id>k__BackingField");
    deckAssociationDeckEntityId = deckAssociation.GetField("DeckEntityId");
    orderedPoolEntries = orderedPool.GetField("Entries");
    poolEntryGuid = poolEntry.GetField("Guid");
    poolEntryEntityId = poolEntry.GetField("EntityId");

    modelGetEntityById = model.GetMethod("GetEntityById", 1).cast<Obj*>();
    createCardWithDefaultOwner =
        cardFactory.GetMethod("CreateCardEntityWithDefaultOwner", 1).cast<Obj*>();

    auto initializeDeck = deckInitializer.GetMethod("InitializeDeck", 2);
    auto removeExtraCardsProcess = removeExtraCardsSystem.GetMethod("ProcessEffect", 1);

    if (!deckInitializerEntityModel.IsValid() ||
        !deckInitializerCardPoolInitializer.IsValid() ||
        !cardPoolInitializerDeckFactory.IsValid() ||
        !deckFactoryCardEntityFactory.IsValid() ||
        !entityComponents.IsValid() || !entityId.IsValid() ||
        !deckAssociationDeckEntityId.IsValid() || !orderedPoolEntries.IsValid() ||
        !poolEntryGuid.IsValid() || !poolEntryEntityId.IsValid() ||
        !modelGetEntityById.IsValid() || !createCardWithDefaultOwner.IsValid() ||
        !initializeDeck.IsValid() || !removeExtraCardsProcess.IsValid()) {
        LOG_ERR("CustomComponents/DeckMaterializer: required API missing");
        return false;
    }

    BNM::BasicHook(initializeDeck, InitializeDeckHook, originalInitializeDeck);
    BNM::BasicHook(
        removeExtraCardsProcess,
        RemoveExtraCardsProcessHook,
        originalRemoveExtraCardsProcess
    );
    if (!originalInitializeDeck || !originalRemoveExtraCardsProcess) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: hook installation failed InitializeDeck=%p RemoveExtraCards=%p",
            originalInitializeDeck, originalRemoveExtraCardsProcess
        );
        return false;
    }

    LOG_INFO("CustomComponents/DeckMaterializer: ready (persistent entity-backed decks)");
    return true;
}

} // namespace CustomComponents::Utilities::DeckMaterializer
