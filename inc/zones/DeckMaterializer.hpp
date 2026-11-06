#include "engine/Events.hpp"
#include "core/HookRegistry.hpp"
#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
#include "core/SceneViewContext.hpp"
#include "queries/InDeckQuery.hpp"
#include <BNM/Exceptions.hpp>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include "diagnostics/PvpInsertionDelta.hpp"
#include "diagnostics/PvpInsertionOwnerConsensus.hpp"
#include "diagnostics/PvpInsertionSafeLookup.hpp"

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
inline BNM::Field<Obj*> cardFactoryEntityModel;
inline std::uint32_t boundFactoryRoot = 0;
inline std::uint32_t boundModelRoot = 0;

inline BNM::Field<MonoList<Obj*>*> entityComponents;
inline BNM::Field<MonoList<Obj*>*> modelEntities;
inline BNM::Field<int> entityId;
inline BNM::Field<int> deckAssociationDeckEntityId;
inline BNM::Field<MonoList<Obj*>*> orderedPoolEntries;
inline BNM::Field<int> poolEntryGuid;
inline BNM::Field<int> poolEntryEntityId;

inline BNM::Method<Obj*> modelGetEntityById;
inline BNM::Method<Obj*> createCardWithDefaultOwner;
// Sow Magic Beans inserts virtual GUID entries through the stock create-in-deck
// system. DiscardPileZoneEffects owns its sole ProcessEffect hook; this module
// provides the non-hooking before/after observer for that existing hook.
inline thread_local bool repairingCreatedEntries = false;

// The PvP session is bound ONLY after all 88 stock prepopulation results pass.
// Rooted live model + *same* stock system distinguish it from prediction clones.
// The stock private helper creates a single CardPoolEntry through exactly the
// same path used by PrepopulateCardPoolSystem, without rerunning its pool scan.
inline std::atomic<bool> pvpInsertRequested{false};
inline std::atomic<bool> pvpInsertApisReady{false};
inline std::atomic<bool> pvpSessionFailed{false};
inline std::atomic<std::uint32_t> pvpModelRoot{0};
inline std::atomic<std::uint32_t> pvpSystemRoot{0};
inline BNM::Field<Obj*> pvpSystemModel;
inline BNM::Field<Obj*> pvpSystemFactory;
inline BNM::Field<Obj*> pvpFactoryDatabase;
inline BNM::Field<int> modelNextEntityId;
inline BNM::Field<int> cardComponentGuid;
inline BNM::Field<int> pvpOwnerId;
inline BNM::Method<Obj*> pvpGetElementType;
inline BNM::Method<void> pvpCreateCardIfMissing;
inline BNM::Method<Obj*> pvpBackingEntityByGuid;

inline Obj* PvpRootedModel() {
    return pvpModelRoot && Core::SceneViewContext::getGCHandle
        ? Core::SceneViewContext::getGCHandle(pvpModelRoot) : nullptr;
}
inline Obj* PvpRootedSystem() {
    return pvpSystemRoot && Core::SceneViewContext::getGCHandle
        ? Core::SceneViewContext::getGCHandle(pvpSystemRoot) : nullptr;
}
inline void ClearPvpSession() {
    if (Core::SceneViewContext::freeGCHandle) {
        if (pvpSystemRoot) Core::SceneViewContext::freeGCHandle(pvpSystemRoot);
        if (pvpModelRoot) Core::SceneViewContext::freeGCHandle(pvpModelRoot);
    }
    pvpSystemRoot = 0;
    pvpModelRoot = 0;
    pvpSessionFailed = false;
}
inline bool BindPvpSessionAfterVerifiedFullDeck(Obj* model, Obj* stockSystem) {
    if (!pvpInsertRequested) return false;
    if (!pvpInsertApisReady || !model || !stockSystem ||
        !Core::SceneViewContext::newGCHandle ||
        !Core::SceneViewContext::getGCHandle ||
        !Core::SceneViewContext::freeGCHandle ||
        pvpSystemModel[stockSystem].Get() != model) return false;
    Obj* factory = pvpSystemFactory[stockSystem].Get();
    if (!factory || cardFactoryEntityModel[factory].Get() != model) return false;
    ClearPvpSession();
    const auto modelHandle = Core::SceneViewContext::newGCHandle(model,false);
    const auto systemHandle = Core::SceneViewContext::newGCHandle(stockSystem,false);
    if (!modelHandle || !systemHandle) {
        if (modelHandle) Core::SceneViewContext::freeGCHandle(modelHandle);
        if (systemHandle) Core::SceneViewContext::freeGCHandle(systemHandle);
        LOG_ERR("CustomComponents/DeckMaterializer: PVP_INSERT_BIND_FAILED reason=root-allocation");
        return false;
    }
    pvpModelRoot = modelHandle;
    pvpSystemRoot = systemHandle;
    LOG_INFO("CustomComponents/DeckMaterializer: PVP_INSERT_BOUND model=%p stockSystem=%p mode=stock-entry-helper predictions=readonly",model,stockSystem);
    return true;
}





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

