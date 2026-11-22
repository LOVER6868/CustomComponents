#include "core/HookRegistry.hpp"
#include "core/ManagedRuntime.hpp"
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <bloom/bloom.h>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/MethodBase.hpp>

#include "core/AbilityRegistry.hpp"
#include "log.h"

// Registry-driven custom keyword sprites. Numeric reconciliation is separate.
namespace CustomComponents::Systems::StatIconSystem {
    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;
    struct Vec2 { float x, y; };                                     // UnityEngine.Vector2
    struct Rect { float x, y, width, height; };                      // UnityEngine.Rect

    constexpr int MultiIcon = 5, HealthMultiIcon = 13;               // CardFrameIconType.Multi / HealthMulti
    constexpr int FirstIconType = 100;

    struct Entry {
        std::string ability;              // ability registry name, e.g. Thrash
        bool attack = true;               // strength place (true) or health place (false)
        int abilityType = 6;              // SpecialAbilityType it travels as
        int previousAbilityType = AbilityDescriptor::NoSpecialAbilityType;
        int iconType = FirstIconType;     // CardFrameIconType it is drawn as
        std::string imagePath;
        float scale = 1.0f;
        Obj* sprite = nullptr;
        bool tried = false;
    };
    inline std::vector<Entry> entries;
    inline bool verbose = false;
    inline std::atomic<int> logLines{0};
    inline bool Note() { return verbose && logLines.load() < 80 && logLines.fetch_add(1) < 80; }

    inline BNM::Class texture2dClass, byteClass;
    inline BNM::Method<Obj*> spriteCreate;                           // Sprite.Create(Texture2D, Rect, Vector2, float)
    inline BNM::Method<void> textureCtor, setHideFlags;              // Texture2D(width, height, format, mipChain), Object.hideFlags
    inline BNM::Method<bool> loadImage;                              // ImageConversion.LoadImage(Texture2D, byte[], bool)
    inline BNM::Method<int> textureWidth, textureHeight;             // Texture.width / height
    inline BNM::Method<Rect> spriteRect;                             // Sprite.rect
    inline BNM::Method<Vec2> spritePivot;                            // Sprite.pivot (pixels)
    inline BNM::Method<float> spritePixelsPerUnit;                   // Sprite.pixelsPerUnit
    inline std::vector<std::shared_ptr<Core::ManagedRuntime::Root>> assetRoots;
    inline BNM::Method<void*> gcAlloc;                               // GCHandle.Alloc(object)
    inline BNM::Field<Obj*> referenceAttack, referenceHealth;        // CardFrameIconSprites.StrikethroughSprite / ArmorSprite

    using IsTypeFn = bool (*)(int, const void*);
    using DeciderFn = int (*)(Obj*, const void*);
    using ConvertFn = int (*)(int, int, const void*);
    using GetSpriteFn = Obj* (*)(void*, int, const void*);
    inline IsTypeFn originalIsAttack = nullptr, originalIsHealth = nullptr, originalIsStar = nullptr;
    inline DeciderFn originalAttackDecider = nullptr, originalHealthDecider = nullptr;
    inline ConvertFn originalConvert = nullptr;
    inline GetSpriteFn originalGetSprite = nullptr;
    inline BNM::Method<Obj*> getSpriteByType;                        // CardFrameIconSprites.GetSpriteByType (through our hook)

    // v0.2 anchors: CardFrameViewComponent (the frame) and its CardFrameIconView.
    inline BNM::Field<Obj*> frameIconView, frameIconSprites;         // CardFrameViewComponent.CardFrameIconView / IconSprites
    inline BNM::Field<Obj*> viewAttackSetter, viewHealthSetter;      // CardFrameIconView.AttackSpriteRenderer / HealthSpriteRenderer
    inline BNM::Field<Obj*> viewAttackList, viewHealthList;          // CardFrameIconView.AttackSpritesEnabled / HealthSpritesEnabled
    inline BNM::Field<Obj*> stateAbilities;                          // EntityState.Abilities
    inline BNM::Method<void> refreshAttack, refreshHealth;           // CardFrameIconView.RefreshAttackSprite / RefreshHealthSprite (private)
    using StateFn = void (*)(void*, Obj*, const void*);
    using TypeFn = void (*)(void*, int, const void*);
    inline StateFn originalSetDataFromState = nullptr;
    inline TypeFn originalAddType = nullptr, originalAddAttack = nullptr, originalRemoveAttack = nullptr,
                  originalAddHealth = nullptr, originalRemoveHealth = nullptr;

