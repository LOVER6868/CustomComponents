#include "diagnostics/DiagnosticLogging.hpp"
#include "core/HookRegistry.hpp"
#pragma once
// Opt-in PvP boot-time stock prepopulation prototype.
// This is an *experimental two-client laboratory mode*, NOT a network protocol.
// Both clients must have identical mods/card data/config; otherwise simulations
// can diverge. Default is OFF. Mode=1 is strictly read-only.
//
// Run after stock SetRngSeedData() returns but before InitializeGame calls the
// GamePredictor and GameStateUpdateHandler. This is earlier than the UI picker.
// Only touch the base model owned by that PvpInitResponseExecutable. Do NOT
// reuse a local PvE CardEntityFactory or create cards from target enumeration.

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstdio>
#include <cstring>

#include "core/SceneViewContext.hpp"
#include "zones/DeckMaterializer.hpp"
#include "diagnostics/PvpDeckPrepopulatePlan.hpp"
#include "pick3/PickFromThreeSubmission.hpp"  // existing game-context DI binder
#include "log.h"

namespace CustomComponents::Diagnostics::PvpDeckPrepopulatePrototype {
using Obj = BNM::IL2CPP::Il2CppObject;
template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

// 0: disabled, 1: read-only preflight, 2: tightly gated lab experiment.
inline int mode = 0;
inline bool labConsent = false;
// Additional opt-in, not implied by Mode=2 or the two-client assertion.
inline bool constructStockSystemLab = false;
// Separate gates: construct/inspect effect first, then opt-in to exactly one
// stock call for 1/2-card tests or four calls for complete 88-entry prepopulation.
// Neither option verifies the peer has the same mod or the same model hash.
inline bool constructStockEffectLab = false;
inline bool executeOneCardLab = false;
// Independent Stage C: invoke the SAME stock ProcessEffect once with count=2.
// One-card and two-card execution flags are MUTUALLY EXCLUSIVE. Both default OFF.
inline bool executeTwoCardLab = false;
// Stage E: four stock calls in deterministic 1,2,6,7 pool order to materialize
// ALL 88 virtual main/super entries. Separate explicit consent, default OFF.
// This is a matched-private-two-client lab, NOT a multiplayer handshake.
inline bool executeFullDeckLab = false;
// Only the two-card lab can choose the opposing main deck (pool 6).
// Keep 1 as the legacy default; values other than 1 or 6 disable mutation.
inline int twoCardTargetPoolId = 1;
// Prevent two executions on the same model, not unrelated matches in the same
// app process. Every model also must pass the untouched 88-entry preflight.
inline Obj* mutatingTrialModel = nullptr;
// Fixed hash pins a known lab model; "auto" validates the actual match deck.
// Neither mode verifies the remote client.
inline std::string requiredFingerprint;
inline BNM::Method<Obj*> getBackingEntityByGuid;
inline unsigned observed = 0;
inline BNM::Field<Obj*> execModel, execInstances;
inline BNM::Field<MonoList<Obj*>*> modelEntities;
inline BNM::Field<int> modelNextId;
inline BNM::Field<MonoList<Obj*>*> entityComponents;
// Post-ProcessEffect read-only identity probe: documented fields from this
// exact game's dump.cs, no hook, no writes and no arbitrary pointer offsets.
inline BNM::Field<int> entityObjectId, createdCardGuid, createdOwnerId;
inline BNM::Field<int> createdDeckAssociation, createdHandAssociation;
inline BNM::Field<int> createdHandElement, createdSourceGuid, createdEffectSourceId;
inline BNM::Field<MonoList<Obj*>*> poolEntries;
inline BNM::Field<int> entryGuid, entryId;
inline BNM::Field<Obj*> systemModel, systemFactory, factoryModel;
inline BNM::Field<Obj*> factoryInstances, factoryDatabase, factoryDataProvider;
inline BNM::Field<Obj*> factoryDefaultOwner, factoryAbilityCreator;
inline BNM::Field<int> effectTargetId, effectNumCards;
inline BNM::Field<Obj*> effectData, effectContext;
inline BNM::Method<Obj*> getEntity;
inline BNM::Method<void> processStockEffect, constructEffect, constructSystem;
inline BNM::Class systemClass, effectClass, factoryClass;
using InitFn = void (*)(Obj*, Obj*, const void*);
inline InitFn originalSeed = nullptr;

struct PoolInfo { int id=0, entries=0, virtualCount=0; Obj* entity=nullptr; };
inline constexpr auto& kIds=PvpDeckPrepopulatePlan::PoolIds;
inline constexpr auto& kExpected=PvpDeckPrepopulatePlan::PoolSizes;

inline bool PoolOf(Obj* entity, Obj*& pool) {
    pool=nullptr;
    if (!entity) return false;
    auto* components = entityComponents[entity].Get();
    if (!components || components->size<0 || components->size>256 ||
        (components->size && !components->items)) return false;
    for(int i=0;i<components->size;i++) {
        Obj* c = components->items->m_Items[i];
        if (c && c->klass && c->klass->name &&
            std::string(c->klass->name)=="OrderedCardPool") {
            if (pool) return false; // ambiguous pools, fail closed
            pool=c;
        }
    }
    return pool!=nullptr;
}

// Integer-only, deterministic fingerprint; no addresses, local RNG or map iteration.
inline bool Preflight(Obj* model, PoolInfo (&pools)[4], std::uint64_t& fingerprint) {
    PvpDeckPrepopulatePlan::Builder plan;
    auto* entities = model ? modelEntities[model].Get() : nullptr;
    if (!entities || entities->size!=16 || modelNextId[model].Get()!=16 ||
        !entities->items) return false;
    for(int n=0;n<4;n++) {
        pools[n].id=kIds[n];
        Obj* entity=nullptr;
        auto lookup=BNM::TryInvoke([&]{ entity=getEntity[model](kIds[n]); });
        if (lookup.IsValid() || !entity) return false;
        Obj* pool=nullptr;
        if (!PoolOf(entity,pool)) return false;
        auto* entries=poolEntries[pool].Get();
        if (!entries || entries->size!=kExpected[n] || !entries->items) return false;
        pools[n].entity=entity;
        pools[n].entries=entries->size;
        if (!plan.BeginPool(kIds[n],entries->size)) return false;
        for(int i=0;i<entries->size;i++) {
            Obj* e=entries->items->m_Items[i];
            if (!e) return false;
            const int id=entryId[e].Get();
            const int guid=entryGuid[e].Get();
            // Only untouched virtual pools are eligible; never revive stale IDs.
            if (!plan.AddEntry(guid,id)) return false;
            ++pools[n].virtualCount;
        }
    }
    fingerprint=plan.fingerprint;
    return plan.Complete();
}

// Resolve all distinct GUIDs against the bound match CardDatabase BEFORE the
// first stock mutation. This is required for dynamic decks: it catches unknown
// cards and mismatched main/super or faction definitions without partial writes.
// This does not allocate game entities or change the deck. In case a custom
// card is not registered in this match database yet, fail closed.
inline bool ValidateMatchCardDefinitions(Obj* factory, const PoolInfo (&pools)[4]) {
    if (!factory || !getBackingEntityByGuid.IsValid() ||
        !factoryDatabase.IsValid() || !createdCardGuid.IsValid()) return false;
    Obj* database=factoryDatabase[factory].Get();
    if (!database) return false;
    int inspected[88]{};
    int count=0;
    for (int n=0;n<4;n++) {
        Obj* pool=nullptr;
        if (!PoolOf(pools[n].entity,pool)) return false;
        auto* entries=poolEntries[pool].Get();
        if (!entries || !entries->items || entries->size!=kExpected[n]) return false;
        for (int i=0;i<entries->size;i++) {
            Obj* e=entries->items->m_Items[i];
            if (!e || entryId[e].Get()!=-1) return false;
            const int guid=entryGuid[e].Get();
            if (guid<=0) return false;
            bool duplicate=false;
            for (int j=0;j<count;j++) if (inspected[j]==guid) { duplicate=true;break; }
            if (duplicate) continue;
            if (count>=88) return false;
            inspected[count++]=guid;
            Obj* definition=nullptr;
            auto error=BNM::TryInvoke([&]{ definition=getBackingEntityByGuid[database](guid); });
            if (error.IsValid() || !definition) {
                LOG_ERR("CustomComponents/PVP_PREPOP: DECK_DEF_MISSING pool=%d index=%d guid=%d writes=0",
                    kIds[n],i,guid);
                return false;
            }
            // The stock database may share the same GUID across decks; verify
            // that the card definition's identity is unambiguous and intact.
            auto* comps=entityComponents[definition].Get();
            if (!comps || !comps->items || comps->size<1 || comps->size>256) return false;
            int cards=0;
            for (int c=0;c<comps->size;c++) {
                Obj* component=comps->items->m_Items[c];
                if (component && component->klass && component->klass->name &&
                    component->klass->namespaze &&
                    std::strcmp(component->klass->namespaze,"PvZCards.Engine.Components")==0 &&
                    std::strcmp(component->klass->name,"Card")==0) {
                    if (createdCardGuid[component].Get()!=guid) return false;
                    ++cards;
                }
            }
            if (cards!=1) return false;
        }
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: DECK_DEFINITIONS_OK distinctGuids=%d writes=0",count);
    return true;
}

// Use the existing injected *match* system, not a new system and not a factory
// from PvE/prediction. The DI binder may not exist this early: no-op in that case.
inline Obj* ResolveStockSystem(Obj* model, const char*& reason) {
    using namespace CustomComponents::Pick3;
    Obj* binder=Submission::Target(Submission::binderRoot);
    if (!binder || !Submission::getInjectionBinder.IsValid() ||
        !Submission::getInjectedInstance.IsValid()) { reason="di-not-ready"; return nullptr; }
    Obj* injector=nullptr;
    auto bx=BNM::TryInvoke([&]{ injector=Submission::getInjectionBinder[binder](); });
    if (bx.IsValid() || !injector) { reason="injection-binder-unavailable"; return nullptr; }
    Obj* system=nullptr;
    auto ex=BNM::TryInvoke([&]{
        system=Submission::getInjectedInstance[injector](systemClass.GetMonoType(), static_cast<Obj*>(nullptr));
    });
    if (ex.IsValid() || !system || !BNM::IsA(system,systemClass)) {
        reason="stock-system-not-bound"; return nullptr;
    }
    Obj* attachedModel=systemModel[system].Get();
    Obj* factory=systemFactory[system].Get();
    if (attachedModel!=model || !factory || factoryModel[factory].Get()!=model) {
        reason="factory-model-mismatch"; return nullptr;
    }
    reason="ready";
    return system;
}

// Resolve a factory from the *same* match DI context. Do not allocate or
// mutate a model here. The factory is a registered service; individual effect
// system objects are usually registered with the effect router, not with DI.
// Even a same-model factory is rejected if it belongs to a different
// InstanceManager or lacks any required injected dependencies.
inline Obj* ResolveBoundFactory(Obj* model, Obj* expectedInstances, const char*& reason) {
    using namespace CustomComponents::Pick3;
    Obj* binder=Submission::Target(Submission::binderRoot);
    if (!binder || !Submission::getInjectionBinder.IsValid() ||
        !Submission::getInjectedInstance.IsValid()) {
        reason="factory-binder-not-ready"; return nullptr;
    }
    Obj* injector=nullptr;
    auto bx=BNM::TryInvoke([&]{ injector=Submission::getInjectionBinder[binder](); });
    if (bx.IsValid() || !injector) { reason="factory-binder-unavailable"; return nullptr; }
    Obj* factory=nullptr;
    auto fx=BNM::TryInvoke([&]{
        factory=Submission::getInjectedInstance[injector](factoryClass.GetMonoType(), static_cast<Obj*>(nullptr));
    });
    if (fx.IsValid()) { reason="factory-di-exception"; return nullptr; }
    if (!factory || !BNM::IsA(factory,factoryClass)) { reason="factory-not-bound"; return nullptr; }
    if (factoryModel[factory].Get()!=model) {
        reason="factory-model-mismatch"; return nullptr;
    }
    if (!expectedInstances || factoryInstances[factory].Get()!=expectedInstances) {
        reason="factory-instance-manager-mismatch"; return nullptr;
    }
    if (!factoryDatabase[factory].Get() || !factoryDataProvider[factory].Get() ||
        !factoryDefaultOwner[factory].Get() || !factoryAbilityCreator[factory].Get()) {
        reason="factory-dependencies-incomplete"; return nullptr;
    }
    reason="ready";
    return factory;
}

// FNV-1a with explicit boundaries and fixed-width integers: output contains
// NO raw addresses, GC handles, clocks, locale or unordered collection data.
inline void IdentityMix32(std::uint64_t& h, std::uint32_t word) {
    for(unsigned i=0;i<4;i++) {
        h ^= static_cast<unsigned char>((word>>(i*8))&255u);
        h *= 1099511628211ull;
    }
}
inline void IdentityMixText(std::uint64_t& h, const char* text) {
    if (!text) text="";
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        h^=*p;
        h*=1099511628211ull;
    }
    h^=0u; h*=1099511628211ull; // terminator: no concatenation collision
}

// The inspector is called AFTER the single pre-existing lab ProcessEffect,
// never before it and never on a normal match. In particular, this function
// does not assign fields, allocate Entity objects, or call card factories.
inline void InspectCreatedEntityIdentities(Obj* model, const PoolInfo (&originalPools)[4]) {
    constexpr int kFirstCreated=16;
    constexpr int kLastCreated=17;
    auto* entities=model?modelEntities[model].Get():nullptr;
    if (!entities || !entities->items || entities->size!=18 ||
        modelNextId[model].Get()!=18) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT skip=unexpected-model-shape count=%d nextId=%d readsOnly=1",
            entities?entities->size:-1,model?modelNextId[model].Get():-1);
        return;
    }
    std::uint64_t poolHash=14695981039346656037ull;
    int linkCount[2]{};
    int virtualEntries=0;
    bool poolComplete=true;
    for(int n=0;n<4;n++) {
        Obj* pool=nullptr;
        if (!PoolOf(originalPools[n].entity,pool)) { poolComplete=false; break; }
        auto* entries=poolEntries[pool].Get();
        if (!entries || !entries->items || entries->size!=kExpected[n]) {
            poolComplete=false; break;
        }
        IdentityMix32(poolHash,static_cast<std::uint32_t>(kIds[n]));
        IdentityMix32(poolHash,static_cast<std::uint32_t>(entries->size));
        for(int i=0;i<entries->size;i++) {
            Obj* entry=entries->items->m_Items[i];
            if (!entry) { poolComplete=false; break; }
            const int cardGuid=entryGuid[entry].Get();
            const int eid=entryId[entry].Get();
            IdentityMix32(poolHash,static_cast<std::uint32_t>(cardGuid));
            IdentityMix32(poolHash,static_cast<std::uint32_t>(eid));
            if (eid==-1) ++virtualEntries;
            if (eid>=kFirstCreated && eid<=kLastCreated) {
                ++linkCount[eid-kFirstCreated];
                CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT POOL_LINK pool=%d index=%d guid=%d entityId=%d readsOnly=1",
                    kIds[n],i,cardGuid,eid);
            }
        }
        if (!poolComplete) break;
    }
    if (!poolComplete) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT skip=unreadable-pool readsOnly=1");
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT POOL_SUMMARY hash=%016llx virtual=%d links16=%d links17=%d readsOnly=1",
        static_cast<unsigned long long>(poolHash),virtualEntries,linkCount[0],linkCount[1]);

    std::uint64_t combinedHash=14695981039346656037ull;
    IdentityMix32(combinedHash,static_cast<std::uint32_t>(entities->size));
    IdentityMix32(combinedHash,static_cast<std::uint32_t>(modelNextId[model].Get()));
    IdentityMix32(combinedHash,static_cast<std::uint32_t>(poolHash));
    IdentityMix32(combinedHash,static_cast<std::uint32_t>(poolHash>>32));
    bool complete=true;
    for(int id=kFirstCreated;id<=kLastCreated;id++) {
        Obj* created=nullptr;
        auto failure=BNM::TryInvoke([&]{ created=getEntity[model](id); });
        if (failure.IsValid() || !created) {
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT ENTITY_MISSING requestedId=%d exception=%d readsOnly=1",
                id,failure.IsValid()?1:0);
            complete=false;
            continue;
        }
        auto* components=entityComponents[created].Get();
        if (!components || !components->items || components->size<0 || components->size>128) {
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT ENTITY_UNREADABLE requestedId=%d readsOnly=1",id);
            complete=false;
            continue;
        }
        const int actualId=entityObjectId[created].Get();
        int listIndex=-1;
        for(int i=0;i<entities->size;i++) {
            if (entities->items->m_Items[i]==created) {listIndex=i; break;}
        }
        std::uint64_t entityHash=14695981039346656037ull;
        IdentityMix32(entityHash,static_cast<std::uint32_t>(id));
        IdentityMix32(entityHash,static_cast<std::uint32_t>(actualId));
        IdentityMix32(entityHash,static_cast<std::uint32_t>(listIndex));
        IdentityMix32(entityHash,static_cast<std::uint32_t>(components->size));
        IdentityMix32(entityHash,static_cast<std::uint32_t>(linkCount[id-kFirstCreated]));
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT ENTITY_BEGIN requestedId=%d actualId=%d listIndex=%d componentCount=%d poolLinks=%d readsOnly=1",
            id,actualId,listIndex,components->size,linkCount[id-kFirstCreated]);
        if (actualId!=id || listIndex<0) complete=false;
        for(int c=0;c<components->size;c++) {
            Obj* component=components->items->m_Items[c];
            if (!component || !component->klass || !component->klass->name ||
                !component->klass->namespaze) {
                CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT COMPONENT_UNREADABLE id=%d index=%d readsOnly=1",id,c);
                complete=false;
                continue;
            }
            const char* ns=component->klass->namespaze;
            const char* type=component->klass->name;
            IdentityMixText(entityHash,ns);
            IdentityMixText(entityHash,type);
            // Only semantic integer fields with known layouts are read. Other
            // components still contribute their qualified type to the hash.
            const char* field="none";
            int value=0;
            bool hasValue=false;
            if (std::strcmp(ns,"PvZCards.Engine.Components")==0) {
                if (std::strcmp(type,"Card")==0 && createdCardGuid.IsValid()) {
                    field="Guid"; value=createdCardGuid[component].Get(); hasValue=true;
                } else if (std::strcmp(type,"Owner")==0 && createdOwnerId.IsValid()) {
                    field="OwnerId"; value=createdOwnerId[component].Get(); hasValue=true;
                } else if (std::strcmp(type,"DeckAssociation")==0 && createdDeckAssociation.IsValid()) {
                    field="DeckEntityId"; value=createdDeckAssociation[component].Get(); hasValue=true;
                } else if (std::strcmp(type,"HandAssociation")==0 && createdHandAssociation.IsValid()) {
                    field="HandEntityId"; value=createdHandAssociation[component].Get(); hasValue=true;
                } else if (std::strcmp(type,"HandElement")==0 && createdHandElement.IsValid()) {
                    field="HandEntityId"; value=createdHandElement[component].Get(); hasValue=true;
                } else if (std::strcmp(type,"CreationSource")==0 && createdSourceGuid.IsValid()) {
                    field="SourceGuid"; value=createdSourceGuid[component].Get(); hasValue=true;
                } else if (std::strcmp(type,"EffectEntity")==0 && createdEffectSourceId.IsValid()) {
                    field="SourceId"; value=createdEffectSourceId[component].Get(); hasValue=true;
                }
            }
            IdentityMixText(entityHash,field);
            IdentityMix32(entityHash,static_cast<std::uint32_t>(hasValue?value:0));
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT COMPONENT id=%d index=%d namespace=%s type=%s field=%s value=%d hasValue=%d readsOnly=1",
                id,c,ns,type,field,value,hasValue?1:0);
        }
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT ENTITY_END id=%d hash=%016llx complete=%d readsOnly=1",
            id,static_cast<unsigned long long>(entityHash),complete?1:0);
        IdentityMix32(combinedHash,static_cast<std::uint32_t>(entityHash));
        IdentityMix32(combinedHash,static_cast<std::uint32_t>(entityHash>>32));
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: ENTITY_INSPECT SUMMARY complete=%d poolHash=%016llx combinedHash=%016llx links16=%d links17=%d readsOnly=1",
        complete?1:0,static_cast<unsigned long long>(poolHash),
        static_cast<unsigned long long>(combinedHash),linkCount[0],linkCount[1]);
}