struct PvpPoolSnapshot {
    int id = 0;
    Obj* entity = nullptr;
    std::vector<Diagnostics::PvpInsertionDelta::Entry> entries;
};
struct InsertionSnapshot {
    int beforeVirtual = -1;
    bool pvpLive = false;
    int nextEntityId = -1;
    PvpPoolSnapshot pools[2];
};

// CRASH FIX: the experimental PvP insertion observer must never invoke
// EntityModel.GetEntityById while stock effect processing is underway. In the
// matched crash build, that call reached IL2CPP and faulted at null+0x38.
// Read the already-owned EntityModel.Entities list instead; reject ambiguous,
// missing, or malformed IDs without changing the model or allocating entities.
inline Obj* FindPvpModelEntityWithoutInvoke(Obj* model, int wantedId) {
    if (!model || !modelEntities.IsValid() || !entityId.IsValid()) return nullptr;
    auto* entities=modelEntities[model].Get();
    if (!entities || (entities->size>0 && !entities->items)) return nullptr;
    return Diagnostics::PvpInsertionSafeLookup::FindUnique<Obj>(
        entities->items ? entities->items->m_Items : nullptr,
        entities->size, wantedId,
        [](Obj* candidate) { return entityId[candidate].Get(); });
}

inline bool ReadPvpPool(Obj* model, int poolId, PvpPoolSnapshot& out) {
    out.id=poolId;
    out.entity=nullptr;
    out.entries.clear();
    out.entity=FindPvpModelEntityWithoutInvoke(model,poolId);
    if (!out.entity || !FindComponent<Obj>(out.entity,"Deck") ||
        FindComponent<Obj>(out.entity,"SuperpowerPool")) return false;
    Obj* pool=FindComponent<Obj>(out.entity,"OrderedCardPool");
    if (!pool) return false;
    auto* entries=orderedPoolEntries[pool].Get();
    if (!entries || entries->size<0 || entries->size>1024 ||
        (entries->size && !entries->items)) return false;
    out.entries.reserve(static_cast<std::size_t>(entries->size));
    for (int i=0;i<entries->size;i++) {
        Obj* entry=entries->items->m_Items[i];
        if (!entry) return false;
        const int guid=poolEntryGuid[entry].Get();
        const int id=poolEntryEntityId[entry].Get();
        if (guid<=0 || id<-1) return false;
        out.entries.push_back({entry,guid,id});
    }
    return true;
}

// Validate owner identity from ALREADY materialized cards in the destination
// deck, whose GUID/EntityId/Owner must all agree. In the initial PvP model the
// regular Deck pool can lack its own Owner component. Never infer an owner
// merely from deck index (1/6) or the acting card's owner.
inline Diagnostics::PvpInsertionOwnerConsensus::Result ExistingPvpDeckOwner(
    Obj* model, const PvpPoolSnapshot& pool) {
    using namespace Diagnostics::PvpInsertionOwnerConsensus;
    std::vector<PhysicalCopy> proofs;
    proofs.reserve(pool.entries.size());
    for (const auto& entry : pool.entries) {
        if (entry.entityId == -1) continue;
        PhysicalCopy proof{};
        proof.poolGuid = entry.guid;
        proof.poolEntityId = entry.entityId;
        // No managed GetEntityById call here: that was the shared native
        // crash site in three captured shuffle/draw-trigger failures.
        Obj* cardEntity = FindPvpModelEntityWithoutInvoke(model,entry.entityId);
        if (cardEntity) {
            proof.cardEntityId = entityId[cardEntity].Get();
            Obj* card = FindComponent<Obj>(cardEntity,"Card");
            Obj* owner = FindComponent<Obj>(cardEntity,"Owner");
            proof.hasCard = card != nullptr;
            proof.hasOwner = owner != nullptr;
            if (card) proof.cardGuid = cardComponentGuid[card].Get();
            if (owner) proof.ownerId = pvpOwnerId[owner].Get();
        }
        proofs.push_back(proof);
    }
    return Resolve(proofs);
}