    // One line per hook the first time it runs: tells which ones the game really reaches.
    enum Fired { FIsAttack, FIsHealth, FIsStar, FAttackDecider, FHealthDecider, FConvert, FGetSprite, FSetData, FAddType,
                 FAddAttack, FRemoveAttack, FAddHealth, FRemoveHealth, FCount };
    inline std::array<std::atomic<bool>, FCount> fired{};
    inline void First(Fired which, const char* name) {
        if (!fired[which].exchange(true)) LOG_INFO("CustomComponents/StatIcons: first call of %s", name);
    }

    inline const Entry* ByAbilityType(int type) {
        for (const auto& entry : entries) if (entry.abilityType == type) return &entry;
        return nullptr;
    }
    inline Entry* ByIconType(int type) {
        for (auto& entry : entries) if (entry.iconType == type) return &entry;
        return nullptr;
    }

    // ---- Which place ----------------------------------------------------------------------------------
    inline bool IsAttackHook(int type, const void* method) {
        First(FIsAttack, "CardViewIconUtils.IsAttackTypeSprite");
        if (const auto* entry = ByAbilityType(type)) return entry->attack;
        return originalIsAttack(type, method);
    }
    inline bool IsHealthHook(int type, const void* method) {
        First(FIsHealth, "CardViewIconUtils.IsHealthTypeSprite");
        if (const auto* entry = ByAbilityType(type)) return !entry->attack;
        return originalIsHealth(type, method);
    }
    inline bool IsStarHook(int type, const void* method) {
        First(FIsStar, "CardViewIconUtils.IsStarSprite");
        if (ByAbilityType(type)) return false;
        return originalIsStar(type, method);
    }

    // ---- Which picture for the place's list ------------------------------------------------------------
    // Only when one of ours is in the list; otherwise the game decides exactly as before.
    inline int Decide(Obj* list, bool attack, int multi, DeciderFn original, const void* method) {
        auto* items = reinterpret_cast<MonoList<int>*>(list);
        if (!items || !items->items || items->size <= 0) return original(list, method);
        const Entry* ours = nullptr;
        std::vector<int> different;
        for (int i = 0; i < items->size; i++) {
            const int type = items->items->m_Items[i];
            if (type == 0) continue;                                     // SpecialAbilityType.None
            const auto* mine = ByAbilityType(type);
            // A card's whole list can reach the decider (the collection passes CardData's): count this place only.
            const IsTypeFn isPlace = attack ? originalIsAttack : originalIsHealth;
            const bool samePlace = mine ? mine->attack == attack : (isPlace && isPlace(type, nullptr));
            if (!samePlace) continue;
            bool seen = false;
            for (int d : different) seen |= d == type;
            if (!seen) different.push_back(type);
            if (const auto* entry = ByAbilityType(type); entry && entry->attack == attack) ours = entry;
        }
        if (!ours) return original(list, method);
        const int result = different.size() == 1 ? ours->iconType : multi;
        if (Note()) LOG_INFO("CustomComponents/StatIcons: %s icon: %s among %zu keyword(s) -> icon type %d",
                             attack ? "strength" : "health", ours->ability.c_str(), different.size(), result);
        return result;
    }
    inline int AttackDeciderHook(Obj* list, const void* method) { First(FAttackDecider, "AttackIconDecider.GetAttackSpriteType"); return Decide(list, true, MultiIcon, originalAttackDecider, method); }
    inline int HealthDeciderHook(Obj* list, const void* method) { First(FHealthDecider, "HealthIconDecider.GetHealthSpriteType"); return Decide(list, false, HealthMultiIcon, originalHealthDecider, method); }

    inline int ConvertHook(int type, int fallback, const void* method) {
        First(FConvert, "SpecialAbilityTypeToCardFrameIconTypeConverter.Convert");
        if (const auto* entry = ByAbilityType(type)) return entry->iconType;
        return originalConvert(type, fallback, method);
    }