// Read only: deck entity's Owner component, when present. An absent or
// ambiguous owner is reported as unverified; never guess 0 for pool 6.
inline bool PoolOwnerId(Obj* entity, int& id) {
    id=-1;
    if (!entity || !createdOwnerId.IsValid()) return false;
    auto* comps=entityComponents[entity].Get();
    if (!comps || comps->size<0 || comps->size>256 ||
        (comps->size && !comps->items)) return false;
    int found=0;
    for (int i=0;i<comps->size;i++) {
        Obj* c=comps->items->m_Items[i];
        if (c && c->klass && c->klass->name && c->klass->namespaze &&
            std::strcmp(c->klass->namespaze,"PvZCards.Engine.Components")==0 &&
            std::strcmp(c->klass->name,"Owner")==0) {
            ++found;
            id=createdOwnerId[c].Get();
        }
    }
    return found==1 && id>=0;
}

// Stage C / shared Stage B post-effect validator. It inspects the *appended*
// entity range, not assumed card/effect pairs. Supports effects created by any
// card, while requiring each side-effect EffectEntity.SourceId to refer to a
// newly materialized card. Unknown extra objects FAIL validation, never repair.
// All inspected fields are from the same game's metadata; no raw offsets.
struct MaterializeInspection {
    bool complete=false;
    bool exact=false;
    int cards=0;
    int effects=0;
    int unknown=0;
    int orphanEffects=0;
    int cardLinks=0;
    int next=-1;
    std::uint64_t poolHash=0;
    std::uint64_t combinedHash=0;
};
inline MaterializeInspection InspectMaterializedEntities(
    Obj* model, const PoolInfo (&originalPools)[4], int targetPoolIndex,
    int expectedCards, const int (&materializedIds)[2],
    const int (&materializedGuids)[2]) {
    MaterializeInspection out{};
    auto* entities=model?modelEntities[model].Get():nullptr;
    out.next=model?modelNextId[model].Get():-1;
    // Bound worst-case C++ work/logging and do not access an incomplete list.
    if (!entities || !entities->items || entities->size<16+expectedCards ||
        entities->size>40 || out.next!=entities->size ||
        (expectedCards!=1 && expectedCards!=2) ||
        (targetPoolIndex!=0 && targetPoolIndex!=2)) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT SKIP reason=unexpected-model-shape entries=%d nextId=%d readsOnly=1",
            entities?entities->size:-1,out.next);
        return out;
    }
    bool complete=true;
    int expectedOwner=-1;
    const bool ownerKnown=PoolOwnerId(originalPools[targetPoolIndex].entity,expectedOwner);
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT OWNER_BASELINE pool=%d known=%d expectedOwner=%d readsOnly=1",
        kIds[targetPoolIndex],ownerKnown?1:0,expectedOwner);
    out.poolHash=14695981039346656037ull;
    int poolLive=0,poolVirtual=0,poolOther=0;
    for(int n=0;n<4;n++) {
        Obj* pool=nullptr;
        if (!PoolOf(originalPools[n].entity,pool)) {complete=false; break;}
        auto* entries=poolEntries[pool].Get();
        if (!entries || !entries->items || entries->size!=kExpected[n]) {complete=false; break;}
        IdentityMix32(out.poolHash,static_cast<std::uint32_t>(kIds[n]));
        IdentityMix32(out.poolHash,static_cast<std::uint32_t>(entries->size));
        for(int i=0;i<entries->size;i++) {
            Obj* entry=entries->items->m_Items[i];
            if (!entry) {complete=false; break;}
            const int guid=entryGuid[entry].Get(),id=entryId[entry].Get();
            IdentityMix32(out.poolHash,static_cast<std::uint32_t>(guid));
            IdentityMix32(out.poolHash,static_cast<std::uint32_t>(id));
            if (id==-1) {++poolVirtual; continue;}
            if (n==targetPoolIndex && i<expectedCards && id==materializedIds[i] &&
                guid==materializedGuids[i] && id>=16 && id<out.next) {
                ++poolLive;
                ++out.cardLinks;
                CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT POOL_LINK pool=%d index=%d guid=%d entityId=%d readsOnly=1",
                    kIds[n],i,guid,id);
            } else {
                ++poolOther;
                CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT UNEXPECTED_POOL_LINK pool=%d index=%d guid=%d entityId=%d readsOnly=1",
                    kIds[n],i,guid,id);
            }
        }
        if (!complete) break;
    }
    out.combinedHash=14695981039346656037ull;
    IdentityMix32(out.combinedHash,static_cast<std::uint32_t>(out.next));
    IdentityMix32(out.combinedHash,static_cast<std::uint32_t>(out.poolHash));
    IdentityMix32(out.combinedHash,static_cast<std::uint32_t>(out.poolHash>>32));
    int effectLinksPerCard[2]{};
    int observedCardsPerSlot[2]{};
    for(int id=16;id<out.next;id++) {
        Obj* created=nullptr;
        auto error=BNM::TryInvoke([&]{ created=getEntity[model](id); });
        if (error.IsValid() || !created) {
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT ENTITY_MISSING id=%d exception=%d readsOnly=1",
                id,error.IsValid()?1:0);
            complete=false; continue;
        }
        auto* components=entityComponents[created].Get();
        if (!components || !components->items || components->size<0 || components->size>128) {
            complete=false;
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT ENTITY_UNREADABLE id=%d readsOnly=1",id);
            continue;
        }
        const int actualId=entityObjectId[created].Get();
        int listIndex=-1;
        for(int i=0;i<entities->size;i++) {
            if (entities->items->m_Items[i]==created) {listIndex=i;break;}
        }
        if (actualId!=id || listIndex!=id) complete=false;
        std::uint64_t hash=14695981039346656037ull;
        IdentityMix32(hash,static_cast<std::uint32_t>(id));
        IdentityMix32(hash,static_cast<std::uint32_t>(actualId));
        IdentityMix32(hash,static_cast<std::uint32_t>(listIndex));
        IdentityMix32(hash,static_cast<std::uint32_t>(components->size));
        int guid=-1,owner=-1,source=-1;
        int cardComponents=0,effectComponents=0;
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT ENTITY_BEGIN id=%d actualId=%d listIndex=%d components=%d readsOnly=1",
            id,actualId,listIndex,components->size);
        for(int c=0;c<components->size;c++) {
            Obj* component=components->items->m_Items[c];
            if (!component || !component->klass || !component->klass->name ||
                !component->klass->namespaze) {complete=false;continue;}
            const char* ns=component->klass->namespaze;
            const char* type=component->klass->name;
            IdentityMixText(hash,ns);
            IdentityMixText(hash,type);
            const char* field="none";
            int value=0;
            bool hasValue=false;
            if (std::strcmp(ns,"PvZCards.Engine.Components")==0) {
                if (std::strcmp(type,"Card")==0 && createdCardGuid.IsValid()) {
                    field="Guid";value=createdCardGuid[component].Get();hasValue=true;
                    guid=value; ++cardComponents;
                } else if (std::strcmp(type,"Owner")==0 && createdOwnerId.IsValid()) {
                    field="OwnerId";value=createdOwnerId[component].Get();hasValue=true;owner=value;
                } else if (std::strcmp(type,"EffectEntity")==0 && createdEffectSourceId.IsValid()) {
                    field="SourceId";value=createdEffectSourceId[component].Get();hasValue=true;
                    source=value; ++effectComponents;
                } else if (std::strcmp(type,"DeckAssociation")==0 && createdDeckAssociation.IsValid()) {
                    field="DeckEntityId";value=createdDeckAssociation[component].Get();hasValue=true;
                } else if (std::strcmp(type,"HandAssociation")==0 && createdHandAssociation.IsValid()) {
                    field="HandEntityId";value=createdHandAssociation[component].Get();hasValue=true;
                } else if (std::strcmp(type,"HandElement")==0 && createdHandElement.IsValid()) {
                    field="HandEntityId";value=createdHandElement[component].Get();hasValue=true;
                } else if (std::strcmp(type,"CreationSource")==0 && createdSourceGuid.IsValid()) {
                    field="SourceGuid";value=createdSourceGuid[component].Get();hasValue=true;
                }
            }
            IdentityMixText(hash,field);
            IdentityMix32(hash,static_cast<std::uint32_t>(hasValue?value:0));
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT COMPONENT id=%d index=%d namespace=%s type=%s field=%s value=%d hasValue=%d readsOnly=1",
                id,c,ns,type,field,value,hasValue?1:0);
        }
        const char* kind="unknown";
        if (cardComponents==1 && effectComponents==0) {
            kind="card";
            ++out.cards;
            bool linked=false;
            for(int j=0;j<expectedCards;j++) {
                if (id==materializedIds[j] && guid==materializedGuids[j] &&
                    owner>=0 && (!ownerKnown || owner==expectedOwner)) {
                    ++observedCardsPerSlot[j];linked=true;
                }
            }
            if (!linked) complete=false;
        } else if (effectComponents==1 && cardComponents==0) {
            kind="effect";
            ++out.effects;
            bool linked=false;
            for(int j=0;j<expectedCards;j++) {
                if (source==materializedIds[j]) {++effectLinksPerCard[j];linked=true;}
            }
            if (!linked) {++out.orphanEffects;complete=false;}
        } else {
            ++out.unknown; complete=false;
        }
        IdentityMixText(hash,kind);
        IdentityMix32(hash,static_cast<std::uint32_t>(guid));
        IdentityMix32(hash,static_cast<std::uint32_t>(owner));
        IdentityMix32(hash,static_cast<std::uint32_t>(source));
        IdentityMix32(out.combinedHash,static_cast<std::uint32_t>(hash));
        IdentityMix32(out.combinedHash,static_cast<std::uint32_t>(hash>>32));
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT ENTITY_END id=%d kind=%s guid=%d owner=%d source=%d hash=%016llx readsOnly=1",
            id,kind,guid,owner,source,static_cast<unsigned long long>(hash));
    }
    for(int j=0;j<expectedCards;j++) {
        if (observedCardsPerSlot[j]!=1) complete=false;
        // GUID 711 was observed on both phones to have an associated effect.
        // A missing expected effect is not silently accepted as success.
        if (materializedGuids[j]==711 && effectLinksPerCard[j]<1) complete=false;
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT CARD_REL pool=%d slot=%d entityId=%d guid=%d cardFound=%d effectLinks=%d ownerKnown=%d ownerExpected=%d readsOnly=1",
            kIds[targetPoolIndex],j,materializedIds[j],materializedGuids[j],
            observedCardsPerSlot[j],effectLinksPerCard[j],ownerKnown?1:0,expectedOwner);
    }
    out.complete=complete;
    out.exact=complete && poolLive==expectedCards && out.cardLinks==expectedCards &&
        poolVirtual==88-expectedCards && poolOther==0 &&
        out.cards==expectedCards && out.unknown==0 && out.orphanEffects==0 &&
        out.next==16+out.cards+out.effects;
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: MATERIALIZE_INSPECT SUMMARY pool=%d complete=%d exact=%d expectedCards=%d cards=%d effects=%d unknown=%d orphanEffects=%d poolLinks=%d virtual=%d others=%d entities=%d nextId=%d poolHash=%016llx combinedHash=%016llx readsOnly=1",
        kIds[targetPoolIndex],complete?1:0,out.exact?1:0,expectedCards,out.cards,out.effects,out.unknown,
        out.orphanEffects,out.cardLinks,poolVirtual,poolOther,entities->size,out.next,
        static_cast<unsigned long long>(out.poolHash),
        static_cast<unsigned long long>(out.combinedHash));
    return out;
}