// -----------------------------------------------------------------------------
// Entity-backed deck lifetime
//
// Stock RemoveExtraCardsInDeckSystem handles MulliganPhaseEndEffect by
// destroying every materialized card still in a deck and resetting each
// CardPoolEntry.EntityId to -1. That is correct for the vanilla virtual-deck
// model, but it destroys the identity/state required by CustomComponents.
//
// Deliberately suppress that one stock cleanup system. The existing
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


inline int MaterializeModelDeck(Obj* model, Obj* cardFactory, Obj* playerEntity) {
    if (!model || !cardFactory || !playerEntity || !cardFactoryEntityModel.IsValid() ||
        cardFactoryEntityModel[cardFactory].Get() != model) {
        LOG_ERR("CustomComponents/DeckMaterializer: model/factory identity mismatch model=%p factory=%p", model, cardFactory);
        return 0;
    }


    Obj* deckAssociation = FindComponent<Obj>(playerEntity, "DeckAssociation");
    if (!deckAssociation) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: playerEntity=%p has no DeckAssociation",
            playerEntity
        );
        return 0;
    }

    const int deckId = deckAssociationDeckEntityId[deckAssociation].Get();
    if (deckId <= 0) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: invalid DeckAssociation.DeckEntityId=%d playerEntity=%p",
            deckId, playerEntity
        );
        return 0;
    }

    Obj* deckEntity = modelGetEntityById[model](deckId);
    if (!deckEntity) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: EntityModel.GetEntityById(%d) returned null",
            deckId
        );
        return 0;
    }

    Obj* orderedPool = FindComponent<Obj>(deckEntity, "OrderedCardPool");
    if (!orderedPool) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: DeckId=%d entity=%p has no OrderedCardPool",
            deckId, deckEntity
        );
        return 0;
    }

    auto* entries = orderedPoolEntries[orderedPool].Get();
    if (!entries || !entries->items) {
        LOG_ERR(
            "CustomComponents/DeckMaterializer: DeckId=%d OrderedCardPool=%p has null Entries",
            deckId, orderedPool
        );
        return 0;
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

    CC_DIAG(
        "CustomComponents/DeckMaterializer: COMPLETE DeckId=%d entries=%d created=%d already=%d failed=%d",
        deckId, entries->size, created, already, failed
    );
    return created;
}


inline void MaterializePlayerDeck(void* initializer, Obj* playerEntity) {
    if (!initializer || !playerEntity) return;
    Obj* model = deckInitializerEntityModel[static_cast<Obj*>(initializer)].Get();
    Obj* poolInitializer = deckInitializerCardPoolInitializer[static_cast<Obj*>(initializer)].Get();
    Obj* deckFactory = poolInitializer ? cardPoolInitializerDeckFactory[poolInitializer].Get() : nullptr;
    Obj* factory = deckFactory ? deckFactoryCardEntityFactory[deckFactory].Get() : nullptr;
    if (!model || !factory) return;
    if (PvpRootedModel() && PvpRootedModel()!=model)
        ClearPvpSession(); // release the previous PvP session on PvE transition
    if (Core::SceneViewContext::newGCHandle && Core::SceneViewContext::getGCHandle &&
        Core::SceneViewContext::freeGCHandle) {
        if (boundFactoryRoot) Core::SceneViewContext::freeGCHandle(boundFactoryRoot);
        if (boundModelRoot) Core::SceneViewContext::freeGCHandle(boundModelRoot);
        boundFactoryRoot = Core::SceneViewContext::newGCHandle(factory, false);
        boundModelRoot = Core::SceneViewContext::newGCHandle(model, false);
    }
    MaterializeModelDeck(model, factory, playerEntity);
}

