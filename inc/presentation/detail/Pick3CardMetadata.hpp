#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Read-only card preview metadata, sourced from the SAME CardDataMapper used
// by the game's CardAssetsTask. No entity allocation, model mutation, or
// binder.GetInstance calls. Localization is optional and may appear later.
#include <cstdio>
#include <unordered_map>
#include <mutex>
#include <string>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include "presentation/detail/Pick3SpriteCache.hpp"
#include "log.h"

namespace CustomComponents::Presentation::Pick3CardMetadata {
    using Obj = BNM::IL2CPP::Il2CppObject;
    using Str = BNM::Structures::Mono::String;
    using ObjectArray = BNM::Structures::Mono::Array<Obj*>;

    struct CardInfo {
        std::string name, cost, stats, description;
        bool fighter = false;
        bool environment = false;
        bool zombie = false;
        int cardColor = 0;
        bool hasCardData = false;
        bool localizedName = false;
        bool localizedDescription = false;
        // Original stock decider outputs from CardData.SpecialAbilities.
        // -1 means use the normal icon, never guess from description text.
        int attackIconType = 0;
        int healthIconType = 1;
        bool iconDecidersUsed = false;
    };

    inline BNM::Class mapperClass, dataClass, textConstantsClass;
    inline BNM::Class localizedMediatorClass, localizationClass;
    inline BNM::Method<Obj*> getCardData;
    inline BNM::Field<int> displayAttack, displayHealth, displayCost;
    inline BNM::Field<bool> isFighter, isEnvironment;
    inline BNM::Field<int> faction, cardColorField;
    inline BNM::Field<Str*> prefabName, baseId;
    inline BNM::Field<Obj*> specialAbilities;
    inline BNM::Method<int> attackIconDecider, healthIconDecider;
    inline bool useTraitIcons = true;
    inline BNM::Field<Obj*> localizationInMediator;
    inline BNM::Method<ObjectArray*> findAll;
    inline BNM::Method<Str*> nameKey, descriptionKey, translate;
    inline BNM::Method<bool> hasKey;
    inline bool dataReady = false;
    inline bool localizationReady = false;
    inline unsigned int warnedMissingProvider = 0;
    inline bool TryTranslate(Obj*,const std::string&,bool,std::string&);
    struct Snapshot {
        std::string prefab, base;
        int cost=0, attack=0, health=0, faction=0, color=0;
        bool fighter=false, environment=false;
    };
    inline std::unordered_map<int,Snapshot> snapshots;
    inline std::mutex snapshotMutex;
    inline unsigned int snapshotCount=0;
    // Passive capture of game-driven card_data initialization. The game calls
    // CardDataMapper.SetCardData while populating the *active* bundle. No
    // AssetBundle.LoadAsset and no GetCardData invocation is made here.
    using SetCardDataFn = void (*)(Obj*, int, Obj*, const void*);
    inline SetCardDataFn originalSetCardData = nullptr;
    inline bool mapperCaptureActive = false;
    inline unsigned int mapperCaptured = 0;
    inline void CaptureSnapshot(int guid, Obj* data);
    inline void SetCardDataCapture(Obj* self, int guid, Obj* data, const void* method) {
        if (!originalSetCardData) return;
        // Preserve game behavior before performing any optional observations.
        originalSetCardData(self, guid, data, method);
        if (mapperCaptureActive && self && guid>0 && data &&
            mapperClass.IsValid() && BNM::IsA(self,mapperClass) &&
            dataClass.IsValid() && BNM::IsA(data,dataClass)) {
            CaptureSnapshot(guid,data);
            ++mapperCaptured;
        }
    }
    inline bool InstallMapperCapture() {
        if (originalSetCardData) return true;
        if (!mapperClass.IsValid() || !dataClass.IsValid()) return false;
        const auto method=mapperClass.GetMethod("SetCardData",2);
        const auto* info=method.IsValid()?method.GetInfo():nullptr;
        if (!info || info->parameters_count!=2 || !info->methodPointer) return false;
        BNM::BasicHook(method,SetCardDataCapture,originalSetCardData);
        return originalSetCardData!=nullptr;
    }