// Stage E reads every pool entry before the first call. All GUIDs and positions
// must survive unchanged; separate physical entity IDs are expected for each
// copy (multiple copies of the same GUID are normal deck semantics).
struct FullDeckSnapshot {
    int guids[4][40]{};
    int ids[4][40]{};
    int owners[4]{-1,-1,-1,-1};
    bool ownerKnown[4]{};
};
inline bool SnapshotFullDeck(const PoolInfo (&pools)[4], FullDeckSnapshot& snap) {
    for(int n=0;n<4;n++) {
        Obj* pool=nullptr;
        if(!PoolOf(pools[n].entity,pool)) return false;
        auto* entries=poolEntries[pool].Get();
        if(!entries || !entries->items || entries->size!=kExpected[n]) return false;
        snap.ownerKnown[n]=PoolOwnerId(pools[n].entity,snap.owners[n]);
        for(int j=0;j<entries->size;j++) {
            Obj* entry=entries->items->m_Items[j];
            if(!entry || entryId[entry].Get()!=-1) return false;
            snap.guids[n][j]=entryGuid[entry].Get();
            snap.ids[n][j]=-1;
            if(snap.guids[n][j]<=0) return false;
        }
    }
    return true;
}

struct FullDeckStepResult {
    bool exact=false;
    int cards=0,effects=0,unknown=0,orphan=0;
    int live=0,virtualRemaining=0,entityCount=0,nextId=0;
    std::uint64_t poolHash=0,entityHash=0;
};
// Called only AFTER a genuine stock ProcessEffect; does not mutate the model.
inline FullDeckStepResult VerifyFullDeckStep(Obj* model,
        const PoolInfo (&pools)[4], FullDeckSnapshot& snap,
        int poolIndex, int oldNext) {
    FullDeckStepResult out{};
    auto* entities=model?modelEntities[model].Get():nullptr;
    out.nextId=model?modelNextId[model].Get():-1;
    out.entityCount=entities?entities->size:-1;
    if(!entities || !entities->items || oldNext<16 || out.nextId<=oldNext ||
       out.nextId!=out.entityCount || out.nextId>2048 ||
       poolIndex<0 || poolIndex>3) {
        LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_VERIFY_SKIP pool=%d reason=unexpected-model-shape oldNext=%d next=%d entities=%d",
            poolIndex>=0 && poolIndex<4?kIds[poolIndex]:-1,oldNext,out.nextId,out.entityCount);
        return out;
    }
    bool good=true;
    int cardIds[40]{};
    int ownerObserved=-1;
    int distinctGuids=0;
    out.poolHash=14695981039346656037ull;
    for(int n=0;n<4;n++) {
        Obj* pool=nullptr;
        if(!PoolOf(pools[n].entity,pool)) {good=false;break;}
        auto* entries=poolEntries[pool].Get();
        if(!entries || !entries->items || entries->size!=kExpected[n]) {good=false;break;}
        IdentityMix32(out.poolHash,static_cast<std::uint32_t>(kIds[n]));
        IdentityMix32(out.poolHash,static_cast<std::uint32_t>(entries->size));
        for(int j=0;j<entries->size;j++) {
            Obj* entry=entries->items->m_Items[j];
            if(!entry) {good=false;break;}
            int guid=entryGuid[entry].Get(),id=entryId[entry].Get();
            if(guid!=snap.guids[n][j]) good=false;
            const bool shouldBeLive=(n<=poolIndex);
            if(shouldBeLive) {
                if(id<16 || id>=out.nextId ||
                    (n<poolIndex && id!=snap.ids[n][j]) ||
                    (n==poolIndex && id<oldNext)) good=false;
                ++out.live;
                if(n==poolIndex) cardIds[j]=id;
            } else if(id!=-1) good=false;
            if(id==-1) ++out.virtualRemaining;
            IdentityMix32(out.poolHash,static_cast<std::uint32_t>(guid));
            IdentityMix32(out.poolHash,static_cast<std::uint32_t>(id));
        }
        if(!good) break;
    }
    // Require all new card ID links to be distinct across the 88-card model.
    if(good) {
        for(int i=0;i<kExpected[poolIndex];i++) {
            for(int j=i+1;j<kExpected[poolIndex];j++) {
                if(cardIds[i]==cardIds[j]) good=false;
            }
            for(int n=0;n<poolIndex;n++) for(int j=0;j<kExpected[n];j++)
                if(cardIds[i]==snap.ids[n][j]) good=false;
        }
    }
    // Verify appended card entities, their true GUID+Owner components, and
    // supporting effect SourceId relationships without assuming N effects/card.
    out.entityHash=14695981039346656037ull;
    IdentityMix32(out.entityHash,static_cast<std::uint32_t>(kIds[poolIndex]));
    IdentityMix32(out.entityHash,static_cast<std::uint32_t>(out.nextId));
    int seenCard[40]{};
    for(int id=oldNext;id<out.nextId;id++) {
        Obj* entity=nullptr;
        auto err=BNM::TryInvoke([&]{ entity=getEntity[model](id); });
        if(err.IsValid() || !entity || entities->items->m_Items[id]!=entity ||
           entityObjectId[entity].Get()!=id) {
            good=false; ++out.unknown; continue;
        }
        auto* comps=entityComponents[entity].Get();
        if(!comps || !comps->items || comps->size<1 || comps->size>128) {
            good=false; ++out.unknown; continue;
        }
        int guid=-1,owner=-1,source=-1,cardParts=0,effectParts=0;
        std::uint64_t h=14695981039346656037ull;
        IdentityMix32(h,static_cast<std::uint32_t>(id));
        IdentityMix32(h,static_cast<std::uint32_t>(comps->size));
        for(int c=0;c<comps->size;c++) {
            Obj* comp=comps->items->m_Items[c];
            if(!comp || !comp->klass || !comp->klass->name || !comp->klass->namespaze) {
                good=false; continue;
            }
            const char* ns=comp->klass->namespaze;
            const char* type=comp->klass->name;
            IdentityMixText(h,ns); IdentityMixText(h,type);
            const bool engine=std::strcmp(ns,"PvZCards.Engine.Components")==0;
            const char* field="none"; int value=0;
            if(engine && std::strcmp(type,"Card")==0) {
                guid=createdCardGuid[comp].Get(); ++cardParts;
                field="Guid"; value=guid;
            } else if(engine && std::strcmp(type,"Owner")==0) {
                owner=createdOwnerId[comp].Get(); field="OwnerId"; value=owner;
            } else if(engine && std::strcmp(type,"EffectEntity")==0) {
                source=createdEffectSourceId[comp].Get(); ++effectParts;
                field="SourceId"; value=source;
            } else if(engine && std::strcmp(type,"DeckAssociation")==0 && createdDeckAssociation.IsValid()) {
                field="DeckEntityId"; value=createdDeckAssociation[comp].Get();
            }
            IdentityMixText(h,field); IdentityMix32(h,static_cast<std::uint32_t>(value));
        }
        if(cardParts==1 && effectParts==0) {
            ++out.cards;
            int found=0;
            for(int j=0;j<kExpected[poolIndex];j++) if(cardIds[j]==id && guid==snap.guids[poolIndex][j]) {
                ++found; ++seenCard[j];
            }
            if(found!=1 || owner<0 ||
                (snap.ownerKnown[poolIndex] && owner!=snap.owners[poolIndex]) ||
                (ownerObserved>=0 && owner!=ownerObserved)) good=false;
            if(ownerObserved<0) ownerObserved=owner;
        } else if(cardParts==0 && effectParts==1) {
            ++out.effects;
            bool sourceFound=false;
            for(int j=0;j<kExpected[poolIndex];j++) if(source==cardIds[j]) sourceFound=true;
            if(!sourceFound) {++out.orphan; good=false;}
        } else {++out.unknown;good=false;}
        IdentityMix32(h,static_cast<std::uint32_t>(guid));
        IdentityMix32(h,static_cast<std::uint32_t>(owner));
        IdentityMix32(h,static_cast<std::uint32_t>(source));
        IdentityMix32(out.entityHash,static_cast<std::uint32_t>(h));
        IdentityMix32(out.entityHash,static_cast<std::uint32_t>(h>>32));
    }
    for(int j=0;j<kExpected[poolIndex];j++) {
        if(seenCard[j]!=1) good=false;
        bool first=true;
        for(int earlier=0;earlier<j;earlier++) if(snap.guids[poolIndex][earlier]==snap.guids[poolIndex][j]) first=false;
        if(first) ++distinctGuids;
    }
    // Save verified links for subsequent stock calls, only after complete pass.
    const int expectedLive=kExpected[0]+kExpected[1]+kExpected[2]+kExpected[3]-out.virtualRemaining;
    out.exact=good && out.cards==kExpected[poolIndex] && out.unknown==0 &&
        out.orphan==0 && out.nextId==oldNext+out.cards+out.effects &&
        out.live==expectedLive &&
        out.virtualRemaining==88-expectedLive;
    if(out.exact) for(int j=0;j<kExpected[poolIndex];j++) snap.ids[poolIndex][j]=cardIds[j];
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: FULL_DECK_POOL_RESULT pool=%d exact=%d cards=%d distinctGuids=%d effects=%d unknown=%d orphan=%d owner=%d live=%d virtual=%d nextId=%d entities=%d poolHash=%016llx entityHash=%016llx partialStatePossible=%d",
        kIds[poolIndex],out.exact?1:0,out.cards,distinctGuids,out.effects,
        out.unknown,out.orphan,ownerObserved,out.live,out.virtualRemaining,
        out.nextId,out.entityCount,
        static_cast<unsigned long long>(out.poolHash),
        static_cast<unsigned long long>(out.entityHash),out.exact?0:1);
    return out;
}