// Called only for a local PvE finder, from its real EntityModel.  The factory
// is NOT transferable between prediction/simulation copies.  A model identity
// mismatch is an unconditional no-op, not permission to allocate there.
inline int RepairSameModelDeckEntries(Obj* targetModel) {
    if (!targetModel || !boundFactoryRoot || !boundModelRoot ||
        !Core::SceneViewContext::getGCHandle || !modelEntities.IsValid())
        return 0;
    Obj* model = Core::SceneViewContext::getGCHandle(boundModelRoot);
    Obj* factory = Core::SceneViewContext::getGCHandle(boundFactoryRoot);
    if (!factory || model != targetModel || cardFactoryEntityModel[factory].Get() != model)
        return 0;
    // The target finder owns this same model.  Enumerate its actual player
    // entities; do not infer players or card types from pool entries.
    auto* entities = modelEntities[model].Get();
    if (!entities || !entities->items || entities->size < 0 || entities->size > 100000)
        return 0;
    int created = 0;
    for (int i=0; i<entities->size; ++i) {
        Obj* player = entities->items->m_Items[i];
        if (!player || !FindComponent<Obj>(player,"DeckAssociation")) continue;
        auto exception = BNM::TryInvoke([&] {created += MaterializeModelDeck(model,factory,player);});
        if (exception.IsValid()) {
            LOG_ERR("CustomComponents/DeckMaterializer: live repair exception at player index=%d",i);
            break;
        }
    }
    return created;
}

// Count ONLY regular Deck entries, not the separate 4-card SuperpowerPool.
// This read-only snapshot also operates on PvP models with no bound factory.
inline int CountVirtualMainDeckEntries(Obj* model) {
    if (!model || !modelEntities.IsValid()) return -1;
    auto* entities=modelEntities[model].Get();
    if (!entities || !entities->items || entities->size<0 || entities->size>100000) return -1;
    int virtualCards=0;
    for(int i=0;i<entities->size;i++) {
        Obj* entity=entities->items->m_Items[i];
        if (!entity || !FindComponent<Obj>(entity,"Deck") ||
            FindComponent<Obj>(entity,"SuperpowerPool")) continue;
        Obj* pool=FindComponent<Obj>(entity,"OrderedCardPool");
        if (!pool) continue;
        auto* entries=orderedPoolEntries[pool].Get();
        if (!entries || entries->size<0 || entries->size>10000 ||
            (entries->size && !entries->items)) return -1;
        for(int j=0;j<entries->size;j++) {
            Obj* entry=entries->items->m_Items[j];
            if (!entry) return -1;
            if (poolEntryEntityId[entry].Get()==-1) ++virtualCards;
        }
    }
    return virtualCards;
}

// Existing single hook owner captures the stock pre-state. A candidate PvP
// match must match the exact rooted model created at initial prepopulation;
// all prediction clones and nonmatching models remain read-only.
inline InsertionSnapshot BeforeStockCreateCardInDeck(Obj* model) {
    InsertionSnapshot snapshot{};
    if (repairingCreatedEntries) return snapshot;
    snapshot.beforeVirtual=CountVirtualMainDeckEntries(model);
    if (!pvpInsertRequested || !pvpInsertApisReady ||
        !model || model!=PvpRootedModel() || !PvpRootedSystem()) return snapshot;
    // A previous skipped/failed virtual batch cannot be silently repaired in
    // an unrelated later effect, even if both use the same card GUID.
    if (pvpSessionFailed || snapshot.beforeVirtual!=0) return snapshot;
    if (!modelNextEntityId.IsValid() || !modelEntities.IsValid()) return snapshot;
    snapshot.nextEntityId=modelNextEntityId[model].Get();
    auto* entities=modelEntities[model].Get();
    if (!entities || !entities->items || entities->size!=snapshot.nextEntityId ||
        snapshot.nextEntityId<=16 || snapshot.nextEntityId>200000) return snapshot;
    if (!ReadPvpPool(model,1,snapshot.pools[0]) ||
        !ReadPvpPool(model,6,snapshot.pools[1])) return snapshot;
    snapshot.pvpLive=true;
    return snapshot;
}

