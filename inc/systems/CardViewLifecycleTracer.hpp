#pragma once
// Stage 2.3E-F -- CardView lifecycle tracer + opt-in return presentation guard.
// Gameplay workers only call NoteReturn(): no Unity / managed objects accessed there.
// Other hooks run on the frontend's stock animation/loading call sites.
// Guards only a returned card's conflicting CreateCardAsync request until its
// previous stock mapping disappears. Never removes/replaces CardViews or edits gameplay.
// History stores numeric pointer VALUES only; never retains managed object references.

#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Utils.hpp>
#include <dlfcn.h>
#include <array>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>
#include <algorithm>
#include "log.h"

namespace CustomComponents::Systems::CardViewLifecycleTracer {
    using Obj = BNM::IL2CPP::Il2CppObject;

    struct Event {
        unsigned long long seq = 0;
        int cardId = -1;
        const char* phase = nullptr; // string literals only
        std::uintptr_t owner = 0;   // mapper / animation / deleter
        std::uintptr_t view = 0;    // old or newly created view
        std::uintptr_t other = 0;   // callback / captured view / parent
        int outstanding = 0;
        int flag = 0;
    };
    inline constexpr size_t kHistorySize = 4096;
    inline constexpr size_t kMaxReplay = 24;
    inline std::mutex mutex;
    inline std::array<Event, kHistorySize> history{};
    inline unsigned long long nextSequence = 0;
    inline std::vector<int> returnedIds;
    inline std::vector<std::pair<int, int>> inflight;
    inline unsigned long sceneGeneration = 0;
    inline bool attemptedInstall = false;

    // The frontend owns these requests. GC handles (not bare managed pointers)
    // are required because an old board death can take many Unity frames.
    using NewGCHandleFn = uint32_t (*)(Obj*, bool);
    using GetGCHandleFn = Obj* (*)(uint32_t);
    using FreeGCHandleFn = void (*)(uint32_t);
    inline NewGCHandleFn newGCHandle = nullptr;
    inline GetGCHandleFn getGCHandle = nullptr;
    inline FreeGCHandleFn freeGCHandle = nullptr;
    inline bool guardEnabled = false;
    inline constexpr size_t kMaxDeferred = 64;
    inline constexpr unsigned kWarningFrames = 1800; // diagnostic only (~30s @60Hz)
    struct DeferredCreate {
        int id = -1;
        uint32_t mapper = 0, card = 0, callback = 0, parent = 0;
        const void* method = nullptr;
        unsigned long generation = 0;
        unsigned frames = 0;
        std::uintptr_t blockedView = 0; // old view observed at deferral
        bool stockRetired = false;      // only stock delete or owned hand cleanup
    };
    // Accessed only by CreateHook, Unity EventSystem.Update and scene Awake.
    inline std::vector<DeferredCreate> deferred;
    inline bool draining = false;

    inline void FreeDeferred(const DeferredCreate& item) {
        if (!freeGCHandle) return;
        for (uint32_t handle : {item.mapper, item.card, item.callback, item.parent})
            if (handle) freeGCHandle(handle);
    }
    inline bool Pin(DeferredCreate& item, Obj* self, Obj* card, Obj* callback, Obj* parent) {
        if (!newGCHandle || !self || !card || !callback) return false;
        item.mapper = newGCHandle(self, false);
        item.card = newGCHandle(card, false);
        item.callback = newGCHandle(callback, false);
        if (parent) item.parent = newGCHandle(parent, false);
        const bool ok = item.mapper && item.card && item.callback && (!parent || item.parent);
        if (!ok) { FreeDeferred(item); item = DeferredCreate{}; }
        return ok;
    }
    // Declared here; defined after originalCreate/Outstanding/GetMapped.
    inline void ProcessDeferredOnFrontendTick();
    inline void ConfirmRetirement(int id, Obj* oldView, const char* origin) {
        if (id < 0 || !oldView) return;
        for (auto& item : deferred) {
            if (item.id == id && item.blockedView == reinterpret_cast<std::uintptr_t>(oldView)) {
                item.stockRetired = true;
                LOG_INFO("CustomComponents/CardLife/COORD: RETIRED id=%d old=%p via=%s", id, oldView, origin);
            }
        }
    }