// One stock call per pool. No manual entity creation, no rewriting GUIDs, no
// Pick3 hooks, no synchronization layer; fail-closed after any partial failure.
inline void ExecuteFullDeckMaterialization(Obj* model,const PoolInfo (&pools)[4],
        Obj* system, Obj* firstEffect, std::uint64_t preflightHash) {
    FullDeckSnapshot snap{};
    if(!SnapshotFullDeck(pools,snap)) {
        LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_SKIP reason=snapshot-failed writes=0"); return;
    }
    // Claim this model before entering the stock code; do not retry failures.
    mutatingTrialModel=model;
    std::uint64_t combined=14695981039346656037ull;
    IdentityMix32(combined,static_cast<std::uint32_t>(preflightHash));
    IdentityMix32(combined,static_cast<std::uint32_t>(preflightHash>>32));
    int cards=0,effects=0,completed=0;
    bool success=true;
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: FULL_DECK_BEGIN pools=4 totalCards=88 order=1,2,6,7 hash=%016llx privateLab=1",
        static_cast<unsigned long long>(preflightHash));
    for(int n=0;n<4;n++) {
        Obj* effect=n==0?firstEffect:effectClass.CreateNewInstance();
        if(!effect) {
            LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_ABORT pool=%d reason=effect-allocation-failed partialStatePossible=%d",
                kIds[n],completed>0?1:0);
            success=false;break;
        }
        const auto root=n==0?0:Core::SceneViewContext::newGCHandle(effect,false);
        if(n>0 && !root) {
            LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_ABORT pool=%d reason=effect-root-failed partialStatePossible=1",kIds[n]);
            success=false;break;
        }
        bool ready=true;
        if(n>0) {
            auto err=BNM::TryInvoke([&]{constructEffect[effect]();});
            ready=!err.IsValid() && effectData[effect].Get() && effectContext[effect].Get();
            if(!ready) LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_ABORT pool=%d reason=effect-ctor-failed partialStatePossible=1",kIds[n]);
        }
        if(ready) {
            effectTargetId[effect]=kIds[n];
            effectNumCards[effect]=kExpected[n];
            ready=effectTargetId[effect].Get()==kIds[n] &&
                  effectNumCards[effect].Get()==kExpected[n];
            if(!ready) LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_ABORT pool=%d reason=effect-fields-failed partialStatePossible=%d",
                kIds[n],completed>0?1:0);
        }
        if(ready) {
            const int startNext=modelNextId[model].Get();
            auto* entries=modelEntities[model].Get();
            ready=entries && entries->items && entries->size==startNext &&
                  startNext>=16 && startNext<2048;
            if(!ready) LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_ABORT pool=%d reason=precall-shape-mismatch partialStatePossible=%d",
                kIds[n],completed>0?1:0);
            if(ready) {
                CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: FULL_DECK_POOL_BEGIN pool=%d count=%d oldNext=%d",
                    kIds[n],kExpected[n],startNext);
                auto err=BNM::TryInvoke([&]{processStockEffect[system](effect);});
                if(err.IsValid()) {
                    LOG_ERR("CustomComponents/PVP_PREPOP: FULL_DECK_EXCEPTION pool=%d class=%s partialStatePossible=1",
                        kIds[n],err.ClassName().c_str());
                    ready=false;
                } else {
                    auto result=VerifyFullDeckStep(model,pools,snap,n,startNext);
                    ready=result.exact;
                    if(ready) {
                        cards+=result.cards;
                        effects+=result.effects;
                        ++completed;
                        IdentityMix32(combined,static_cast<std::uint32_t>(result.poolHash));
                        IdentityMix32(combined,static_cast<std::uint32_t>(result.poolHash>>32));
                        IdentityMix32(combined,static_cast<std::uint32_t>(result.entityHash));
                        IdentityMix32(combined,static_cast<std::uint32_t>(result.entityHash>>32));
                    }
                }
            }
        }
        if(root) Core::SceneViewContext::freeGCHandle(root);
        if(!ready) {success=false;break;}
    }
    const auto* entities=modelEntities[model].Get();
    const int next=modelNextId[model].Get();
    const bool exact=success && completed==4 && cards==88 &&
        entities && entities->size==next;
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: FULL_DECK_RESULT exact=%d poolsCompleted=%d cards=%d effects=%d live=%d virtual=%d nextId=%d entities=%d combinedHash=%016llx partialStatePossible=%d",
        exact?1:0,completed,cards,effects,exact?88:-1,exact?0:-1,next,
        entities?entities->size:-1,static_cast<unsigned long long>(combined),exact?0:1);
    if (exact && Utilities::DeckMaterializer::pvpInsertRequested &&
        !Utilities::DeckMaterializer::BindPvpSessionAfterVerifiedFullDeck(model,system))
        LOG_ERR("CustomComponents/PVP_PREPOP: PVP_INSERT_DISABLED reason=session-bind-failed (initial-deck-exact=1)");
}