    // ---- The picture -----------------------------------------------------------------------------------
    inline bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) return false;
        uint8_t buffer[16384];
        size_t count;
        out.clear();
        while ((count = std::fread(buffer, 1, sizeof buffer, file)) > 0) {
            if(out.size()+count>16u*1024u*1024u){std::fclose(file);out.clear();return false;}
            out.insert(out.end(),buffer,buffer+count);
        }
        std::fclose(file);
        return true;
    }

    // Made once, the first time the game asks for it, sized against the game's own keyword icon of that place.
    inline Obj* SpriteFor(Entry& entry, Obj* iconSprites) {
        if (entry.sprite || entry.tried) return entry.sprite;
        entry.tried = true;
        Obj* reference = iconSprites ? (entry.attack ? referenceAttack : referenceHealth)[iconSprites].Get() : nullptr;
        if (!reference) {
            LOG_ERR("CustomComponents/StatIcons: the game's own %s keyword icon is not there to size against; %s keeps the game's icon",
                    entry.attack ? "strength" : "health", entry.ability.c_str());
            return nullptr;
        }
        const Rect refRect = spriteRect[reference]();
        const Vec2 refPivot = spritePivot[reference]();
        const float refPpu = spritePixelsPerUnit[reference]();
        if (refRect.width <= 0 || refRect.height <= 0 || refPpu <= 0) {
            LOG_ERR("CustomComponents/StatIcons: the game's icon has no size (%.1f x %.1f, %.1f px/unit)", refRect.width, refRect.height, refPpu);
            return nullptr;
        }
        std::vector<uint8_t> png;
        if (!ReadFile(entry.imagePath, png) || png.empty()) {
            LOG_WARN("CustomComponents/StatIcons: no picture at %s (%s); %s keeps the game's icon", entry.imagePath.c_str(), std::strerror(errno), entry.ability.c_str());
            return nullptr;
        }
        Obj* texture = texture2dClass.CreateNewInstance();
        if (!texture) return nullptr;
        Core::ManagedRuntime::Root textureRoot(texture);
        if(!textureRoot.Get())return nullptr;
        textureCtor[texture](2, 2, 4 /* TextureFormat.RGBA32 */, false);
        auto* bytes = byteClass.NewArray<uint8_t>(png.size());   // the game's own allocator (BNM Array::Create is unsafe here)
        Core::ManagedRuntime::Root bytesRoot(reinterpret_cast<Obj*>(bytes));
        if (bytes) std::memcpy(bytes->m_Items, png.data(), png.size());
        if (!bytes || !loadImage(texture, bytes, true)) {
            LOG_ERR("CustomComponents/StatIcons: %s is not an image the game can read (use PNG)", entry.imagePath.c_str());
            return nullptr;
        }
        const float width = static_cast<float>(textureWidth[texture]()), height = static_cast<float>(textureHeight[texture]());
        // Fit inside the game's icon (world size = pixels / pixels per unit), keep our proportions, times Scale.
        const float refWorldW = refRect.width / refPpu, refWorldH = refRect.height / refPpu;
        float ppu = std::max(width / refWorldW, height / refWorldH) / (entry.scale > 0 ? entry.scale : 1.0f);
        const Vec2 pivot{refPivot.x / refRect.width, refPivot.y / refRect.height};   // Sprite.Create takes it as a fraction
        Obj* sprite = spriteCreate(texture, Rect{0, 0, width, height}, pivot, ppu);
        if (!sprite) {
            LOG_ERR("CustomComponents/StatIcons: could not make a sprite from %s", entry.imagePath.c_str());
            return nullptr;
        }
        Core::ManagedRuntime::Root spriteRoot(sprite);if(!spriteRoot.Get())return nullptr;
        setHideFlags[texture](61 /* HideFlags.HideAndDontSave: survives scene changes */);
        setHideFlags[sprite](61);
        assetRoots.push_back(std::make_shared<Core::ManagedRuntime::Root>(texture));
        assetRoots.push_back(std::make_shared<Core::ManagedRuntime::Root>(sprite));
        entry.sprite = sprite;
        LOG_INFO("CustomComponents/StatIcons: %s picture %s (%.0fx%.0f) made at %.1f px/unit, pivot %.2f, %.2f; game's %s icon: %.0fx%.0f at %.1f px/unit, pivot %.1f, %.1f px",
                 entry.ability.c_str(), entry.imagePath.c_str(), width, height, ppu, pivot.x, pivot.y,
                 entry.attack ? "Strikethrough" : "Armor", refRect.width, refRect.height, refPpu, refPivot.x, refPivot.y);
        return sprite;
    }

    inline Obj* GetSpriteHook(void* self, int type, const void* method) {
        First(FGetSprite, "CardFrameIconSprites.GetSpriteByType");
        if (auto* entry = ByIconType(type)) {
            if (Obj* sprite = SpriteFor(*entry, static_cast<Obj*>(self))) return sprite;
            // No picture: the game's plain icon of that place, never an empty slot.
            return originalGetSprite(self, entry->attack ? 0 /* BasicAttack */ : 1 /* BasicHealth */, method);
        }
        return originalGetSprite(self, type, method);
    }

    // ---- v0.2: the frame anchors ------------------------------------------------------------------------
    inline MonoList<int>* IntList(Obj* list) { return reinterpret_cast<MonoList<int>*>(list); }
    inline bool Contains(Obj* list, int type) {
        auto* items = IntList(list);
        if (!items || !items->items) return false;
        for (int i = 0; i < items->size; i++) if (items->items->m_Items[i] == type) return true;
        return false;
    }
    inline std::string Describe(Obj* list) {
        auto* items = IntList(list);
        if (!items || !items->items) return "none";
        std::string out;
        for (int i = 0; i < items->size; i++) out += (i ? "," : "") + std::to_string(items->items->m_Items[i]);
        return out.empty() ? "empty" : out;
    }
    // List<SpecialAbilityType>.Add / Remove, through the managed list (keeps its version counter right).
    inline void ListCall(Obj* list, const char* name, int type) {
        if (!list) return;
        auto method = BNM::Class(list).GetMethod(name, 1);
        if (method.IsValid()) method.template cast<void>()[list](type);   // Remove's bool result is not needed
    }
    // The setter is a SpriteRendererSpriteSetter (frames in a match) or an ImageSpriteSetter; both have set_sprite.
    inline void SetSprite(Obj* setter, Obj* sprite) {
        if (!setter || !sprite) return;
        auto method = BNM::Class(setter).GetMethod("set_sprite", 1);
        if (method.IsValid()) method.template cast<void>()[setter](sprite);
    }

    // After the game changed the frame's icons: if one of ours is in a place's list, that place shows ours (alone)
    // or the "more than one" picture. `state` (SetDataFromState only) says which keywords the card has right now.
    inline void Fix(Obj* frame, Obj* state, const char* where) {
        Obj* iconView = frame ? frameIconView[frame].Get() : nullptr;
        if (!iconView) return;
        Obj* sprites = frameIconSprites[frame].Get();
        Obj* abilities = state ? stateAbilities[state].Get() : nullptr;
        for (const bool attack : {true, false}) {
            Obj* list = (attack ? viewAttackList : viewHealthList)[iconView].Get();
            if (!list) continue;
            Entry* ours = nullptr;
            for (auto& entry : entries) {
                if (entry.attack != attack) continue;
                if (abilities) {
                    const bool has = Contains(abilities, entry.abilityType), listed = Contains(list, entry.abilityType);
                    if (has && !listed) ListCall(list, "Add", entry.abilityType);
                    if (!has && listed) {                                  // gone: let the game decide without it
                        ListCall(list, "Remove", entry.abilityType);
                        (attack ? refreshAttack : refreshHealth)[iconView]();
                    }
                }
                if (Contains(list, entry.abilityType)) ours = &entry;
            }
            if (!ours) continue;
            std::vector<int> different;
            auto* items = IntList(list);
            for (int i = 0; items && items->items && i < items->size; i++) {
                const int type = items->items->m_Items[i];
                if (type == 0) continue;
                bool seen = false;
                for (int d : different) seen |= d == type;
                if (!seen) different.push_back(type);
            }
            Obj* sprite = nullptr;
            if (different.size() == 1) sprite = SpriteFor(*ours, sprites);
            else if (sprites) sprite = getSpriteByType[sprites](attack ? MultiIcon : HealthMultiIcon);
            if (!sprite) continue;
            SetSprite((attack ? viewAttackSetter : viewHealthSetter)[iconView].Get(), sprite);
            if (Note()) LOG_INFO("CustomComponents/StatIcons: %s: frame %p %s place = %s (list %s)", where, static_cast<void*>(frame),
                                 attack ? "strength" : "health", different.size() == 1 ? ours->ability.c_str() : "more than one", Describe(list).c_str());
        }
    }

    inline std::atomic<int> stateLines{0};
    inline void SetDataFromStateHook(void* self, Obj* state, const void* method) {
        First(FSetData, "CardFrameViewComponent.SetDataFromState");
        originalSetDataFromState(self, state, method);
        Obj* abilities = state ? stateAbilities[state].Get() : nullptr;
        bool ours = false;
        for (const auto& entry : entries) ours |= Contains(abilities, entry.abilityType);
        if (verbose && (ours || stateLines.load() < 12) && stateLines.fetch_add(1) < 40)
            LOG_INFO("CustomComponents/StatIcons: SetDataFromState frame %p abilities %s%s", self, Describe(abilities).c_str(), ours ? " (ours)" : "");
        Fix(static_cast<Obj*>(self), state, "SetDataFromState");
    }
    inline void AddTypeHook(void* self, int type, const void* method) {
        First(FAddType, "CardFrameViewComponent.AddSpecialAbilityType");
        if (const auto* entry = ByAbilityType(type)) {
            // Not handed to the game: its (inlined) place check does not know our value.
            if (Obj* view = frameIconView[static_cast<Obj*>(self)].Get()) {
                Obj* list = (entry->attack ? viewAttackList : viewHealthList)[view].Get();
                if (list && !Contains(list, type)) ListCall(list, "Add", type);
            }
        } else {
            originalAddType(self, type, method);
        }
        Fix(static_cast<Obj*>(self), nullptr, "AddSpecialAbilityType");
    }
    inline void PlaceHook(void* self, int type, const void* method, bool attack, bool add, TypeFn original, Fired which, const char* name) {
        First(which, name);
        original(self, type, method);
        if (const auto* entry = ByAbilityType(type); entry && entry->attack == attack) {
            if (Obj* view = frameIconView[static_cast<Obj*>(self)].Get()) {
                Obj* list = (attack ? viewAttackList : viewHealthList)[view].Get();
                if (add && list && !Contains(list, type)) ListCall(list, "Add", type);
                if (!add && list && Contains(list, type)) {
                    ListCall(list, "Remove", type);
                    (attack ? refreshAttack : refreshHealth)[view]();
                }
            }
        }
        Fix(static_cast<Obj*>(self), nullptr, name);
    }
    inline void AddAttackHook(void* s, int t, const void* m) { PlaceHook(s, t, m, true, true, originalAddAttack, FAddAttack, "CardFrameViewComponent.AddAttackSprite"); }
    inline void RemoveAttackHook(void* s, int t, const void* m) { PlaceHook(s, t, m, true, false, originalRemoveAttack, FRemoveAttack, "CardFrameViewComponent.RemoveAttackSprite"); }
    inline void AddHealthHook(void* s, int t, const void* m) { PlaceHook(s, t, m, false, true, originalAddHealth, FAddHealth, "CardFrameViewComponent.AddHealthSprite"); }
    inline void RemoveHealthHook(void* s, int t, const void* m) { PlaceHook(s, t, m, false, false, originalRemoveHealth, FRemoveHealth, "CardFrameViewComponent.RemoveHealthSprite"); }

    // ---- Start-up --------------------------------------------------------------------------------------
    inline std::string Trim(const std::string& text) {
        const auto first = text.find_first_not_of(" \t");
        if (first == std::string::npos) return std::string();
        return text.substr(first, text.find_last_not_of(" \t") - first + 1);
    }

    // "Thrash, Attack, 6, thrash-frame.png, 1.0"
    inline bool ParseEntry(const std::string& line, Entry& entry) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            const size_t comma = line.find(',', start);
            parts.push_back(Trim(line.substr(start, comma == std::string::npos ? std::string::npos : comma - start)));
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        if (parts.size() < 4 || parts[0].empty() || parts[3].empty()) return false;
        entry.ability = parts[0];
        if (parts[1] == "Attack" || parts[1] == "attack" || parts[1] == "Strength" || parts[1] == "strength") entry.attack = true;
        else if (parts[1] == "Health" || parts[1] == "health") entry.attack = false;
        else return false;
        entry.abilityType = std::atoi(parts[2].c_str());
        entry.imagePath = parts[3];
        entry.scale = parts.size() > 4 ? static_cast<float>(std::atof(parts[4].c_str())) : 1.0f;
        return entry.abilityType > 0;
    }

    template<typename F> inline bool Need(F member, const char* what) {
        if (!member.IsValid()) LOG_ERR("CustomComponents/StatIcons: not found: %s", what);
        return member.IsValid();
    }

    // After AbilityDefinitions: the registry must already hold the abilities named in config.ini.
    inline bool Initialize() {
        if (!BLOOM_CONFIG_BOOL("StatIcons", "Enabled", true)) {
            LOG_INFO("CustomComponents/StatIcons: disabled in config.ini");
            return true;
        }
        verbose = BLOOM_CONFIG_BOOL("StatIcons", "Verbose", false);
        std::string folder(BLOOM_CONFIG("StatIcons", "Folder"));
        if (folder.empty()) folder = "/sdcard/Android/data/com.ea.gp.pvzheroes/files";

        for (int i = 1; i <= 4; i++) {
            const std::string key = "Icon" + std::to_string(i);
            const std::string line(BLOOM_CONFIG("StatIcons", key.c_str()));
            if (Trim(line).empty()) continue;
            Entry entry;
            if (!ParseEntry(line, entry)) {
                LOG_ERR("CustomComponents/StatIcons: %s = \"%s\" is not \"Ability, Attack or Health, ability type, picture.png[, scale]\"", key.c_str(), line.c_str());
                continue;
            }
            if (entry.abilityType <= 0 || entry.abilityType == 13 /* Unique: the star */ || ByAbilityType(entry.abilityType)) {
                LOG_ERR("CustomComponents/StatIcons: %s: ability type %d is not usable (0, 13 or already taken)", key.c_str(), entry.abilityType);
                continue;
            }
            const auto* found = AbilityRegistry::Find(std::string_view(entry.ability));
            if (!found) {
                LOG_ERR("CustomComponents/StatIcons: %s: no ability named %s in the registry", key.c_str(), entry.ability.c_str());
                continue;
            }
            if (found->specialAbilityType != AbilityDescriptor::NoSpecialAbilityType && found->specialAbilityType != entry.abilityType) {
                LOG_ERR("CustomComponents/StatIcons: %s: %s already shows as ability type %d; left as it is", key.c_str(), entry.ability.c_str(), found->specialAbilityType);
                continue;
            }
            // The registry's entries are ordinary (non-const) storage; Find only hands them out read-only.
            if(std::any_of(entries.begin(),entries.end(),[&](const auto& entry){return entry.ability==found->name;}))continue;
            bool reserved=false;AbilityRegistry::ForEach([&](const auto& other){if(other.id!=found->id && other.specialAbilityType==entry.abilityType)reserved=true;});
            if(reserved){LOG_ERR("CustomComponents/StatIcons: ability type already belongs to another keyword");continue;}
            entry.previousAbilityType=found->specialAbilityType;
            entry.iconType = FirstIconType + i;
            entry.imagePath = folder + "/sprig/sprites/" + entry.imagePath;
            entries.push_back(entry);
        }
        if (entries.empty()) {
            LOG_INFO("CustomComponents/StatIcons: no icons configured");
            return true;
        }

        const auto sprite = BNM::Class("UnityEngine", "Sprite");
        const auto texture = BNM::Class("UnityEngine", "Texture");
        const auto object = BNM::Class("UnityEngine", "Object");
        const auto utils = BNM::Class("PvZCards.Game", "CardViewIconUtils");
        const auto iconSprites = BNM::Class("PvZCards.Game", "CardFrameIconSprites");
        texture2dClass = BNM::Class("UnityEngine", "Texture2D");
        byteClass = BNM::Class("System", "Byte");
        bool ok = texture2dClass.IsValid() && byteClass.IsValid();
        ok &= Need(spriteCreate = sprite.GetMethod("Create", 4), "Sprite.Create(texture, rect, pivot, pixelsPerUnit)");
        ok &= Need(spriteRect = sprite.GetMethod("get_rect", 0), "Sprite.rect");
        ok &= Need(spritePivot = sprite.GetMethod("get_pivot", 0), "Sprite.pivot");
        ok &= Need(spritePixelsPerUnit = sprite.GetMethod("get_pixelsPerUnit", 0), "Sprite.pixelsPerUnit");
        ok &= Need(textureCtor = texture2dClass.GetMethod(".ctor", {std::string_view("width"), std::string_view("height"), std::string_view("textureFormat"), std::string_view("mipChain")}), "Texture2D(width, height, textureFormat, mipChain)");
        ok &= Need(textureWidth = texture.GetMethod("get_width", 0), "Texture.width");
        ok &= Need(textureHeight = texture.GetMethod("get_height", 0), "Texture.height");
        ok &= Need(setHideFlags = object.GetMethod("set_hideFlags", 1), "Object.hideFlags");
        ok &= Need(loadImage = BNM::Class("UnityEngine", "ImageConversion").GetMethod("LoadImage", 3), "ImageConversion.LoadImage");
        ok &= Need(referenceAttack = iconSprites.GetField("StrikethroughSprite"), "CardFrameIconSprites.StrikethroughSprite");
        ok &= Need(referenceHealth = iconSprites.GetField("ArmorSprite"), "CardFrameIconSprites.ArmorSprite");
        gcAlloc = BNM::Class("System.Runtime.InteropServices", "GCHandle").GetMethod("Alloc", 1);

        auto isAttack = utils.GetMethod("IsAttackTypeSprite", 1);
        auto isHealth = utils.GetMethod("IsHealthTypeSprite", 1);
        auto isStar = utils.GetMethod("IsStarSprite", 1);
        auto attackDecider = BNM::Class("PvZCards.Game", "AttackIconDecider").GetMethod("GetAttackSpriteType", 1);
        auto healthDecider = BNM::Class("PvZCards.Game", "HealthIconDecider").GetMethod("GetHealthSpriteType", 1);
        auto convert = BNM::Class("PvZCards.Game", "SpecialAbilityTypeToCardFrameIconTypeConverter").GetMethod("Convert", 2);
        auto getSprite = iconSprites.GetMethod("GetSpriteByType", 1);
        getSpriteByType = getSprite;
        const auto frame = BNM::Class("PvZCards.Game", "CardFrameViewComponent");
        const auto iconView = BNM::Class("PvZCards.Game", "CardFrameIconView");
        ok &= Need(frameIconView = frame.GetField("CardFrameIconView"), "CardFrameViewComponent.CardFrameIconView");
        ok &= Need(frameIconSprites = frame.GetField("IconSprites"), "CardFrameViewComponent.IconSprites");
        ok &= Need(viewAttackSetter = iconView.GetField("AttackSpriteRenderer"), "CardFrameIconView.AttackSpriteRenderer");
        ok &= Need(viewHealthSetter = iconView.GetField("HealthSpriteRenderer"), "CardFrameIconView.HealthSpriteRenderer");
        ok &= Need(viewAttackList = iconView.GetField("AttackSpritesEnabled"), "CardFrameIconView.AttackSpritesEnabled");
        ok &= Need(viewHealthList = iconView.GetField("HealthSpritesEnabled"), "CardFrameIconView.HealthSpritesEnabled");
        ok &= Need(refreshAttack = iconView.GetMethod("RefreshAttackSprite", 0), "CardFrameIconView.RefreshAttackSprite");
        ok &= Need(refreshHealth = iconView.GetMethod("RefreshHealthSprite", 0), "CardFrameIconView.RefreshHealthSprite");
        ok &= Need(stateAbilities = BNM::Class("PvZCards.Engine", "EntityState").GetField("Abilities"), "EntityState.Abilities");
        auto setData = frame.GetMethod("SetDataFromState", 1);
        auto addType = frame.GetMethod("AddSpecialAbilityType", 1);
        auto addAttack = frame.GetMethod("AddAttackSprite", 1);
        auto removeAttack = frame.GetMethod("RemoveAttackSprite", 1);
        auto addHealth = frame.GetMethod("AddHealthSprite", 1);
        auto removeHealth = frame.GetMethod("RemoveHealthSprite", 1);
        ok &= Need(setData, "CardFrameViewComponent.SetDataFromState");
        ok &= Need(addType, "CardFrameViewComponent.AddSpecialAbilityType");
        ok &= Need(addAttack, "CardFrameViewComponent.AddAttackSprite");
        ok &= Need(removeAttack, "CardFrameViewComponent.RemoveAttackSprite");
        ok &= Need(addHealth, "CardFrameViewComponent.AddHealthSprite");
        ok &= Need(removeHealth, "CardFrameViewComponent.RemoveHealthSprite");
        ok &= Need(isAttack, "CardViewIconUtils.IsAttackTypeSprite");
        ok &= Need(isHealth, "CardViewIconUtils.IsHealthTypeSprite");
        ok &= Need(isStar, "CardViewIconUtils.IsStarSprite");
        ok &= Need(attackDecider, "AttackIconDecider.GetAttackSpriteType");
        ok &= Need(healthDecider, "HealthIconDecider.GetHealthSpriteType");
        ok &= Need(convert, "SpecialAbilityTypeToCardFrameIconTypeConverter.Convert");
        ok &= Need(getSprite, "CardFrameIconSprites.GetSpriteByType");
        if (!ok) {
            entries.clear();
            return false;
        }

        Core::Hooks::Install(isAttack, IsAttackHook, originalIsAttack);
        Core::Hooks::Install(isHealth, IsHealthHook, originalIsHealth);
        Core::Hooks::Install(isStar, IsStarHook, originalIsStar);
        Core::Hooks::Install(attackDecider, AttackDeciderHook, originalAttackDecider);
        Core::Hooks::Install(healthDecider, HealthDeciderHook, originalHealthDecider);
        Core::Hooks::Install(convert, ConvertHook, originalConvert);
        Core::Hooks::Install(getSprite, GetSpriteHook, originalGetSprite);
        Core::Hooks::Install(setData, SetDataFromStateHook, originalSetDataFromState);
        Core::Hooks::Install(addType, AddTypeHook, originalAddType);
        Core::Hooks::Install(addAttack, AddAttackHook, originalAddAttack);
        Core::Hooks::Install(removeAttack, RemoveAttackHook, originalRemoveAttack);
        Core::Hooks::Install(addHealth, AddHealthHook, originalAddHealth);
        Core::Hooks::Install(removeHealth, RemoveHealthHook, originalRemoveHealth);
        if(!originalSetDataFromState || !originalAddType || !originalAddAttack || !originalRemoveAttack ||
           !originalAddHealth || !originalRemoveHealth || !originalIsAttack || !originalIsHealth || !originalIsStar ||
           !originalAttackDecider || !originalHealthDecider || !originalConvert || !originalGetSprite) {
            entries.clear();LOG_ERR("CustomComponents/StatIcons: incomplete interception; custom icon mapping disabled");return false;
        }
        for(const auto& entry:entries) {
            const auto* ability=AbilityRegistry::Find(std::string_view(entry.ability));
            if(!ability || !AbilityRegistry::SetSpecialAbilityType(ability->id,entry.abilityType)) {
                for(const auto& prior:entries)if(auto* item=AbilityRegistry::Find(std::string_view(prior.ability)))AbilityRegistry::SetSpecialAbilityType(item->id,prior.previousAbilityType);
                entries.clear();LOG_ERR("CustomComponents/StatIcons: visual enum collision");return false;
            }
        }
        for (const auto& entry : entries)
            LOG_INFO("CustomComponents/StatIcons: ready: %s shows %s in the %s place (ability type %d, icon type %d, scale %.2f)",
                     entry.ability.c_str(), entry.imagePath.c_str(), entry.attack ? "strength" : "health", entry.abilityType, entry.iconType, entry.scale);
        return true;
    }
} // namespace CustomComponents::Systems::StatIconSystem

namespace CustomComponents::Systems {
    inline bool InitializeStatIcons() { return StatIconSystem::Initialize(); }
}