    inline BNM::Field<int> cardDataId;
    inline BNM::Field<int> cardViewId;
    inline BNM::Field<Obj*> cardMapperToViewMapper;
    inline BNM::Field<Obj*> deleterToViewMapper;
    inline BNM::Field<Obj*> deathToViewMapper;
    inline BNM::Field<Obj*> deathCaptured;
    inline BNM::Field<int> deathCardId;
    inline BNM::Field<Obj*> triggeredDeathToViewMapper;
    inline BNM::Field<Obj*> triggeredDeathCaptured;
    inline BNM::Field<int> triggeredDeathCardId;
    inline BNM::Method<Obj*> getView;

    using CreateFn = void (*)(void*, Obj*, Obj*, Obj*, bool, const void*);
    using DeleteFn = void (*)(void*, Obj*, const void*);
    using AnimationFn = void (*)(void*, const void*);
    using AnimationCompleteFn = void (*)(void*, Obj*, Obj*, const void*);
    inline CreateFn originalCreate = nullptr;
    inline DeleteFn originalDelete = nullptr;
    inline DeleteFn originalDeleteReplacement = nullptr;
    inline AnimationFn originalDeathStart = nullptr;
    inline AnimationFn originalTriggeredDeathStart = nullptr;
    inline AnimationCompleteFn originalDeathComplete = nullptr;
    inline AnimationCompleteFn originalTriggeredDeathComplete = nullptr;