static void SeedHook(Obj* self, Obj* response, const void* method) {
    // Never suppress stock RNG setup, and never change the game when disabled.
    originalSeed(self,response,method);
    if (!self || mode==0) return;
    ++observed;
    Obj* model=execModel[self].Get();
    if (Utilities::DeckMaterializer::PvpRootedModel() &&
        Utilities::DeckMaterializer::PvpRootedModel()!=model)
        Utilities::DeckMaterializer::ClearPvpSession();
    PoolInfo pools[4]{};
    std::uint64_t hash=0;
    const bool eligible=Preflight(model,pools,hash);
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: seq=%u mode=%d model=%p eligible=%d hash=%016llx nextId=%d expected=40/4/40/4",
        observed,mode,model,eligible?1:0,static_cast<unsigned long long>(hash),
        model?modelNextId[model].Get():-1);
    if (!eligible) { CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=initial-model-shape-mismatch writes=0"); return; }
    if (mode==1) { CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: PREFLIGHT_OK virtual=88 writes=0"); return; }
    if (!labConsent) { CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=lab-consent-disabled writes=0"); return; }
    // Avoid further GC allocations on later matches once one stock attempt ran.
    if (mutatingTrialModel==model && (executeOneCardLab || executeTwoCardLab || executeFullDeckLab)) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=trial-already-attempted-this-model writes=0");
        return;
    }

    const char* reason="unresolved";
    Obj* system=ResolveStockSystem(model,reason);
    const bool boundSystem = system != nullptr;
    // Direct system DI lookup may fail because an EffectSystem is registered
    // with EffectRouter rather than bound as a standalone DI service. Only on
    // that precise miss do we examine the registered card factory.
    Obj* factory=nullptr;
    if (!system && std::string(reason)=="stock-system-not-bound") {
        const char* factoryReason="unresolved";
        factory=ResolveBoundFactory(model,execInstances[self].Get(),factoryReason);
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: FACTORY_PROBE result=%s modelMatch=%d instanceMatch=%d writes=0",
            factoryReason,factory?1:0,factory?1:0);
        if (!factory) {
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=%s writes=0",factoryReason);
            return;
        }
        if (!constructStockSystemLab) {
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=factory-ready-construct-disabled writes=0");
            return;
        }
    } else if (!system) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=%s writes=0",reason);
        return;
    }
    if (!Core::SceneViewContext::newGCHandle || !Core::SceneViewContext::freeGCHandle) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=gc-roots-unavailable writes=0"); return;
    }
    // The stock PrepopulateCardPoolSystem::.ctor is a no-op on this game's
    // binary and calling it via BNM previously crashed two matches. Never
    // call it. Only allocate the system and assign its two injected fields.
    // No deck mutation can occur until the independent effect-execution gate.
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: CRASH_PROBE stage=factory-root-enter writes=0");
    // Root the DI factory before any managed allocations for the fallback.
    const auto factoryRoot=factory?Core::SceneViewContext::newGCHandle(factory,false):0;
    if (factory && !factoryRoot) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=factory-gc-root-failed writes=0"); return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: CRASH_PROBE stage=factory-root-ok handle=%u writes=0",
        static_cast<unsigned>(factoryRoot));
    if (!system) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: CRASH_PROBE stage=system-allocate-enter writes=0");
        system=systemClass.CreateNewInstance();
        if (!system) {
            Core::SceneViewContext::freeGCHandle(factoryRoot);
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=system-allocation-failed writes=0");
            return;
        }
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: CRASH_PROBE stage=system-allocate-ok system=%p writes=0",system);
    const auto systemRoot=Core::SceneViewContext::newGCHandle(system,false);
    if (!systemRoot) {
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=system-gc-root-failed writes=0");
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: CRASH_PROBE stage=system-root-ok handle=%u writes=0",
        static_cast<unsigned>(systemRoot));
    if (!boundSystem) {
        // Skip constructSystem[system]() intentionally. Verified no-op by
        // disassembly; the previous TryInvoke call preceded both native crashes.
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: CRASH_PROBE stage=ctor-skipped nativeNoop=1 writes=0");
        // The two injected fields are the complete dependency surface of
        // PrepopulateCardPoolSystem in this exact game's dump.cs.
        systemModel[system]=model;
        systemFactory[system]=factory;
        if (systemModel[system].Get()!=model || systemFactory[system].Get()!=factory ||
            factoryModel[factory].Get()!=model ||
            factoryInstances[factory].Get()!=execInstances[self].Get()) {
            Core::SceneViewContext::freeGCHandle(systemRoot);
            Core::SceneViewContext::freeGCHandle(factoryRoot);
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=constructed-system-validation-failed writes=0");
            return;
        }
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: CRASH_PROBE stage=dependency-fields-validated modelMatched=1 instancesMatched=1 writes=0");
    }
    // Stage A: isolated creation of a genuine stock effect. Effect::.ctor is
    // NOT a no-op: it creates EffectData/EffectContext and sets default IDs.
    // The previous crash therefore does not justify skipping this ctor.
    if (!constructStockEffectLab) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=stock-effect-construction-disabled writes=0");
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: EFFECT_PROBE stage=allocate-enter writes=0");
    Obj* effect=effectClass.CreateNewInstance();
    if (!effect) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=effect-allocation-failed writes=0");
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: EFFECT_PROBE stage=allocate-ok effect=%p writes=0",effect);
    const auto effectRoot=Core::SceneViewContext::newGCHandle(effect,false);
    if (!effectRoot) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=effect-gc-root-failed writes=0");
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: EFFECT_PROBE stage=ctor-enter writes=0");
    auto ctorError=BNM::TryInvoke([&]{ constructEffect[effect](); });
    if (ctorError.IsValid()) {
        LOG_ERR("CustomComponents/PVP_PREPOP: SKIP reason=effect-ctor-exception class=%s writes=0",
            ctorError.ClassName().c_str());
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    const bool dataReady=effectData[effect].Get()!=nullptr;
    const bool contextReady=effectContext[effect].Get()!=nullptr;
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: EFFECT_PROBE stage=ctor-ok dataReady=%d contextReady=%d writes=0",
        dataReady?1:0,contextReady?1:0);
    if (!dataReady || !contextReady) {
        LOG_ERR("CustomComponents/PVP_PREPOP: SKIP reason=effect-construction-incomplete writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }

    // One-card lab is unchanged: pool 1. Two-card lab can target pool 6
    // only by an additional explicit opt-in, not a picker/runtime rewrite.
    // Full four-pool initial model fingerprint is always computed.
    const int kTestCount=executeFullDeckLab?40:(executeTwoCardLab?2:1);
    const int kTestPool=executeTwoCardLab?twoCardTargetPoolId:1;
    const int kTestPoolIndex=kTestPool==6?2:0;
    if (kTestPool!=1 && kTestPool!=6) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=invalid-target-pool target=%d writes=0",kTestPool);
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    effectTargetId[effect]=kTestPool;
    effectNumCards[effect]=kTestCount;
    if (effectTargetId[effect].Get()!=kTestPool ||
        effectNumCards[effect].Get()!=kTestCount) {
        LOG_ERR("CustomComponents/PVP_PREPOP: SKIP reason=effect-field-validation-failed writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: EFFECT_PROBE stage=ready target=%d count=%d fingerprint=%016llx writes=0",
        kTestPool,kTestCount,static_cast<unsigned long long>(hash));
    if (!executeOneCardLab && !executeTwoCardLab && !executeFullDeckLab) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=card-execution-disabled writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }

    // Stage 2A: strict model shape and an optional static pin. In "auto" mode,
    // resolve every unique card definition through this match's CardDatabase;
    // in pinned mode preserve the earlier exact-hash laboratory behavior.
    // Neither mode negotiates compatibility with the remote private-server peer.
    char hashText[17]{};
    std::snprintf(hashText,sizeof(hashText),"%016llx",static_cast<unsigned long long>(hash));
    if (requiredFingerprint.empty()) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=deck-validation-mode-not-configured writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    if (!PvpDeckPrepopulatePlan::AcceptFingerprint(requiredFingerprint,hash)) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=expected-fingerprint-mismatch actual=%s writes=0",hashText);
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    // No rollback is possible if stock code partially mutates the model.
    // Preflight again immediately before invoking stock code, fail closed if
    // match state changed during allocation or effect initialization.
    PoolInfo secondPools[4]{};
    std::uint64_t secondHash=0;
    if (!Preflight(model,secondPools,secondHash) || secondHash!=hash ||
        systemModel[system].Get()!=model ||
        !systemFactory[system].Get() ||
        factoryModel[systemFactory[system].Get()].Get()!=model ||
        (factory && factoryInstances[factory].Get()!=execInstances[self].Get())) {
        LOG_ERR("CustomComponents/PVP_PREPOP: SKIP reason=pre-execution-revalidation-failed writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    // The historic pinned-hash test is retained as an exact rollback mode.
    // Dynamic mode must resolve the definitions through the bound factory
    // before any stock call. This is intentionally stricter than GUID>0.
    if (requiredFingerprint=="auto" &&
        !ValidateMatchCardDefinitions(systemFactory[system].Get(),secondPools)) {
        LOG_ERR("CustomComponents/PVP_PREPOP: SKIP reason=dynamic-definition-validation-failed writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: DECK_AUTHORIZATION mode=%s hash=%s writes=0",
        requiredFingerprint=="auto"?"auto":"pinned",hashText);
    if (mutatingTrialModel==model) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=trial-already-attempted-this-model writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    if (executeFullDeckLab) {
        ExecuteFullDeckMaterialization(model,secondPools,system,effect,hash);
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    // Snapshot the two original GUIDs before stock mutation. The post-call
    // validator must not accept a card whose pool GUID was rewritten.
    int expectedGuids[2]{-1,-1};
    Obj* targetPool=nullptr;
    if (!PoolOf(secondPools[kTestPoolIndex].entity,targetPool)) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=target-pool-disappeared writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    auto* beforeEntries=poolEntries[targetPool].Get();
    if (!beforeEntries || !beforeEntries->items ||
        beforeEntries->size!=kExpected[kTestPoolIndex]) {
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=target-pool-entries-unavailable writes=0");
        Core::SceneViewContext::freeGCHandle(effectRoot);
        Core::SceneViewContext::freeGCHandle(systemRoot);
        if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
        return;
    }
    int deckOwner=-1;
    const bool deckOwnerKnown=PoolOwnerId(secondPools[kTestPoolIndex].entity,deckOwner);
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: TARGET_BASELINE pool=%d ownerKnown=%d owner=%d entries=%d count=%d writes=0",
        kTestPool,deckOwnerKnown?1:0,deckOwner,beforeEntries->size,kTestCount);
    for(int i=0;i<kTestCount;i++) {
        Obj* beforeEntry=beforeEntries->items->m_Items[i];
        if (!beforeEntry || entryId[beforeEntry].Get()!=-1) {
            CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: SKIP reason=target-entry-not-virtual writes=0");
            Core::SceneViewContext::freeGCHandle(effectRoot);
            Core::SceneViewContext::freeGCHandle(systemRoot);
            if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
            return;
        }
        expectedGuids[i]=entryGuid[beforeEntry].Get();
        CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: TARGET_ENTRY pool=%d slot=%d guid=%d virtual=1 writes=0",
            kTestPool,i,expectedGuids[i]);
    }
    // Claim before invoking stock code; if it partially fails, no retry.
    mutatingTrialModel=model;
    const char* trial=kTestCount==2?"TWO_CARD":"ONE_CARD";
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: %s_BEGIN pool=%d count=%d nextId=16 initialHash=%016llx privateLab=1",
        trial,kTestPool,kTestCount,static_cast<unsigned long long>(hash));
    auto processError=BNM::TryInvoke([&]{ processStockEffect[system](effect); });
    if (processError.IsValid()) {
        LOG_ERR("CustomComponents/PVP_PREPOP: %s_EXCEPTION class=%s partialStatePossible=1",
            trial,processError.ClassName().c_str());
    }
    // Read-back is deliberately non-repairing: unexpected writes or entity
    // IDs are evidence of a failed trial, not permission to patch the model.
    int live=0,virtualRemaining=0,otherNonVirtual=0;
    int materializedIds[2]{-1,-1}, materializedGuids[2]{-1,-1};
    bool unreadable=false;
    for (int n=0;n<4;n++) {
        Obj* pool=nullptr;
        if (!PoolOf(pools[n].entity,pool)) { unreadable=true; break; }
        auto* entries=poolEntries[pool].Get();
        if (!entries || !entries->items || entries->size!=kExpected[n]) {
            unreadable=true; break;
        }
        for (int i=0;i<entries->size;i++) {
            Obj* entry=entries->items->m_Items[i];
            if (!entry) { unreadable=true; break; }
            const int id=entryId[entry].Get();
            if (n==kTestPoolIndex && i<kTestCount) {
                materializedIds[i]=id;
                materializedGuids[i]=entryGuid[entry].Get();
            }
            if (id==-1) ++virtualRemaining;
            else if (id>0) {
                ++live;
                if (n!=kTestPoolIndex || i>=kTestCount) ++otherNonVirtual;
            } else ++otherNonVirtual;
        }
        if (unreadable) break;
    }
    auto* entities=modelEntities[model].Get();
    const int count=entities?entities->size:-1;
    const int next=modelNextId[model].Get();
    MaterializeInspection inspection{};
    if (!processError.IsValid() && !unreadable)
        inspection=InspectMaterializedEntities(model,pools,kTestPoolIndex,kTestCount,
            materializedIds,materializedGuids);
    bool preservedGuids=true;
    for(int i=0;i<kTestCount;i++)
        if (materializedGuids[i]!=expectedGuids[i]) preservedGuids=false;
    const bool exact=!processError.IsValid() && !unreadable && inspection.exact &&
        preservedGuids && live==kTestCount && virtualRemaining==88-kTestCount &&
        otherNonVirtual==0;
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: %s_RESULT pool=%d exact=%d live=%d virtual=%d others=%d firstId=%d secondId=%d nextId=%d entities=%d exception=%d cardEntities=%d effectEntities=%d inspectionComplete=%d partialStatePossible=%d",
        trial,kTestPool,exact?1:0,live,virtualRemaining,otherNonVirtual,
        materializedIds[0],materializedIds[1],next,count,
        processError.IsValid()?1:0,inspection.cards,inspection.effects,
        inspection.complete?1:0,exact?0:1);
    // Keep original single-card identity trace, so prior matched fingerprints
    // remain directly comparable with the new generalized validator.
    if (kTestPool==1 && kTestCount==1 && !processError.IsValid())
        InspectCreatedEntityIdentities(model,pools);
    Core::SceneViewContext::freeGCHandle(effectRoot);
    Core::SceneViewContext::freeGCHandle(systemRoot);
    if (factoryRoot) Core::SceneViewContext::freeGCHandle(factoryRoot);
}

inline bool Install(int requestedMode, bool allowLab, bool allowConstruction,
                    bool allowEffectConstruction, bool allowOneCardExecution,
                    bool allowTwoCardExecution, int requestedTwoCardTargetPoolId,
                    bool allowFullDeckExecution,
                    std::string_view configuredFingerprint,
                    bool allowPvpInsertedCards = false) {
    mode=(requestedMode>=0 && requestedMode<=2)?requestedMode:0;
    labConsent=allowLab;
    constructStockSystemLab=allowConstruction;
    constructStockEffectLab=allowEffectConstruction;
    executeOneCardLab=allowOneCardExecution;
    executeTwoCardLab=allowTwoCardExecution;
    executeFullDeckLab=allowFullDeckExecution;
    // Independent, private-server opt-in; startup and initial prepopulation
    // never depend on the optional private helper metadata being available.
    Utilities::DeckMaterializer::pvpInsertRequested=allowPvpInsertedCards;
    twoCardTargetPoolId=requestedTwoCardTargetPoolId;
    const bool invalidTarget=executeTwoCardLab &&
        twoCardTargetPoolId!=1 && twoCardTargetPoolId!=6;
    const bool incompatibleLabs=(executeOneCardLab?1:0)+(executeTwoCardLab?1:0)+
        (executeFullDeckLab?1:0)>1;
    // Invalid target or conflicting modes must not fall back to one-card.
    if (invalidTarget || incompatibleLabs) {
        LOG_ERR("CustomComponents/PVP_PREPOP: invalid lab configuration conflict=%d targetInvalid=%d target=%d; disabling mutation",
            incompatibleLabs?1:0,invalidTarget?1:0,twoCardTargetPoolId);
        executeOneCardLab=false;
        executeTwoCardLab=false;
        executeFullDeckLab=false;
    }
    requiredFingerprint=configuredFingerprint;
    if (requiredFingerprint=="AUTO" || requiredFingerprint=="Auto") requiredFingerprint="auto";
    // One-card mutation requires EVERY independent operator switch. A
    // half-configured or stale config can only run the read-only stages.
    if (!(mode==2 && labConsent && constructStockSystemLab && constructStockEffectLab))
        executeOneCardLab=executeTwoCardLab=executeFullDeckLab=false;
    if (!(mode==2 && labConsent && constructStockSystemLab &&
          constructStockEffectLab && executeFullDeckLab))
        Utilities::DeckMaterializer::pvpInsertRequested=false;
    if (!mode) return true;
    BNM::Class exe("PvZCards.Multiplayer.Networking","PvpInitResponseExecutable");
    BNM::Class model("PvZCards.Engine","EntityModel");
    BNM::Class entity("PvZCards.Engine","Entity");
    BNM::Class pool("PvZCards.Engine.Components","OrderedCardPool");
    BNM::Class entry("PvZCards.Engine.Components.Data","CardPoolEntry");
    BNM::Class factory("PvZCards.Core.Utilities","CardEntityFactory");
    systemClass=BNM::Class("PvZCards.Core.Systems","PrepopulateCardPoolSystem");
    factoryClass=factory;
    effectClass=BNM::Class("PvZCards.Engine.Effects","PrepopulateCardPoolEffect");
    BNM::Class baseEffect("PvZCards.Engine","Effect");
    BNM::Class databaseClass("PvZCards.Core.Utilities","CardDatabase");
    if (!exe.IsValid() || !model.IsValid() || !entity.IsValid() || !pool.IsValid() ||
        !entry.IsValid() || !factory.IsValid() || !systemClass.IsValid() ||
        !effectClass.IsValid() || !baseEffect.IsValid()) return false;
    execModel=exe.GetField("<EntityModel>k__BackingField");
    execInstances=exe.GetField("<Instances>k__BackingField");
    modelEntities=model.GetField("Entities");
    modelNextId=model.GetField("NextEntityId");
    entityComponents=entity.GetField("Components");
    entityObjectId=entity.GetField("<Id>k__BackingField");
    createdCardGuid=BNM::Class("PvZCards.Engine.Components","Card").GetField("Guid");
    createdOwnerId=BNM::Class("PvZCards.Engine.Components","Owner").GetField("OwnerId");
    createdDeckAssociation=BNM::Class("PvZCards.Engine.Components","DeckAssociation").GetField("DeckEntityId");
    createdHandAssociation=BNM::Class("PvZCards.Engine.Components","HandAssociation").GetField("HandEntityId");
    createdHandElement=BNM::Class("PvZCards.Engine.Components","HandElement").GetField("HandEntityId");
    createdSourceGuid=BNM::Class("PvZCards.Engine.Components","CreationSource").GetField("SourceGuid");
    createdEffectSourceId=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId");
    poolEntries=pool.GetField("Entries");
    entryGuid=entry.GetField("Guid");
    entryId=entry.GetField("EntityId");
    getEntity=model.GetMethod("GetEntityById",1).cast<Obj*>();
    systemModel=systemClass.GetField("<Model>k__BackingField");
    systemFactory=systemClass.GetField("<CardFactory>k__BackingField");
    factoryModel=factory.GetField("<Model>k__BackingField");
    factoryInstances=factory.GetField("<Instances>k__BackingField");
    factoryDatabase=factory.GetField("<CardDataBase>k__BackingField");
    getBackingEntityByGuid=databaseClass.GetMethod("GetBackingEntityByGuid",1).cast<Obj*>();
    factoryDataProvider=factory.GetField("<CardDataProvider>k__BackingField");
    factoryDefaultOwner=factory.GetField("<DefaultOwnerAssigner>k__BackingField");
    factoryAbilityCreator=factory.GetField("<AbilityCreator>k__BackingField");
    effectData=baseEffect.GetField("Data");
    effectContext=baseEffect.GetField("Context");
    effectTargetId=baseEffect.GetField("TargetId");
    effectNumCards=effectClass.GetField("NumCardsToCreate");
    processStockEffect=systemClass.GetMethod("ProcessEffect",1).cast<void>();
    constructEffect=effectClass.GetMethod(".ctor",0).cast<void>();
    constructSystem=systemClass.GetMethod(".ctor",0).cast<void>();
    auto seed=exe.GetMethod("SetRngSeedData",1);
    if (!execModel.IsValid() || !execInstances.IsValid() || !modelEntities.IsValid() || !modelNextId.IsValid() ||
        !entityComponents.IsValid() || !poolEntries.IsValid() || !entryGuid.IsValid() ||
        !entryId.IsValid() || !getEntity.IsValid() || !seed.IsValid() ||
        (mode==2 && !entityObjectId.IsValid()) ||
        (executeFullDeckLab && (!createdCardGuid.IsValid() || !createdOwnerId.IsValid() ||
                                (requiredFingerprint=="auto" && !getBackingEntityByGuid.IsValid()) ||
                                !createdEffectSourceId.IsValid())) ||
        (mode==2 && (!systemModel.IsValid() || !systemFactory.IsValid() ||
                     !factoryModel.IsValid() || !effectData.IsValid() ||
                     !effectContext.IsValid() || !effectTargetId.IsValid() ||
                     !effectNumCards.IsValid() || !processStockEffect.IsValid() ||
                     !constructEffect.IsValid() || !constructSystem.IsValid() ||
                     !factoryInstances.IsValid() || !factoryDatabase.IsValid() ||
                     !factoryDataProvider.IsValid() || !factoryDefaultOwner.IsValid() ||
                     !factoryAbilityCreator.IsValid()))) return false;
    CustomComponents::Core::Hooks::Install(seed,SeedHook,originalSeed);
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: install=%d mode=%d labConsent=%d constructStockSystemLab=%d stockOnly=1",
        originalSeed?1:0,mode,labConsent?1:0,constructStockSystemLab?1:0);
    CC_SELECTION_DIAG("CustomComponents/PVP_PREPOP: labGates effectConstruct=%d oneCardExecute=%d twoCardExecute=%d twoCardTargetPool=%d fullDeckExecute=%d conflict=%d (peerCompatibilityNotVerified=1)",
        constructStockEffectLab?1:0,executeOneCardLab?1:0,
        executeTwoCardLab?1:0,twoCardTargetPoolId,executeFullDeckLab?1:0,incompatibleLabs?1:0);
    return originalSeed!=nullptr;
}
} // namespace CustomComponents::Diagnostics::PvpDeckPrepopulatePrototype