    // Capture only scalar values / copied strings, NEVER a Unity or managed pointer.
    inline void CaptureSnapshot(int guid, Obj* data) {
        if (guid<=0 || !data || !dataClass.IsValid() || !BNM::IsA(data,dataClass)) return;
        if (!displayCost.IsValid() || !displayAttack.IsValid() || !displayHealth.IsValid() ||
            !isFighter.IsValid() || !isEnvironment.IsValid() || !faction.IsValid()) return;
        Snapshot v;
        v.cost=displayCost[data].Get(); v.attack=displayAttack[data].Get();
        v.health=displayHealth[data].Get(); v.fighter=isFighter[data].Get();
        v.environment=isEnvironment[data].Get(); v.faction=faction[data].Get();
        if(cardColorField.IsValid()) v.color=cardColorField[data].Get();
        if(prefabName.IsValid()) { Str* x=prefabName[data].Get(); if(x) v.prefab=x->str(); }
        if(baseId.IsValid()) { Str* x=baseId[data].Get(); if(x) v.base=x->str(); }
        bool inserted=false;
        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            inserted=snapshots.find(guid)==snapshots.end();
            snapshots[guid]=std::move(v);
        }
        if(inserted && ++snapshotCount<=15)
            LOG_INFO("CustomComponents/Pick3/META_SNAPSHOT: captured guid=%d total=%u",guid,snapshotCount);
    }
    inline bool ReadSnapshot(int guid, Obj* mapper, CardInfo& info) {
        Snapshot v;
        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            auto it=snapshots.find(guid);
            if(it==snapshots.end()) return false;
            v=it->second;
        }
        info.hasCardData=true; info.fighter=v.fighter;
        info.environment=v.environment; info.zombie=v.faction==1;
        info.cardColor=v.color;
        info.cost=std::to_string(v.cost);
        info.stats=v.fighter ? std::to_string(v.attack)+" / "+std::to_string(v.health) : "-";
        if(mapper) {
            info.localizedName=TryTranslate(mapper,v.prefab,true,info.name) ||
                (v.base!=v.prefab && TryTranslate(mapper,v.base,true,info.name));
            info.localizedDescription=TryTranslate(mapper,v.prefab,false,info.description) ||
                (v.base!=v.prefab && TryTranslate(mapper,v.base,false,info.description));
        }
        if(!info.localizedName && !v.prefab.empty()) info.name=v.prefab;
        LOG_INFO("CustomComponents/Pick3/META_SNAPSHOT: HIT guid=%d cost=%d fighter=%d localized=%d",guid,v.cost,v.fighter?1:0,info.localizedName?1:0);
        return true;
    }


    inline void Initialize() {
        mapperClass = BNM::Class("PvZCards.Core.Utilities", "CardDataMapper");
        dataClass = BNM::Class("PvZCards.Core.Utilities", "CardData");
        textConstantsClass = BNM::Class("PvZCards.Game", "CardTextConstants");
        localizedMediatorClass = BNM::Class("PvZCards.Utilities", "LocalizedTextSetterMediator");
        localizationClass = BNM::Class("PvZCards.Utilities", "LocalizationMapper");
        const auto resources = BNM::Class("UnityEngine", "Resources");
        if (mapperClass.IsValid()) getCardData = mapperClass.GetMethod("GetCardData", 1).cast<Obj*>();
        if (dataClass.IsValid()) {
            displayAttack = dataClass.GetField("DisplayAttack");
            displayHealth = dataClass.GetField("DisplayHealth");
            displayCost = dataClass.GetField("DisplaySunCost");
            isFighter = dataClass.GetField("IsFighter");
            isEnvironment = dataClass.GetField("IsEnvironment");
            faction = dataClass.GetField("Faction");
            cardColorField = dataClass.GetField("Color");
            prefabName = dataClass.GetField("PrefabName");
            baseId = dataClass.GetField("BaseId");
            specialAbilities = dataClass.GetField("SpecialAbilities").cast<Obj*>();
        }
        const BNM::Class attackDecider("PvZCards.Game", "AttackIconDecider");
        const BNM::Class healthDecider("PvZCards.Game", "HealthIconDecider");
        if (attackDecider.IsValid()) attackIconDecider=attackDecider.GetMethod("GetAttackSpriteType",1).cast<int>();
        if (healthDecider.IsValid()) healthIconDecider=healthDecider.GetMethod("GetHealthSpriteType",1).cast<int>();
        if (textConstantsClass.IsValid()) {
            nameKey = textConstantsClass.GetMethod("GetName", 1).cast<Str*>();
            descriptionKey = textConstantsClass.GetMethod("GetShortDescription", 1).cast<Str*>();
        }
        if (localizedMediatorClass.IsValid())
            localizationInMediator = localizedMediatorClass.GetField("<LocalizationMapper>k__BackingField");
        if (localizationClass.IsValid()) {
            hasKey = localizationClass.GetMethod("HasKey", 1).cast<bool>();
            translate = localizationClass.GetMethod("TranslateString", 1).cast<Str*>();
        }
        if (resources.IsValid()) findAll = resources.GetMethod("FindObjectsOfTypeAll", 1).cast<ObjectArray*>();
        dataReady = mapperClass.IsValid() && dataClass.IsValid() && getCardData.IsValid() &&
            displayAttack.IsValid() && displayHealth.IsValid() && displayCost.IsValid() &&
            isFighter.IsValid() && isEnvironment.IsValid() && faction.IsValid() &&
            prefabName.IsValid() && baseId.IsValid();
        localizationReady = localizedMediatorClass.IsValid() && localizationClass.IsValid() &&
            localizationInMediator.IsValid() && findAll.IsValid() && nameKey.IsValid() &&
            descriptionKey.IsValid() && hasKey.IsValid() && translate.IsValid();
        CC_DIAG("CustomComponents/Pick3/DATA: SETUP cardData=%d localization=%d",
                 dataReady ? 1 : 0, localizationReady ? 1 : 0);
        LOG_INFO("CustomComponents/Pick3/STOCK_TRAITS: READY available=%d enabled=%d source=CardData.SpecialAbilities",
                 specialAbilities.IsValid() && attackIconDecider.IsValid() &&
                 healthIconDecider.IsValid() ? 1 : 0, useTraitIcons?1:0);
    }

    inline Obj* FindLocalizationMapper() {
        if (!localizationReady) return nullptr;
        ObjectArray* mediators = nullptr;
        const auto ex = BNM::TryInvoke([&] {
            mediators = findAll(localizedMediatorClass.GetMonoType());
        });
        if (ex.IsValid() || !mediators) return nullptr;
        // Do not keep scene-owned mediator pointers across scene changes.
        const auto count = mediators->GetCapacity();
        for (BNM::IL2CPP::il2cpp_array_size_t i = 0; i < count; ++i) {
            Obj* mediator = mediators->m_Items[i];
            if (!mediator || !BNM::IsA(mediator, localizedMediatorClass)) continue;
            Obj* mapper = localizationInMediator[mediator].Get();
            if (mapper && BNM::IsA(mapper, localizationClass)) return mapper;
        }
        return nullptr;
    }

    inline bool TryTranslate(Obj* mapper, const std::string& asset, bool forName,
                             std::string& result) {
        if (!mapper || asset.empty()) return false;
        Str* key = nullptr;
        const auto keyEx = BNM::TryInvoke([&] {
            key = forName ? nameKey(BNM::CreateMonoString(asset)) :
                            descriptionKey(BNM::CreateMonoString(asset));
        });
        if (keyEx.IsValid() || !key) return false;
        bool exists = false;
        const auto checkEx = BNM::TryInvoke([&] { exists = hasKey[mapper](key); });
        if (checkEx.IsValid() || !exists) return false;
        Str* text = nullptr;
        const auto translateEx = BNM::TryInvoke([&] { text = translate[mapper](key); });
        if (translateEx.IsValid() || !text) return false;
        result = text->str();
        return !result.empty();
    }

    // Targeting instructions are keyed by the ACTIVATING prefab, not by a
    // randomly displayed candidate. The en_# entry uses the exact prefab
    // string, including spaces/case: "Banana Bonsai Bonanza_PickFromThreeTargeting".
    // Never substitute _Targeting: it may describe a different action and
    // is misleading when the effect's source chain cannot be verified.
    inline bool TryRawLocalizationKey(Obj* mapper, const std::string& key,
                                      std::string& out) {
        if (!mapper || !localizationReady || key.empty()) return false;
        Str* managedKey = BNM::CreateMonoString(key);
        bool exists = false;
        const auto check = BNM::TryInvoke([&] { exists = hasKey[mapper](managedKey); });
        if (check.IsValid() || !exists) return false;
        Str* translated = nullptr;
        const auto translatedEx = BNM::TryInvoke([&] { translated = translate[mapper](managedKey); });
        if (translatedEx.IsValid() || !translated) return false;
        out = translated->str();
        return !out.empty();
    }

    inline bool TryPickFromThreeTargeting(int sourceCardGuid, Obj* mapper,
                                         std::string& out, std::string* resolvedKey = nullptr) {
        if (!mapper || sourceCardGuid <= 0 || !dataReady) return false;
        // The active CardDataMapper snapshot has already copied prefab names
        // during SetCardData. Use that stable string first: the native artwork
        // provider is often disabled in safe mode (factory probe 4), and its
        // absence must NOT suppress our custom localization entry.
        std::string prefabText;
        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            auto it=snapshots.find(sourceCardGuid);
            if(it!=snapshots.end()) prefabText=it->second.prefab;
        }
        if(prefabText.empty()) {
            // Legacy fallback: only if a valid, live provider was captured.
            Obj* provider=Pick3SpriteCache::Target(Pick3SpriteCache::providerRoot);
            if(provider && mapperClass.IsValid() && BNM::IsA(provider,mapperClass) &&
               getCardData.IsValid() && prefabName.IsValid()) {
                Obj* data=nullptr;
                const auto got=BNM::TryInvoke([&]{data=getCardData[provider](sourceCardGuid);});
                if(!got.IsValid() && data && dataClass.IsValid() && BNM::IsA(data,dataClass)) {
                    Str* prefab=prefabName[data].Get();
                    if(prefab) prefabText=prefab->str();
                }
            }
        }
        if(prefabText.empty()) return false;
        const std::string specific = prefabText + "_PickFromThreeTargeting";
        if (TryRawLocalizationKey(mapper, specific, out)) {
            if (resolvedKey) *resolvedKey = specific;
            return true;
        }
        return false;
    }

    inline CardInfo Read(int cardId, Obj* localizationMapper) {
        CardInfo info;
        char numeric[48]{};
        std::snprintf(numeric, sizeof(numeric), "Card #%d", cardId);
        info.name = numeric;
        info.cost = "?";
        info.stats = "?";
        info.description = "Text unavailable";
        if (ReadSnapshot(cardId,localizationMapper,info)) return info;
        if (!dataReady) return info;
        Obj* provider = Pick3SpriteCache::Target(Pick3SpriteCache::providerRoot);
        if (!provider || !BNM::IsA(provider, mapperClass)) {
            if (warnedMissingProvider++ == 0)
                CC_DIAG("CustomComponents/Pick3/DATA: waiting for game's CardDataMapper capture");
            return info;
        }
        Obj* data = nullptr;
        const auto dataEx = BNM::TryInvoke([&] { data = getCardData[provider](cardId); });
        if (dataEx.IsValid() || !data || !BNM::IsA(data, dataClass)) {
            CC_DIAG("CustomComponents/Pick3/DATA: cardId=%d GetCardData unavailable exception=%d",
                     cardId, dataEx.IsValid() ? 1 : 0);
            return info;
        }
        info.hasCardData = true;
        char buffer[64]{};
        const int cost = displayCost[data].Get();
        const int attack = displayAttack[data].Get();
        const int health = displayHealth[data].Get();
        const bool fighter = isFighter[data].Get();
        info.fighter = fighter;
        info.environment = isEnvironment[data].Get();
        info.zombie = faction[data].Get() == 1;
        if (cardColorField.IsValid()) info.cardColor = cardColorField[data].Get();
        if (fighter && useTraitIcons && specialAbilities.IsValid() &&
            attackIconDecider.IsValid() && healthIconDecider.IsValid()) {
            Obj* abilities=specialAbilities[data].Get();
            // Use the game's actual List<SpecialAbilityType> and its OWN
            // trait-priority rules. Do not infer ability types from localized
            // descriptions or enum names, and do not mutate the list.
            if (abilities) {
                int a=0, h=1;
                const auto ea=BNM::TryInvoke([&]{ a=attackIconDecider(abilities); });
                const auto eh=BNM::TryInvoke([&]{ h=healthIconDecider(abilities); });
                if (!ea.IsValid() && !eh.IsValid() && a>=0 && a<15 && h>=0 && h<15) {
                    info.attackIconType=a;
                    info.healthIconType=h;
                    info.iconDecidersUsed=true;
                }
            }
        }
        if (cost >= 0) { std::snprintf(buffer, sizeof(buffer), "%d", cost); info.cost = buffer; }
        if (fighter) {
            std::snprintf(buffer, sizeof(buffer), "%d / %d", attack, health);
            info.stats = buffer;
        } else {
            info.stats = "-"; // Tricks/environments have no fighter attack/health.
        }
        Str* prefab = prefabName[data].Get();
        Str* base = baseId[data].Get();
        const std::string prefabStr = prefab ? prefab->str() : "";
        const std::string baseStr = base ? base->str() : "";
        if (localizationMapper) {
            info.localizedName = TryTranslate(localizationMapper, prefabStr, true, info.name) ||
                (baseStr != prefabStr && TryTranslate(localizationMapper, baseStr, true, info.name));
            info.localizedDescription = TryTranslate(localizationMapper, prefabStr, false, info.description) ||
                (baseStr != prefabStr && TryTranslate(localizationMapper, baseStr, false, info.description));
        }
        CC_DIAG("CustomComponents/Pick3/DATA: cardId=%d base='%s' prefab='%s' cost=%d attack=%d health=%d fighter=%d nameLoc=%d descLoc=%d",
                 cardId, baseStr.c_str(), prefabStr.c_str(), cost, attack, health,
                 fighter ? 1 : 0, info.localizedName ? 1 : 0, info.localizedDescription ? 1 : 0);
        return info;
    }
}