    inline int CurrentOutstandingUnlocked(int id) {
        for (const auto& p : inflight)
            if (p.first == id) return p.second;
        return 0;
    }
    inline int Outstanding(int id) {
        std::lock_guard<std::mutex> lock(mutex);
        return CurrentOutstandingUnlocked(id);
    }
    inline int ChangeOutstanding(int id, int delta) {
        if (id < 0) return 0;
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& p : inflight) {
            if (p.first == id) {
                p.second = std::max(0, p.second + delta);
                return p.second;
            }
        }
        if (delta > 0) inflight.emplace_back(id, delta);
        return std::max(0, delta);
    }
    inline bool IsReturnedUnlocked(int id) {
        return std::find(returnedIds.begin(), returnedIds.end(), id) != returnedIds.end();
    }
    inline bool IsReturned(int id) {
        std::lock_guard<std::mutex> lock(mutex);
        return IsReturnedUnlocked(id);
    }
    inline void Record(const char* phase, int id, const void* owner = nullptr,
                       const void* view = nullptr, const void* other = nullptr,
                       int flag = 0) {
        if (id < 0) return;
        Event e;
        bool focused = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            e = { ++nextSequence, id, phase,
                  reinterpret_cast<std::uintptr_t>(owner),
                  reinterpret_cast<std::uintptr_t>(view),
                  reinterpret_cast<std::uintptr_t>(other),
                  CurrentOutstandingUnlocked(id), flag };
            history[(e.seq - 1) % kHistorySize] = e;
            focused = IsReturnedUnlocked(id) || e.outstanding > 1;
        }
        if (focused)
            LOG_INFO("CustomComponents/CardLife: #%llu %s id=%d owner=%p view=%p other=%p inflight=%d flag=%d",
                     e.seq, phase, e.cardId, (void*)e.owner, (void*)e.view,
                     (void*)e.other, e.outstanding, e.flag);
    }

    // Invoked on GAMEPLAY thread immediately after a verified discard -> hand transfer.
    // Replays lifecycle events captured before the return so board deaths are visible.
    inline void NoteReturn(int id, int handId, int pileId) {
        if (id < 0) return;
        std::array<Event, kMaxReplay> prior{};
        size_t count = 0;
        unsigned long generation = 0;
        int active = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!IsReturnedUnlocked(id)) returnedIds.push_back(id);
            generation = sceneGeneration;
            active = CurrentOutstandingUnlocked(id);
            const unsigned long long start = nextSequence > kHistorySize
                ? nextSequence - kHistorySize + 1 : 1;
            // Keep last kMaxReplay events for this card in reverse order.
            for (unsigned long long seq = nextSequence; seq >= start && count < kMaxReplay; --seq) {
                const Event& event = history[(seq - 1) % kHistorySize];
                if (event.seq == seq && event.cardId == id) prior[count++] = event;
                if (seq == 1) break;
            }
        }
        LOG_INFO("CustomComponents/CardLife/RETURN: id=%d hand=%d pile=%d scene=%lu outstanding=%d replay=%zu (no Unity access)",
                 id, handId, pileId, generation, active, count);
        for (size_t n = count; n > 0; --n) {
            const Event& e = prior[n - 1];
            LOG_INFO("CustomComponents/CardLife/PRIOR: #%llu %s id=%d owner=%p view=%p other=%p inflight=%d flag=%d",
                     e.seq, e.phase, e.cardId, (void*)e.owner, (void*)e.view,
                     (void*)e.other, e.outstanding, e.flag);
        }
        Record("RETURN_MARK", id, nullptr, nullptr, nullptr, handId);
    }
    inline void OnSceneAwake() {
        // The old scene's load tasks no longer have a safe completion target.
        // Drop their rooted requests, never dispatch them into the new scene.
        if (!deferred.empty()) {
            LOG_INFO("CustomComponents/CardLife/COORD: SCENE_DROP requests=%zu", deferred.size());
            for (const auto& pending : deferred) FreeDeferred(pending);
            deferred.clear();
        }
        unsigned long generation = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++sceneGeneration;
            generation = sceneGeneration;
            nextSequence = 0;
            history.fill(Event{});
            returnedIds.clear();
            inflight.clear();
        }
        LOG_INFO("CustomComponents/CardLife: SCENE_RESET generation=%lu", generation);
    }
    inline Obj* GetMapped(Obj* mapper, int id) {
        if (!mapper || id < 0 || !getView.IsValid()) return nullptr;
        return getView[mapper](id);
    }

    // Called from the existing single MapCardView hook -- do not hook it twice.
    inline void MapBefore(void* self, Obj* newView, Obj* cardData, bool replacement) {
        const int id = cardData && cardDataId.IsValid() ? cardDataId[cardData].Get() : -1;
        Obj* mapper = self && cardMapperToViewMapper.IsValid()
            ? cardMapperToViewMapper[static_cast<Obj*>(self)].Get() : nullptr;
        Obj* existing = GetMapped(mapper, id);
        Record("MAP_BEFORE", id, mapper, existing, newView, replacement ? 1 : 0);
    }
    inline void MapAfter(void* self, Obj* newView, Obj* cardData, bool replacement) {
        const int id = cardData && cardDataId.IsValid() ? cardDataId[cardData].Get() : -1;
        Obj* mapper = self && cardMapperToViewMapper.IsValid()
            ? cardMapperToViewMapper[static_cast<Obj*>(self)].Get() : nullptr;
        Obj* after = GetMapped(mapper, id);
        Record("MAP_AFTER", id, mapper, after, newView, replacement ? 1 : 0);
        ChangeOutstanding(id, -1);
    }
    static void SubmitCreate(void* self, Obj* cardData, Obj* callback, Obj* parentGO,
                             bool replacement, const void* method, int id) {
        Obj* mapper = self && cardMapperToViewMapper.IsValid()
            ? cardMapperToViewMapper[static_cast<Obj*>(self)].Get() : nullptr;
        Obj* existing = GetMapped(mapper, id);
        const int before = Outstanding(id);
        ChangeOutstanding(id, 1);
        Record("CREATE_ENTER", id, mapper, existing, callback,
               (replacement ? 1 : 0) | (before ? 2 : 0));
        if (before > 0)
            LOG_ERR("CustomComponents/CardLife/OVERLAP: id=%d pendingBefore=%d mapper=%p mapped=%p parent=%p",
                    id, before, mapper, existing, parentGO);
        originalCreate(self, cardData, callback, parentGO, replacement, method);
        Record("CREATE_SUBMITTED", id, mapper, existing, parentGO, replacement ? 1 : 0);
    }
    static void CreateHook(void* self, Obj* cardData, Obj* callback,
                           Obj* parentGO, bool replacement, const void* method) {
        const int id = cardData && cardDataId.IsValid() ? cardDataId[cardData].Get() : -1;
        Obj* mapper = self && cardMapperToViewMapper.IsValid()
            ? cardMapperToViewMapper[static_cast<Obj*>(self)].Get() : nullptr;
        Obj* existing = GetMapped(mapper, id);
        const int outstanding = Outstanding(id);
        bool alreadyQueued = false;
        for (const auto& item : deferred)
            if (item.id == id) { alreadyQueued = true; break; }

        // Never touch non-return or replacement loads. The stock replacement
        // mechanism is reserved for stock transforms and the existing hand path.
        if (guardEnabled && !replacement && id >= 0 && IsReturned(id) &&
            (existing || outstanding || alreadyQueued)) {
            if (deferred.size() < kMaxDeferred) {
                DeferredCreate item;
                item.id = id;
                item.method = method;
                item.generation = sceneGeneration;
                item.blockedView = reinterpret_cast<std::uintptr_t>(existing);
                if (Pin(item, static_cast<Obj*>(self), cardData, callback, parentGO)) {
                    deferred.push_back(item);
                    Record("CREATE_DEFERRED", id, mapper, existing, callback,
                           outstanding > 0 ? 2 : 1);
                    LOG_INFO("CustomComponents/CardLife/COORD: DEFER id=%d mapped=%p inflight=%d queue=%zu gen=%lu",
                             id, existing, outstanding, deferred.size(), item.generation);
                    return;
                }
            }
            // Preserve the stock call if infrastructure fails; don't silently
            // strand the stock LoadCardsTask with no callback.
            LOG_ERR("CustomComponents/CardLife/COORD: CANNOT_DEFER id=%d (handle/limit), STOCK_FALLBACK", id);
        }
        SubmitCreate(self, cardData, callback, parentGO, replacement, method, id);
    }
    inline void ProcessDeferredOnFrontendTick() {
        if (!guardEnabled || draining || deferred.empty()) return;
        draining = true;
        // At most one request per entity is released in a frame. A new mapped
        // view must never be overwritten by a later pending request.
        std::vector<int> releasedIds;
        for (size_t i = 0; i < deferred.size();) {
            auto& item = deferred[i];
            if (item.generation != sceneGeneration) {
                FreeDeferred(item);
                deferred.erase(deferred.begin() + i);
                continue;
            }
            Obj* cardMapper = getGCHandle(item.mapper);
            Obj* data = getGCHandle(item.card);
            Obj* callback = getGCHandle(item.callback);
            Obj* parent = item.parent ? getGCHandle(item.parent) : nullptr;
            Obj* viewMapper = cardMapper && cardMapperToViewMapper.IsValid()
                ? cardMapperToViewMapper[cardMapper].Get() : nullptr;
            Obj* existing = GetMapped(viewMapper, item.id);
            // If the previous request was still loading when we deferred,
            // capture its eventual mapped view before waiting for retirement.
            if (existing && !item.blockedView)
                item.blockedView = reinterpret_cast<std::uintptr_t>(existing);
            const bool retirementConfirmed = !item.blockedView || item.stockRetired;
            const bool release = cardMapper && data && callback && viewMapper &&
                !existing && retirementConfirmed && Outstanding(item.id) == 0 &&
                std::find(releasedIds.begin(), releasedIds.end(), item.id) == releasedIds.end();
            if (!release) {
                if (++item.frames == kWarningFrames ||
                    (item.frames > kWarningFrames && item.frames % kWarningFrames == 0)) {
                    LOG_ERR("CustomComponents/CardLife/COORD: STILL_WAITING id=%d frames=%u mapped=%p blocked=%p retired=%d inflight=%d queue=%zu",
                            item.id, item.frames, existing, (void*)item.blockedView,
                            (int)item.stockRetired, Outstanding(item.id), deferred.size());
                }
                ++i;
                continue;
            }
            const int id = item.id;
            const void* method = item.method;
            DeferredCreate pending = item; // retain GC roots until stock call returns
            deferred.erase(deferred.begin() + i);
            releasedIds.push_back(id);
            Record("CREATE_RELEASE", id, viewMapper, nullptr, callback);
            LOG_INFO("CustomComponents/CardLife/COORD: RELEASE id=%d frames=%u remaining=%zu",
                     id, pending.frames, deferred.size());
            // Dispatch using stock CreateCardAsync. We deliberately do not
            // bypass its callback: LoadCardsTask must complete normally.
            SubmitCreate(cardMapper, data, callback, parent, false, method, id);
            FreeDeferred(pending);
        }
        draining = false;
    }
    inline int CardId(Obj* view) {
        // CardView is an interface; concrete frontend objects are CardViewComponent.
        // CardId is an instance field of that concrete type at dump.cs offset 0x108.
        return view && cardViewId.IsValid() ? cardViewId[view].Get() : -1;
    }
    static void DeleteHook(void* self, Obj* view, const void* method) {
        const int id = CardId(view);
        Obj* mapper = self && deleterToViewMapper.IsValid()
            ? deleterToViewMapper[static_cast<Obj*>(self)].Get() : nullptr;
        Record("DELETE_BEFORE", id, mapper, GetMapped(mapper, id), view);
        originalDelete(self, view, method);
        Obj* after = GetMapped(mapper, id);
        Record("DELETE_AFTER", id, mapper, after, view);
        // Only confirm the exact old view's stock removal; no mutation here.
        if (!after) ConfirmRetirement(id, view, "stock-delete");
    }
    static void DeleteReplacementHook(void* self, Obj* view, const void* method) {
        const int id = CardId(view);
        Obj* mapper = self && deleterToViewMapper.IsValid()
            ? deleterToViewMapper[static_cast<Obj*>(self)].Get() : nullptr;
        Record("DELETE_REPLACEMENT_BEFORE", id, mapper, GetMapped(mapper, id), view);
        originalDeleteReplacement(self, view, method);
        Record("DELETE_REPLACEMENT_AFTER", id, mapper, GetMapped(mapper, id), view);
    }
    static void DeathStartHook(void* self, const void* method) {
        auto* anim = static_cast<Obj*>(self);
        const int id = deathCardId[anim].Get();
        Obj* mapper = deathToViewMapper[anim].Get();
        Record("DEATH_START_BEFORE", id, anim, GetMapped(mapper, id));
        originalDeathStart(self, method);
        Record("DEATH_START_AFTER", id, anim, GetMapped(mapper, id), deathCaptured[anim].Get());
    }
    static void TriggeredDeathStartHook(void* self, const void* method) {
        auto* anim = static_cast<Obj*>(self);
        const int id = triggeredDeathCardId[anim].Get();
        Obj* mapper = triggeredDeathToViewMapper[anim].Get();
        Record("ONDEATH_START_BEFORE", id, anim, GetMapped(mapper, id));
        originalTriggeredDeathStart(self, method);
        Record("ONDEATH_START_AFTER", id, anim, GetMapped(mapper, id), triggeredDeathCaptured[anim].Get());
    }
    static void DeathCompleteHook(void* self, Obj* sender, Obj* args, const void* method) {
        auto* anim = static_cast<Obj*>(self);
        const int id = deathCardId[anim].Get();
        Obj* mapper = deathToViewMapper[anim].Get();
        Record("DEATH_FRAME_COMPLETE_BEFORE", id, anim, GetMapped(mapper, id), deathCaptured[anim].Get());
        originalDeathComplete(self, sender, args, method);
        Record("DEATH_FRAME_COMPLETE_AFTER", id, anim, GetMapped(mapper, id), deathCaptured[anim].Get());
    }
    static void TriggeredDeathCompleteHook(void* self, Obj* sender, Obj* args, const void* method) {
        auto* anim = static_cast<Obj*>(self);
        const int id = triggeredDeathCardId[anim].Get();
        Obj* mapper = triggeredDeathToViewMapper[anim].Get();
        Record("ONDEATH_FRAME_COMPLETE_BEFORE", id, anim, GetMapped(mapper, id), triggeredDeathCaptured[anim].Get());
        originalTriggeredDeathComplete(self, sender, args, method);
        Record("ONDEATH_FRAME_COMPLETE_AFTER", id, anim, GetMapped(mapper, id), triggeredDeathCaptured[anim].Get());
    }

    inline bool Initialize() {
        if (attemptedInstall) return originalCreate != nullptr;
        attemptedInstall = true;
        const auto cardMapper = BNM::Class("PvZCards.Game", "CardViewMapper");
        const auto cardIdData = BNM::Class("PvZCards.Engine.Records", "CardIdData");
        const auto viewMapper = BNM::Class("PvZCards.Game", "EntityViewMapper");
        const auto deleter = BNM::Class("PvZCards.Game", "CardViewDeleter");
        const auto cardViewInterface = BNM::Class("PvZCards.Game", "CardView");
        const auto cardViewComponent = BNM::Class("PvZCards.Game", "CardViewComponent");
        const auto death = BNM::Class("PvZCards.Game", "DestroyCardEffectAnimation");
        const auto triggeredDeath = BNM::Class("PvZCards.Game", "DeathTriggerDestroyCardEffectAnimation");
        if (!cardMapper.IsValid() || !cardIdData.IsValid() || !viewMapper.IsValid() ||
            !deleter.IsValid() || !cardViewInterface.IsValid() || !cardViewComponent.IsValid() ||
            !death.IsValid() || !triggeredDeath.IsValid()) {
            LOG_ERR("CustomComponents/CardLife: missing classes; tracer disabled");
            return false;
        }
        cardDataId = cardIdData.GetField("EntityId");
        cardViewId = cardViewComponent.GetField("CardId");
        cardMapperToViewMapper = cardMapper.GetField("<ViewMapper>k__BackingField");
        deleterToViewMapper = deleter.GetField("<EntityViewMapper>k__BackingField");
        deathCardId = death.GetField("DestroyedCardId");
        deathToViewMapper = death.GetField("<EntityViewMapper>k__BackingField");
        deathCaptured = death.GetField("DestroyedCard");
        triggeredDeathCardId = triggeredDeath.GetField("DestroyedCardId");
        triggeredDeathToViewMapper = triggeredDeath.GetField("<EntityViewMapper>k__BackingField");
        triggeredDeathCaptured = triggeredDeath.GetField("<CardView>k__BackingField");
        getView = viewMapper.GetMethod("GetViewObject", 1).cast<Obj*>();
        if (!cardDataId.IsValid() || !cardViewId.IsValid() ||
            !cardMapperToViewMapper.IsValid() || !deleterToViewMapper.IsValid() ||
            !deathCardId.IsValid() || !deathToViewMapper.IsValid() || !deathCaptured.IsValid() ||
            !triggeredDeathCardId.IsValid() || !triggeredDeathToViewMapper.IsValid() ||
            !triggeredDeathCaptured.IsValid() || !getView.IsValid()) {
            LOG_ERR("CustomComponents/CardLife: missing fields or methods; tracer disabled");
            return false;
        }
        // Strong managed references for requests deferred across Unity frames.
        // All three APIs are exported by this game's libil2cpp.so.
        void* lib = BNM::GetIl2CppLibraryHandle();
        newGCHandle = reinterpret_cast<NewGCHandleFn>(dlsym(lib ? lib : RTLD_DEFAULT, "il2cpp_gchandle_new"));
        getGCHandle = reinterpret_cast<GetGCHandleFn>(dlsym(lib ? lib : RTLD_DEFAULT, "il2cpp_gchandle_get_target"));
        freeGCHandle = reinterpret_cast<FreeGCHandleFn>(dlsym(lib ? lib : RTLD_DEFAULT, "il2cpp_gchandle_free"));
        guardEnabled = newGCHandle && getGCHandle && freeGCHandle;
        LOG_INFO("CustomComponents/CardLife/COORD: GC roots ready=%d", (int)guardEnabled);
        auto install = [](auto method, auto hook, auto& original, const char* label) {
            if (!method.IsValid()) {
                LOG_ERR("CustomComponents/CardLife: method missing %s", label);
                return false;
            }
            BNM::BasicHook(method, hook, original);
            const bool ok = original != nullptr;
            LOG_INFO("CustomComponents/CardLife: hook %s = %d", label, (int)ok);
            return ok;
        };
        bool ok = true;
        ok = install(cardMapper.GetMethod("CreateCardAsync", 4), CreateHook, originalCreate, "CardViewMapper.CreateCardAsync") && ok;
        // No second MapCardView hook: presentation module already owns it.
        ok = install(deleter.GetMethod("DeleteCard", {cardViewInterface.GetCompileTimeClass()}),
                     DeleteHook, originalDelete, "CardViewDeleter.DeleteCard(CardView)") && ok;
        ok = install(deleter.GetMethod("DeleteReplacedCard", 1),
                     DeleteReplacementHook, originalDeleteReplacement, "CardViewDeleter.DeleteReplacedCard") && ok;
        ok = install(death.GetMethod("OnStarted", 0), DeathStartHook,
                     originalDeathStart, "DestroyCardEffectAnimation.OnStarted") && ok;
        ok = install(triggeredDeath.GetMethod("OnStarted", 0), TriggeredDeathStartHook,
                     originalTriggeredDeathStart, "DeathTriggerDestroyCardEffectAnimation.OnStarted") && ok;
        ok = install(death.GetMethod("HandleFrameDieComplete", 2), DeathCompleteHook,
                     originalDeathComplete, "DestroyCardEffectAnimation.HandleFrameDieComplete") && ok;
        ok = install(triggeredDeath.GetMethod("HandleFrameDieComplete", 2), TriggeredDeathCompleteHook,
                     originalTriggeredDeathComplete, "DeathTriggerDestroyCardEffectAnimation.HandleFrameDieComplete") && ok;
        LOG_INFO("CustomComponents/CardLife: %s (tracer + scoped deferred return CreateCardAsync; no gameplay changes)",
                 ok ? "READY" : "PARTIAL HOOK COVERAGE");
        return ok;
    }
} // namespace CustomComponents::Systems::CardViewLifecycleTracer