// Rooted initial match only. Stock helper is invoked on each *new* virtual
// entry in the post-insertion pool order. All old entries are checked unchanged
// before the first allocation. We never rematerialize old virtual cards.
inline void MaterializePvpInsertions(Obj* model,const InsertionSnapshot& before,
                                     int expectedGuid,int requestedAmount) {
    if (!before.pvpLive || !model || pvpSessionFailed || model!=PvpRootedModel()) return;
    PvpPoolSnapshot after[2];
    if (!ReadPvpPool(model,1,after[0]) || !ReadPvpPool(model,6,after[1])) {
        LOG_ERR("CustomComponents/DeckMaterializer: PVP_INSERT_SKIP reason=unreadable-post-stock-pools writes=0");
        return;
    }
    using namespace Diagnostics::PvpInsertionDelta;
    int target=-1;
    std::vector<std::size_t> newIndices;
    const char* reason="unverified";
    for (int n=0;n<2;n++) {
        if (after[n].entity!=before.pools[n].entity) {
            reason="pool-entity-changed"; goto reject;
        }
        if (after[n].entries.size()==before.pools[n].entries.size()) {
            // Check all old entry objects and fields even if size is unchanged.
            const auto& old=before.pools[n].entries;
            std::vector<bool> seen(old.size(),false);
            for (const auto& entry:after[n].entries) {
                bool found=false;
                for (std::size_t j=0;j<old.size();j++) {
                    const auto& b=old[j];
                    if (entry.identity==b.identity && entry.guid==b.guid &&
                        entry.entityId==b.entityId && !seen[j]) {
                        seen[j]=true;found=true;break;
                    }
                }
                if (!found) {reason="unmodified-pool-changed"; goto reject;}
            }
            continue;
        }
        if (target!=-1) {reason="multiple-pools-changed";goto reject;}
        auto delta=Compare(before.pools[n].entries,after[n].entries,
                           expectedGuid,requestedAmount);
        if (!delta.valid) {reason=delta.reason; goto reject;}
        target=n;
        newIndices=std::move(delta.newIndices);
    }
    if (target==-1) return; // the stock effect did not insert into a main pool
    {
        Obj* system=PvpRootedSystem();
        if (!system || pvpSystemModel[system].Get()!=model ||
            !pvpSystemFactory[system].Get() ||
            cardFactoryEntityModel[pvpSystemFactory[system].Get()].Get()!=model) {
            reason="stock-system-model-mismatch";goto reject;
        }
        Obj* factory=pvpSystemFactory[system].Get();
        Obj* database=pvpFactoryDatabase[factory].Get();
        if (!database) {reason="missing-card-database";goto reject;}
        // Validate every new GUID before a stock mutation.
        Obj* definition=nullptr;
        auto check=BNM::TryInvoke([&]{definition=pvpBackingEntityByGuid[database](expectedGuid);});
        if (check.IsValid() || !definition) {reason="unknown-card-definition";goto reject;}
        Obj* cardDefinition=FindComponent<Obj>(definition,"Card");
        if (!cardDefinition || cardComponentGuid[cardDefinition].Get()!=expectedGuid) {
            reason="definition-guid-mismatch";goto reject;
        }
        const auto ownerProof=ExistingPvpDeckOwner(model,after[target]);
        if (!ownerProof.valid) {reason=ownerProof.reason;goto reject;}
        const int expectedOwner=ownerProof.ownerId;
        LOG_INFO("CustomComponents/DeckMaterializer: PVP_INSERT_OWNER pool=%d owner=%d verifiedCopies=%d evidence=existing-physical-cards writes=0",
            after[target].id,expectedOwner,ownerProof.checked);
        Obj* elementType=nullptr;
        auto typeError=BNM::TryInvoke([&] {
            elementType=pvpGetElementType[system](after[target].entity);
        });
        if (typeError.IsValid() || !elementType) {reason="element-type-unavailable";goto reject;}
        const int initialNext=modelNextEntityId[model].Get();
        auto* initialEntities=modelEntities[model].Get();
        if (initialNext!=before.nextEntityId || !initialEntities ||
            initialEntities->size!=initialNext) {
            reason="model-changed-during-stock-insert";goto reject;
        }
        std::uint64_t fingerprint=14695981039346656037ull;
        Mix32(fingerprint,static_cast<std::uint32_t>(after[target].id));
        Mix32(fingerprint,static_cast<std::uint32_t>(expectedGuid));
        Mix32(fingerprint,static_cast<std::uint32_t>(requestedAmount));
        Mix32(fingerprint,static_cast<std::uint32_t>(initialNext));
        int created=0;
        pvpSessionFailed=true; // claim once, including partial exceptions
        repairingCreatedEntries=true;
        for (auto index:newIndices) {
            Obj* entry=const_cast<Obj*>(static_cast<const Obj*>(after[target].entries[index].identity));
            auto err=BNM::TryInvoke([&]{pvpCreateCardIfMissing[system](entry,elementType);});
            if (err.IsValid()) {
                repairingCreatedEntries=false;
                LOG_ERR("CustomComponents/DeckMaterializer: PVP_INSERT_ERROR pool=%d guid=%d completed=%d requested=%d exception=%s partialStatePossible=1",
                    after[target].id,expectedGuid,created,requestedAmount,err.ClassName().c_str());
                return;
            }
            const int id=poolEntryEntityId[entry].Get();
            Obj* materialized=FindPvpModelEntityWithoutInvoke(model,id);
            Obj* card=materialized?FindComponent<Obj>(materialized,"Card"):nullptr;
            Obj* cardOwner=materialized?FindComponent<Obj>(materialized,"Owner"):nullptr;
            if (id<initialNext || !materialized || entityId[materialized].Get()!=id ||
                !card || cardComponentGuid[card].Get()!=expectedGuid ||
                !cardOwner || pvpOwnerId[cardOwner].Get()!=expectedOwner) {
                repairingCreatedEntries=false;
                LOG_ERR("CustomComponents/DeckMaterializer: PVP_INSERT_ERROR pool=%d guid=%d id=%d reason=stock-result-invalid partialStatePossible=1",
                    after[target].id,expectedGuid,id);
                return;
            }
            Mix32(fingerprint,static_cast<std::uint32_t>(index));
            Mix32(fingerprint,static_cast<std::uint32_t>(id));
            ++created;
        }
        repairingCreatedEntries=false;
        const int endNext=modelNextEntityId[model].Get();
        const auto* entities=modelEntities[model].Get();
        if (!entities || endNext!=entities->size || created!=requestedAmount) {
            LOG_ERR("CustomComponents/DeckMaterializer: PVP_INSERT_ERROR pool=%d reason=final-model-shape partialStatePossible=1",after[target].id);
            return;
        }
        Mix32(fingerprint,static_cast<std::uint32_t>(endNext));
        pvpSessionFailed=false;
        LOG_INFO("CustomComponents/DeckMaterializer: PVP_INSERT_RESULT exact=1 pool=%d guid=%d created=%d beforeNext=%d nextId=%d deltaHash=%016llx model=live-pvp",
            after[target].id,expectedGuid,created,initialNext,endNext,
            static_cast<unsigned long long>(fingerprint));
    }
    return;
reject:
    LOG_ERR("CustomComponents/DeckMaterializer: PVP_INSERT_SKIP reason=%s guid=%d requested=%d writes=0",
            reason,expectedGuid,requestedAmount);
}

