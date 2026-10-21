#pragma once
// Stock UI sprite capture, never an asset loader. All hooks are passive:
// forward stock first, then copy only initialized Sprite fields.
// The registry retains managed GC handles, NOT AssetBundle ownership.
// Unity native-lifetime is checked again before a borrowed Sprite is used.
// Do not enable concurrently with Pick3AssetTracer (same stock hooks).
#include <array>
#include <cstdint>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "core/SceneViewContext.hpp"
#include "log.h"

namespace CustomComponents::Presentation::Pick3GameOwnedAssetRegistry {
using Obj = BNM::IL2CPP::Il2CppObject;
using VoidFn = void (*)(void*);
using VoidIntFn = void (*)(void*, int);
using IconFn = void* (*)(void*, int);
struct RGBA { float r, g, b, a; };

inline bool enabled = false;
inline bool useInPicker = false;
inline bool logCapture = true;
inline BNM::Class spriteClass;
inline BNM::Method<bool> isUnityAlive;
inline BNM::Field<Obj*> collectionPlant, collectionZombie, collectionIcons, collectionScheme;
inline BNM::Field<Obj*> boardIcons, boardScheme, inspectionIcons;
inline BNM::Field<Obj*> costViewSunGO, costViewBrainGO;
inline BNM::Class imageClass, spriteRendererClass;
inline BNM::Method<Obj*> getImageComponent, getImageChild, getSpriteRendererComponent, getSpriteRendererChild;
inline BNM::Method<Obj*> imageSprite, rendererSprite;
inline VoidIntFn stockInspectionSetFaction = nullptr, stockInspectionShowCost = nullptr;
inline bool costViewHookAvailable = false;
inline std::array<BNM::Field<Obj*>, 15> iconFields{};
inline std::array<BNM::Field<RGBA>, 10> classColors{};
inline std::array<std::uint32_t, 15> iconRoots{};
inline std::array<std::uint32_t, 2> costRoots{};
inline std::array<RGBA, 10> palette{};
inline std::array<bool, 10> paletteValid{};
inline VoidFn stockCollectionAwake = nullptr, stockBoardAwake = nullptr,
              stockInspectionAwake = nullptr;
inline IconFn stockIconByType = nullptr;
inline int captureCount = 0, usedCount = 0, invalidCount = 0;
inline bool warnedNoGC = false;
inline std::array<bool, 4> loggedUse{};
inline std::array<bool, 4> loggedFallback{};
inline bool palettePrinted = false;
inline constexpr std::array<const char*, 15> kIcons = {
    "BasicAttackSprite", "BasicHealthSprite", "ArmorSprite", "DeadlySprite",
    "FrenzySprite", "MultiSprite", "StrikethroughSprite", "TruestrikeSprite",
    "UnhurtableSprite", "AmbushSprite", "AttackOverrideSprite", "RepeaterSprite",
    "OvershootSprite", "HealthMultiSprite", "UntrickableSprite"
};
inline constexpr std::array<const char*, 10> kColors = {
    "Kabloom", "MegaGro", "Guardian", "Smarty", "Solar", "Brainy",
    "Hearty", "Sneaky", "Hungry", "Madcap"
};

inline void Release(std::uint32_t& handle) {
    if (handle && Core::SceneViewContext::freeGCHandle)
        Core::SceneViewContext::freeGCHandle(handle);
    handle = 0;
}
inline void Reset(const char* reason) {
    for (auto& r : costRoots) Release(r);
    for (auto& r : iconRoots) Release(r);
    paletteValid.fill(false);
    loggedUse.fill(false);
    loggedFallback.fill(false);
    palettePrinted = false;
    if (enabled && logCapture)
        LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: RESET reason=%s", reason);
}
inline bool Alive(Obj* sprite) {
    if (!sprite || !spriteClass.IsValid() || !BNM::IsA(sprite, spriteClass)) return false;
    // Unlike a GC handle, Object.op_Implicit also checks Unity's native object.
    // Fail closed if the managed/native liveness API isn't available.
    if (!isUnityAlive.IsValid()) return false;
    bool result = false;
    const auto ex = BNM::TryInvoke([&]{ result = isUnityAlive(sprite); });
    return !ex.IsValid() && result;
}
inline Obj* Target(std::uint32_t& handle) {
    if (!handle || !Core::SceneViewContext::getGCHandle) return nullptr;
    Obj* obj = Core::SceneViewContext::getGCHandle(handle);
    if (!Alive(obj)) {
        Release(handle);
        ++invalidCount;
        return nullptr;
    }
    return obj;
}
inline bool Remember(std::uint32_t& root, Obj* sprite) {
    if (!sprite || !Alive(sprite)) return false;
    if (!Core::SceneViewContext::newGCHandle || !Core::SceneViewContext::getGCHandle ||
        !Core::SceneViewContext::freeGCHandle) {
        if (!warnedNoGC) {
            warnedNoGC = true;
            LOG_ERR("CustomComponents/Pick3/STOCK_REGISTRY: GC handles unavailable; capture skipped");
        }
        return false;
    }
    if (root && Target(root) == sprite) return false;
    const std::uint32_t newRoot = Core::SceneViewContext::newGCHandle(sprite, false);
    if (!newRoot) return false;
    Release(root);
    root = newRoot;
    ++captureCount;
    return true;
}
inline void CapturePalette(void* scheme, const char* source) {
    if (!scheme) return;
    int count = 0;
    for (int i = 0; i < 10; ++i) {
        if (!classColors[i].IsValid()) continue;
        const auto v = classColors[i][scheme].Get();
        if (!(v.a > 0.f && v.a <= 1.f)) continue;
        palette[i] = v;
        paletteValid[i] = true;
        ++count;
    }
    if (count && logCapture && !palettePrinted) {
        palettePrinted = true;
        LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: PALETTE source=%s classes=%d", source, count);
    }
}
inline void CaptureIcons(void* iconSet, const char* source) {
    if (!iconSet) return;
    int updated = 0;
    for (std::size_t i = 0; i < iconFields.size(); ++i) {
        // Never make fifteen Unity native checks on every stat-icon draw.
        // Captured roots are revalidated at the point of use instead.
        if (!iconRoots[i] && iconFields[i].IsValid() &&
            Remember(iconRoots[i], iconFields[i][iconSet].Get())) ++updated;
    }
    if (updated && logCapture)
        LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: ICONS source=%s updated=%d total=%d", source, updated, captureCount);
}
// This path observes the stock inspection's already-activated cost icons.
// No Unity bundles, Resources scans, GameObject creation or scene ownership.
inline Obj* IconFromGO(Obj* gameObject) {
    if (!gameObject) return nullptr;
    Obj* component = nullptr;
    if (getImageComponent.IsValid() && imageClass.IsValid()) {
        const auto e = BNM::TryInvoke([&] { component = getImageComponent[gameObject](imageClass.GetMonoType()); });
        if (e.IsValid()) component = nullptr;
    }
    if (!component && getImageChild.IsValid() && imageClass.IsValid()) {
        const auto e = BNM::TryInvoke([&] { component = getImageChild[gameObject](imageClass.GetMonoType()); });
        if (e.IsValid()) component = nullptr;
    }
    Obj* result = nullptr;
    if (component && imageSprite.IsValid()) {
        const auto e = BNM::TryInvoke([&] { result = imageSprite[component](); });
        if (e.IsValid()) result = nullptr;
    }
    if (result) return result;
    component = nullptr;
    if (getSpriteRendererComponent.IsValid() && spriteRendererClass.IsValid()) {
        const auto e = BNM::TryInvoke([&] { component = getSpriteRendererComponent[gameObject](spriteRendererClass.GetMonoType()); });
        if (e.IsValid()) component = nullptr;
    }
    if (!component && getSpriteRendererChild.IsValid() && spriteRendererClass.IsValid()) {
        const auto e = BNM::TryInvoke([&] { component = getSpriteRendererChild[gameObject](spriteRendererClass.GetMonoType()); });
        if (e.IsValid()) component = nullptr;
    }
    if (component && rendererSprite.IsValid()) {
        const auto e = BNM::TryInvoke([&] { result = rendererSprite[component](); });
        if (e.IsValid()) result = nullptr;
    }
    return result;
}
inline void CaptureInspectionCost(void* self) {
    if (!enabled || !self) return;
    Obj* sun = costViewSunGO.IsValid() ? IconFromGO(costViewSunGO[self].Get()) : nullptr;
    Obj* brain = costViewBrainGO.IsValid() ? IconFromGO(costViewBrainGO[self].Get()) : nullptr;
    const bool a=Remember(costRoots[0], sun);
    const bool b=Remember(costRoots[1], brain);
    if ((a || b) && logCapture)
        LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: COSTS source=inspection-view sun=%d brain=%d", a?1:0,b?1:0);
}
inline void InspectionSetFaction(void* self, int faction) {
    stockInspectionSetFaction(self, faction);
    CaptureInspectionCost(self);
}
inline void InspectionShowCost(void* self, int cost) {
    stockInspectionShowCost(self, cost);
    CaptureInspectionCost(self);
}
inline void CollectionAwake(void* self) {
    stockCollectionAwake(self);
    if (!enabled || !self) return;
    const bool sun = collectionPlant.IsValid() && Remember(costRoots[0], collectionPlant[self].Get());
    const bool brain = collectionZombie.IsValid() && Remember(costRoots[1], collectionZombie[self].Get());
    if ((sun || brain) && logCapture)
        LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: COSTS source=collection sun=%d brain=%d", sun?1:0,brain?1:0);
    if (collectionIcons.IsValid()) CaptureIcons(collectionIcons[self].Get(), "collection");
    if (collectionScheme.IsValid()) CapturePalette(collectionScheme[self].Get(), "collection");
}
inline void BoardAwake(void* self) {
    stockBoardAwake(self);
    if (!enabled || !self) return;
    if (boardIcons.IsValid()) CaptureIcons(boardIcons[self].Get(), "board.awake");
    if (boardScheme.IsValid()) CapturePalette(boardScheme[self].Get(), "board.awake");
}
inline void InspectionAwake(void* self) {
    stockInspectionAwake(self);
    if (enabled && self && inspectionIcons.IsValid())
        CaptureIcons(inspectionIcons[self].Get(), "inspection.awake");
}
inline void* IconByType(void* self, int type) {
    void* result = stockIconByType(self, type);
    // The icon resolver is naturally called after board-frame Awake has
    // completed, when its IconSprites field may finally be populated.
    if (enabled && self) CaptureIcons(self, "icon-resolver");
    return result;
}

inline Obj* Cost(bool zombie) {
    if (!enabled || !useInPicker) return nullptr;
    Obj* sprite = Target(costRoots[zombie ? 1 : 0]);
    if (sprite) ++usedCount;
    const int slot = zombie ? 1 : 0;
    if (logCapture && !(sprite ? loggedUse[slot] : loggedFallback[slot])) {
        (sprite ? loggedUse[slot] : loggedFallback[slot]) = true;
        LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: COST_%s faction=%s", sprite ? "USE" : "FALLBACK", zombie ? "zombie" : "plant");
    }
    return sprite;
}
inline Obj* Icon(int type) {
    if (!enabled || !useInPicker || type < 0 || type >= 15) return nullptr;
    // Captured fields are in the same order as CardFrameIconType 0..14.
    Obj* sprite = Target(iconRoots[static_cast<std::size_t>(type)]);
    if (sprite) ++usedCount;
    if (type < 2 && logCapture) {
        const int slot = type + 2;
        if (!(sprite ? loggedUse[slot] : loggedFallback[slot])) {
            (sprite ? loggedUse[slot] : loggedFallback[slot]) = true;
            LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: ICON_%s type=%d", sprite ? "USE" : "FALLBACK", type);
        }
    }
    return sprite;
}
inline bool ClassColor(int flag, RGBA& output) {
    if (!enabled || !useInPicker || flag <= 0 || (flag & (flag - 1))) return false;
    int bit = 0;
    for (int v=flag; v>1; v>>=1) ++bit;
    if (bit >= 10 || !paletteValid[bit]) return false;
    output = palette[bit];
    return true;
}
inline bool Initialize(bool requested, bool apply, bool verbose) {
    enabled = requested;
    useInPicker = requested && apply;
    logCapture = verbose;
    if (!requested) {
        LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: disabled");
        return true;
    }
    spriteClass = BNM::Class("UnityEngine", "Sprite");
    auto unityObject = BNM::Class("UnityEngine", "Object");
    if (unityObject.IsValid())
        isUnityAlive = unityObject.GetMethod("op_Implicit", 1).cast<bool>();
    const BNM::Class coll("PvZCards.UI", "CollectionCardFrameView");
    const BNM::Class board("PvZCards.Game", "CardFrameViewComponent");
    const BNM::Class panel("PvZCards.CardInspection", "CardInspectionPanelView");
    const BNM::Class icons("PvZCards.Game", "CardFrameIconSprites");
    const BNM::Class scheme("PvZCards.Game", "ScriptableObjectColorScheme");
    if (!spriteClass.IsValid() || !isUnityAlive.IsValid() ||
        !coll.IsValid() || !board.IsValid() || !panel.IsValid() || !icons.IsValid()) {
        enabled = useInPicker = false;
        LOG_ERR("CustomComponents/Pick3/STOCK_REGISTRY: required types unavailable; fallback kept");
        return false;
    }
    collectionPlant = coll.GetField("PlantCostSprite").cast<Obj*>();
    collectionZombie = coll.GetField("ZombieCostSprite").cast<Obj*>();
    collectionIcons = coll.GetField("IconSprites").cast<Obj*>();
    collectionScheme = coll.GetField("ColorScheme").cast<Obj*>();
    boardIcons = board.GetField("IconSprites").cast<Obj*>();
    boardScheme = board.GetField("ColorScheme").cast<Obj*>();
    inspectionIcons = panel.GetField("IconSprites").cast<Obj*>();
    const BNM::Class costView("PvZCards.CardInspection", "CardInspectionSunCostView");
    if (costView.IsValid()) {
        costViewSunGO=costView.GetField("SunIcon").cast<Obj*>();
        costViewBrainGO=costView.GetField("BrainIcon").cast<Obj*>();
    }
    imageClass=BNM::Class("UnityEngine.UI", "Image");
    spriteRendererClass=BNM::Class("UnityEngine", "SpriteRenderer");
    const BNM::Class go("UnityEngine", "GameObject");
    if (go.IsValid()) {
        getImageComponent=go.GetMethod("GetComponent",1).cast<Obj*>();
        getImageChild=go.GetMethod("GetComponentInChildren",1).cast<Obj*>();
        getSpriteRendererComponent=getImageComponent;
        getSpriteRendererChild=getImageChild;
    }
    if (imageClass.IsValid()) imageSprite=imageClass.GetMethod("get_sprite",0).cast<Obj*>();
    if (spriteRendererClass.IsValid()) rendererSprite=spriteRendererClass.GetMethod("get_sprite",0).cast<Obj*>();
    for (std::size_t i=0; i<kIcons.size(); ++i)
        iconFields[i] = icons.GetField(kIcons[i]).cast<Obj*>();
    if (scheme.IsValid()) for (std::size_t i=0; i<kColors.size(); ++i)
        classColors[i] = scheme.GetField(kColors[i]).cast<RGBA>();
    if (!collectionPlant.IsValid() || !collectionZombie.IsValid() ||
        !boardIcons.IsValid() || !inspectionIcons.IsValid() ||
        !iconFields[0].IsValid() || !iconFields[1].IsValid()) {
        enabled = useInPicker = false;
        LOG_ERR("CustomComponents/Pick3/STOCK_REGISTRY: sprite field mismatch; fallback kept");
        return false;
    }
    auto cAwake = coll.GetMethod("Awake",0);
    auto bAwake = board.GetMethod("Awake",0);
    auto pAwake = panel.GetMethod("Awake",0);
    auto iconByType = icons.GetMethod("GetSpriteByType",1);
    if (!cAwake.IsValid() || !bAwake.IsValid() || !pAwake.IsValid() || !iconByType.IsValid()) {
        enabled = useInPicker = false;
        LOG_ERR("CustomComponents/Pick3/STOCK_REGISTRY: method mismatch; fallback kept");
        return false;
    }
    BNM::BasicHook(cAwake, CollectionAwake, stockCollectionAwake);
    BNM::BasicHook(bAwake, BoardAwake, stockBoardAwake);
    BNM::BasicHook(pAwake, InspectionAwake, stockInspectionAwake);
    BNM::BasicHook(iconByType, IconByType, stockIconByType);
    // Optional hook: only installed when all read-only field/getter plumbing
    // exists; a missing signature must never disable the four stable hooks.
    if (costViewSunGO.IsValid() && costViewBrainGO.IsValid() &&
        ((imageSprite.IsValid() && getImageComponent.IsValid()) ||
         (rendererSprite.IsValid() && getSpriteRendererComponent.IsValid()))) {
        const auto setFaction = costView.GetMethod("SetFaction",1);
        if (setFaction.IsValid()) {
            BNM::BasicHook(setFaction, InspectionSetFaction, stockInspectionSetFaction);
            costViewHookAvailable = stockInspectionSetFaction != nullptr;
            if (costViewHookAvailable) {
                const auto showCost=costView.GetMethod("ShowAndSetSunCost",1);
                if (showCost.IsValid())
                    BNM::BasicHook(showCost, InspectionShowCost, stockInspectionShowCost);
            }
        }
    }
    if (!stockCollectionAwake || !stockBoardAwake || !stockInspectionAwake || !stockIconByType) {
        // If only some native hooks succeeded, keep forwarding them but
        // disable capture/use rather than accessing incomplete state.
        enabled = useInPicker = false;
        LOG_ERR("CustomComponents/Pick3/STOCK_REGISTRY: hook incomplete; fallback kept");
        return false;
    }
    LOG_INFO("CustomComponents/Pick3/STOCK_REGISTRY: READY hooks=%d useInPicker=%d costViewHook=%d bundlesLoaded=0 avatarCalls=0",
             4 + (costViewHookAvailable?1:0) + (stockInspectionShowCost?1:0), useInPicker?1:0, costViewHookAvailable?1:0);
    return true;
}
} // namespace CustomComponents::Presentation::Pick3GameOwnedAssetRegistry