inline void AfterStockCreateCardInDeck(Obj* model,const InsertionSnapshot& before,
                                      int expectedGuid,int requestedAmount) {
    if (!model || repairingCreatedEntries) return;
    if (before.beforeVirtual<0) return;
    const int after=CountVirtualMainDeckEntries(model);
    if (before.pvpLive)
        MaterializePvpInsertions(model,before,expectedGuid,requestedAmount);
    else if (after>before.beforeVirtual) {
        if (pvpInsertRequested && model==PvpRootedModel()) {
            LOG_ERR("CustomComponents/DeckMaterializer: PVP_INSERT_SKIP reason=pre-stock-snapshot-unavailable writes=0");
            return;
        }
        repairingCreatedEntries=true;
        RepairSameModelDeckEntries(model); // unchanged PvE-only model/factory gate
        repairingCreatedEntries=false;
    }
    else return;
    const int remaining=CountVirtualMainDeckEntries(model);
    LOG_INFO("CustomComponents/DeckMaterializer: INSERT_VIRTUAL before=%d after=%d remaining=%d context=%s",
        before.beforeVirtual,after,remaining,
        before.pvpLive ? "live-pvp" :
        (boundModelRoot && Core::SceneViewContext::getGCHandle &&
         Core::SceneViewContext::getGCHandle(boundModelRoot)==model ? "live-pve" : "readonly-clone"));
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
    Engine::Events::beforeDeckInitialize.Notify({static_cast<Obj*>(self),playerData,playerEntity});
    originalInitializeDeck(self, playerData, playerEntity, method);
    MaterializePlayerDeck(self, playerEntity);
    Engine::Events::afterDeckInitialize.Notify({static_cast<Obj*>(self),playerData,playerEntity});
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
    cardFactoryEntityModel = cardFactory.GetField("<Model>k__BackingField");

    entityComponents = entity.GetField("Components");
    modelEntities = model.GetField("Entities");
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
        !deckFactoryCardEntityFactory.IsValid() || !cardFactoryEntityModel.IsValid() ||
        !entityComponents.IsValid() || !entityId.IsValid() ||
        !modelEntities.IsValid() ||
        !deckAssociationDeckEntityId.IsValid() || !orderedPoolEntries.IsValid() ||
        !poolEntryGuid.IsValid() || !poolEntryEntityId.IsValid() ||
        !modelGetEntityById.IsValid() || !createCardWithDefaultOwner.IsValid() ||
        !initializeDeck.IsValid() || !removeExtraCardsProcess.IsValid()) {
        LOG_ERR("CustomComponents/DeckMaterializer: required API missing");
        return false;
    }

    // PvP inserted-card materialization is optional: missing stock private
    // helper metadata must never prevent the proven PvE/PvP core from loading.
    auto prepop=BNM::Class("PvZCards.Core.Systems","PrepopulateCardPoolSystem");
    auto cardDatabase=BNM::Class("PvZCards.Core.Utilities","CardDatabase");
    auto cardType=BNM::Class("PvZCards.Engine.Components","Card");
    auto ownerType=BNM::Class("PvZCards.Engine.Components","Owner");
    if (prepop.IsValid() && cardDatabase.IsValid() && cardType.IsValid() && ownerType.IsValid()) {
        pvpSystemModel=prepop.GetField("<Model>k__BackingField");
        pvpSystemFactory=prepop.GetField("<CardFactory>k__BackingField");
        pvpFactoryDatabase=cardFactory.GetField("<CardDataBase>k__BackingField");
        modelNextEntityId=model.GetField("NextEntityId");
        cardComponentGuid=cardType.GetField("Guid");
        pvpOwnerId=ownerType.GetField("OwnerId");
        pvpGetElementType=prepop.GetMethod("GetElementType",1).cast<Obj*>();
        pvpCreateCardIfMissing=prepop.GetMethod("CreateCardIfNotAlreadyCreated",2).cast<void>();
        pvpBackingEntityByGuid=cardDatabase.GetMethod("GetBackingEntityByGuid",1).cast<Obj*>();
        pvpInsertApisReady=pvpSystemModel.IsValid() && pvpSystemFactory.IsValid() &&
            pvpFactoryDatabase.IsValid() && modelNextEntityId.IsValid() &&
            cardComponentGuid.IsValid() && pvpOwnerId.IsValid() &&
            pvpGetElementType.IsValid() &&
            pvpCreateCardIfMissing.IsValid() && pvpBackingEntityByGuid.IsValid();
    }
    LOG_INFO("CustomComponents/DeckMaterializer: pvpInsertStockHelperReady=%d (optional; no extra hooks)",pvpInsertApisReady?1:0);
    CustomComponents::Core::Hooks::Install(initializeDeck, InitializeDeckHook, originalInitializeDeck);
    CustomComponents::Core::Hooks::Install(
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

    // Do NOT independently hook CreateCardInDeckSystem.ProcessEffect here.
    // DiscardPileZoneEffects owns it and calls the observer above after stock.
    LOG_INFO("CustomComponents/DeckMaterializer: ready initial=1 insertObserver=shared (PvE repair; rooted-PvP stock-entry repair)");
    return true;
}

} // namespace CustomComponents::Utilities::DeckMaterializer
