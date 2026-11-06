#include "diagnostics/DiagnosticLogging.hpp"
#include "core/HookRegistry.hpp"
#include "pick3/AutomaticChoicePolicy.hpp"
#pragma once
// Discover v1.0b UI port for PickFromThree. Only presentation code was ported.
// PF3 target finding, ChooseTargetBlockingEffect, submission, replication and
// animation queue ownership stay in their original systems.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include "log.h"
#include "diagnostics/Pf3RepairTrace.hpp"
#include "pick3/PickFromThreeCore.hpp"
#include "pick3/PickFromThreeTargetBridge.hpp"
#include "pick3/PickFromThreeSubmission.hpp"
#include "presentation/TargetingPresentationRefreshSystem.hpp"
#include "presentation/DiscardPileViewer.hpp"
#include "presentation/detail/Pick3VisualBarrierTracker.hpp"
#include "presentation/detail/Pick3VisualQueueGate.hpp"
#include "presentation/detail/Pick3CardMetadata.hpp"
#include "core/FrontendTickDispatcher.hpp"
#include "core/PresentationRefreshPolicy.hpp"
namespace CustomComponents::Presentation::PickFromThreeDiscoverPresentation {
    using Obj=BNM::IL2CPP::Il2CppObject;
    struct Vec3 { float x=0,y=0,z=0; Vec3()=default; Vec3(float a,float b,float c):x(a),y(b),z(c){} };
    struct Vec2 { float x=0,y=0; Vec2()=default; Vec2(float a,float b):x(a),y(b){} };
    template<typename T>using MonoList=BNM::Structures::Mono::List<T>;
    inline std::atomic<int> logBudget{1100};
#define PICK3_DISCOVER_LOG(...) do {if(::CustomComponents::Presentation::PickFromThreeDiscoverPresentation::logBudget.fetch_sub(1)>0) CC_SELECTION_DIAG(__VA_ARGS__);}while(0)
    inline constexpr int kMaxOptions=3;
    inline int currentPromptGuid=-1;
    inline bool promptBehindCards=false, suppressStockHint=false;
    inline float promptFontScale=0.80f;
    inline int promptMaxChars=32;
    struct Group {
        Obj* model=nullptr;
        int source=-1;
        int serial=-1;
        int player=-1;
        int from=0;
        std::vector<int> ids;
        std::vector<int> guids;
        std::vector<InspectionStats::Snapshot> stats;
        bool open=false;
        int picked=-1;
    };
    inline std::mutex groupsMutex;
    inline std::vector<Group> groups;
    inline bool FindOpenGroup(int source,Group& out) {
        std::lock_guard<std::mutex> l(groupsMutex);
        for(auto it=groups.rbegin();it!=groups.rend();++it)
            if(it->source==source && it->open){out=*it;return true;}
        return false;
    }
    inline bool IsGroupOpen(int source,int serial) {
        std::lock_guard<std::mutex> l(groupsMutex);
        for(const auto& g:groups) if(g.source==source&&g.serial==serial)return g.open;
        return false;
    }
    inline void CaptureGroupStats(Group& group,Obj* model) {
        group.stats.clear();group.stats.reserve(group.ids.size());
        for(std::size_t i=0;i<group.ids.size();++i) {
            Obj* entity=Engine::EntityAccess::Find(model,group.ids[i]);
            const bool exact=entity && i<group.guids.size() && InspectionStats::Guid(entity)==group.guids[i];
            group.stats.push_back(exact?InspectionStats::Capture(entity,model):InspectionStats::Snapshot{group.guids[i]});
        }
    }
    inline void Publish(const Group& g) {
        std::lock_guard<std::mutex> l(groupsMutex);
        groups.clear();groups.push_back(g);
    }
    inline void CloseGroup(int source,int serial,int picked) {
        std::lock_guard<std::mutex> l(groupsMutex);
        for(auto& g:groups)if(g.source==source&&g.serial==serial){g.open=false;g.picked=picked;}
    }

    namespace Ui {

        struct Bounds { Vec3 center; Vec3 extents; };        // UnityEngine.Bounds
        struct Color { float r, g, b, a; };                   // UnityEngine.Color
        struct Rect { float x, y, width, height; };           // UnityEngine.Rect

        // Layout (fractions of the screen; the game is portrait).
        // Cards a little above the hand's scale (hand 0.26; a new view starts at 0.30), placed by pivot.
        inline constexpr float kCardScale = 0.30f;
        // Card centres, from the bottom (fractions of the screen). The game's "choose a target" prompt is moved to
        // kPromptY while the screen is open, so the column can be tight.
        // Round 30k: tighter and higher, so the bottom button clears the hand (taps there went to hand cards). The card's
        // visible centre is about 0.02 above its pivot.
        inline constexpr float kRowY[kMaxOptions] = {0.69f, 0.515f, 0.34f};
        inline float RowY(int count,int i) { return count==1 ? 0.52f : (count==2 ? (i==0?0.67f:0.36f) : kRowY[std::min(i,kMaxOptions-1)]); }
        inline float promptX = 0.5f, promptY = 0.80f;
        inline constexpr float kButtonBelow = 0.062f;          // button centre below its card's pivot (just under the card)
        inline constexpr float kButtonScale = 0.55f;           // of the "Let's Rock!" button's size
        // Tap areas (fractions of the screen, half sizes): the card opens its card info, the button picks.
        inline constexpr float kCardHalfW = 0.14f, kCardHalfH = 0.058f;
        inline constexpr float kCardCentreAbovePivot = 0.02f;    // for the card's tap area
        inline constexpr float kHiddenShift = 1000.0f;          // world units sideways = off screen
        inline constexpr float kButtonHeight = 0.042f;          // of the screen height
        inline constexpr float kLabelFontSize = 8.0f;           // first guess; the label is then fitted to the button
        inline constexpr int kTmpCenter = 5;                    // TMPro.TextAlignmentOptions.Center (this TMP version)
        inline constexpr float kButtonHalfW = 0.15f, kButtonHalfH = 0.026f;
        inline constexpr float kScrimAlpha = 0.72f;
        inline constexpr int kSortingBoost = 3000;            // added to the cards' sorting orders

        // TargetingPresentationRefreshSystem::RefreshStockPresentation (set by it): re-runs the game's
        // "choose a target" presentation, so the new views get the valid-target highlight.
        inline bool (*refreshPresentation)(Obj* holder, int inputState) = nullptr;

        // Captured from LoadCardsTask.Start.
        inline Obj* cardViewMapper = nullptr;
        inline Obj* rootView = nullptr;

        // Game members
        inline BNM::Field<Obj*> taskMapper, taskRootView, mapperViewMapper, holderTappedSignal;
        inline BNM::Method<void> mapperCreateCardAsync;       // CardViewMapper.CreateCardAsync(CardIdData, Action<CardView>, GameObject, bool)
        inline BNM::Method<void> viewMapperRemove;            // EntityViewMapper.RemoveViewObject(int)
        inline BNM::Method<Obj*> viewMapperGet;               // EntityViewMapper.GetViewObject(int)
        inline BNM::Method<void> cardViewShowInHand;          // CardViewComponent.ShowInHand()
        inline BNM::Method<Vec3> cardViewDesiredScale;        // CardViewComponent.GetDesiredScale()
        // A temporary missing mapper must NOT permanently blacklist a menu.
        // The existing Update loop retries once LoadCardsTask captures it.
        inline int missingMapperLoggedSource = -1, missingMapperLoggedSerial = -1;
        inline BNM::IL2CPP::Il2CppClass* callbackClass = nullptr;   // Action<CardView>
        inline BNM::Class cardIdDataClass;
        inline const BNM::IL2CPP::MethodInfo* callbackMethodInfo = nullptr;

        // Unity members
        inline BNM::Method<Obj*> cameraMain;                  // Camera.main
        inline BNM::Method<bool> cameraOrthographic;
        inline BNM::Method<float> cameraOrthographicSize;
        inline BNM::Method<Vec3> cameraViewportToWorld;       // Camera.ViewportToWorldPoint(Vector3)
        inline BNM::Method<Obj*> componentTransform, componentGameObject;
        inline BNM::Method<Obj*> componentsInChildren;        // Component.GetComponentsInChildren(Type, bool)
        inline BNM::Method<Vec3> transformGetPosition, transformGetLocalScale;
        inline BNM::Method<void> transformSetPosition, transformSetLocalScale;
        inline BNM::Method<void> gameObjectCtor;               // GameObject(string)
        inline BNM::Method<Obj*> gameObjectAddComponent;       // GameObject.AddComponent(Type)
        inline BNM::Method<Obj*> gameObjectTransform;
        inline BNM::Method<int> gameObjectGetLayer;            // GameObject.layer (optional)
        inline BNM::Method<void> gameObjectSetActive;
        inline BNM::Method<Obj*> transformGetParent;
        inline BNM::Method<Obj*> objectInstantiate;            // Object.Instantiate(Object, Transform, bool)
        inline BNM::Method<Obj*> findObjectsOfType;            // Object.FindObjectsOfType(Type, bool)
        inline BNM::Method<Obj*> componentInParent;            // Component.GetComponentInParent(Type)
        inline BNM::Method<int> canvasRenderMode;              // Canvas.renderMode
        inline BNM::Method<Obj*> canvasWorldCamera;            // Canvas.worldCamera
        inline BNM::Method<Vec3> cameraWorldToScreen, cameraScreenToWorld;
        inline BNM::Method<void> tmpSetText, tmpUguiSetText;   // TMPro.TextMeshPro / TextMeshProUGUI .text
        inline BNM::Field<Obj*> mulliganDoneButton;            // MulliganViewComponent.DoneButton
        inline BNM::Method<void> behaviourSetEnabled;          // Behaviour.enabled (set)
        inline BNM::Method<bool> behaviourGetEnabled;
        inline BNM::Method<bool> gameObjectActiveInHierarchy;
        inline BNM::MonoType* imageType = nullptr;              // UnityEngine.UI.Image
        inline BNM::Method<Obj*> imageGetSprite, tmpUguiGetFont, spriteRendererGetSprite;
        inline BNM::Method<BNM::Structures::Mono::String*> objectGetName;
        inline BNM::Method<void> tmpSetFont, tmpSetFontSize, tmpSetAlignment, tmpSetColor, tmpSetWrap;
        inline BNM::Method<Rect> spriteGetRect;
        inline BNM::Method<float> spriteGetPPU;
        inline BNM::Field<Obj*> hintToolTip;                    // TargetToolTipHintViewComponent.ToolTip
        inline BNM::Field<Vec3> tipStatic, tipTarget;           // CardToolTipViewComponent.StaticPosition / TargetPosition
        inline BNM::Method<void> transformSetEuler;            // Transform.eulerAngles (set)
        inline BNM::Method<float> timeDeltaTime;               // Time.deltaTime
        inline BNM::Field<Obj*> mulliganDrawProps;             // MulliganViewComponent.DrawCardAnimProperties
        inline BNM::Field<Vec3> drawInitialPosition, drawInitialScale;
        inline BNM::Field<float> drawInitialRotation, drawAnimationTime, drawTimeBetween;
        inline BNM::Method<Obj*> componentGet;                 // Component.GetComponent(Type)
        inline BNM::MonoType* overlayViewType = nullptr;       // PvZCards.CardInspection.CardInspectionOverlayView
        inline BNM::Method<void> canvasSetOverride, canvasSetOrder, canvasSetLayer, groupSetAlpha, groupSetIgnoreParents;
        inline BNM::Method<int> canvasGetOrder;
        inline BNM::MonoType *animatorType = nullptr, *canvasGroupType = nullptr, *promptViewType = nullptr;
        inline BNM::Field<Obj*> factoryOfMapper, binderOfFactory;  // CardViewMapper.<CardViewFactory>, CardViewFactory.<Binder>
        // Set by DiscardPileViewer: the card-info overlay (ShowCardInspectionOverlaySignal) over a list of cards.
        inline bool (*showCards)(Obj* signal, const int* ids, const int* guids, int count, int initial, const InspectionStats::Snapshot* stats) = nullptr;
        inline BNM::MonoType* showSignalType = nullptr;
        inline BNM::MonoType *mulliganViewType = nullptr, *tmpType = nullptr, *tmpUguiType = nullptr, *canvasType = nullptr;
        inline BNM::Method<void> gameObjectSetLayer;
        inline BNM::Method<void> objectDestroy;                // Object.Destroy(Object)
        inline BNM::Method<Bounds> rendererBounds;
        inline BNM::Method<bool> rendererEnabled;
        inline BNM::Method<int> rendererGetOrder, rendererGetLayer;
        inline BNM::Method<void> rendererSetOrder, rendererSetLayer;
        inline BNM::Method<int> groupGetOrder, groupGetLayer;  // UnityEngine.Rendering.SortingGroup (optional)
        inline BNM::Method<void> groupSetOrder, groupSetLayer;
        inline BNM::Method<void> spriteRendererSetSprite, spriteRendererSetColor;
        inline BNM::Method<Color> spriteRendererGetColor;
        inline BNM::Method<Obj*> spriteCreate;                 // Sprite.Create(Texture2D, Rect, Vector2, float)
        inline BNM::Method<Obj*> whiteTexture;                 // Texture2D.whiteTexture
        inline BNM::MonoType *rendererType = nullptr, *sortingGroupType = nullptr, *spriteRendererType = nullptr;
        inline bool ready = false;

        inline uintptr_t (*gcNew)(Obj*, bool) = nullptr;
        inline void (*gcFree)(uintptr_t) = nullptr;

        struct Slot {
            int id = -1;                 // Gameplay entity ID, -1 for CardDatabase cards.
            int renderId = -1;           // Unique CardViewMapper key; NEVER sent to gameplay.
            int guid = -1;
            InspectionStats::Snapshot stats;
            Obj* callback = nullptr;      // our Action<CardView>
            uintptr_t callbackHandle = 0; // pin pending async callback until it fires or closes
            Obj* view = nullptr;          // CardViewComponent
            uintptr_t handle = 0;
            Obj* button = nullptr;        // GameObject: the button's background sprite
            uintptr_t buttonHandle = 0;
            Obj* outline = nullptr;       // GameObject: a darker box behind the plain button (no game sprite found)
            uintptr_t outlineHandle = 0;
            Obj* label = nullptr;         // GameObject: its "Choose" TextMeshPro
            uintptr_t labelHandle = 0;
            bool labelFitted = false;
            bool loaded = false;
            // Root view pins these child renderers throughout the exit.
            std::vector<std::pair<Obj*,Color>> exitSprites;
            Vec3 exitButtonAt{},exitLabelAt{},exitOutlineAt{};
        };

        struct State {
            bool shown = false;
            int source = -1;
            int serial = -1;
            Obj* holder = nullptr;
            Slot slots[kMaxOptions];
            int count = 0;
            int loaded = 0;
            bool laidOut = false;
            bool refreshed = false;
            Obj* scrim = nullptr;         // GameObject
            uintptr_t scrimHandle = 0;
            int sortingLayer = 0;
            int layer = -1;               // the card views' GameObject layer (the camera may draw only some layers)
            int lowestOrder = 0;
            float worldPerScreenX = 0, worldPerScreenY = 0, screenLeft = 0, screenBottom = 0;
            bool inspecting = false;      // the card-info overlay is open: our objects are hidden
            bool away = false;            // shifted away (SetAllActive(false))
            bool buttonsShown = false;    // buttons moved in (after the entrance)
            // Entrance (the mulligan's own draw animation values)
            bool animating = false;
            // Presentation-only exit choreography; NEVER gates service dispatch.
            bool exiting = false;
            // Submission is independent of presentation loading. If dispatch
            // beats CardViewMapper callbacks, keep only a pending VISUAL exit.
            bool deferredExit = false;
            int deferredExitChoice = -1;
            unsigned deferredExitFrames = 0;
            unsigned exitRenderedFrames = 0;
            int exitChosen = -1;
            float exitClock = 0;
            float exitDuration = 0.82f;
            float clock = 0;
            Vec3 from[kMaxOptions], to[kMaxOptions];
            float fromScale = 0.1f, fromAngle = 0, duration = 0.35f, stagger = 0.1f;
            Obj* scrimRenderer = nullptr;
            float buttonHalfW = 0.12f, buttonWorldHeight = 1;
            bool plainButtons = false;    // no game sprite: boxes sized to the label
            Obj* tip = nullptr;           // the prompt's CardToolTipViewComponent (moved by its fields every frame)
            uintptr_t tipHandle = 0;
            Vec3 tipStaticWas;
            Vec3 tipTargetWas;
            int tipFrames = 0;
            bool overlaySeen = false;
            float inspectFrames = 0;
            Vec3 promptAt;
            Obj* prompt = nullptr;        // Transform of the game's "choose a target" prompt, moved while open
            uintptr_t promptHandle = 0;
            Vec3 promptWas;
            Obj* customPrompt = nullptr;
            uintptr_t customPromptHandle = 0;
            bool localizedPrompt = false;
            bool secondaryTargetActive = false;
            unsigned promptMapperVersion = 0;
        };

        inline State state;
        struct RetiredCallback { Obj* callback; uintptr_t handle; int renderId; };
        inline std::vector<RetiredCallback> retiredCallbacks;

        // All PF3 option previews need their own view-mapper identity, even
        // when a physical card has a real gameplay ID. The real card can be
        // mapped independently while this preview is still exit-animating.
        // CardDatabase options also require this: repeated entityId=-1 used
        // to coalesce callbacks. These negative IDs stay presentation-only.
        inline int nextVirtualRenderId = -1000000;
        inline int AllocateVirtualRenderId() {
            if (nextVirtualRenderId <= -2000000000) nextVirtualRenderId = -1000000;
            return nextVirtualRenderId--;
        }

        template<typename T> inline std::vector<T> ArrayItems(BNM::Structures::Mono::Array<T>* array) {
            std::vector<T> out;
            if (!array) return out;
            for (BNM::IL2CPP::il2cpp_array_size_t i = 0; i < array->capacity; i++) out.push_back(array->m_Items[i]);
            return out;
        }

        inline std::vector<Obj*> Children(Obj* component, BNM::MonoType* type, bool includeInactive = true) {
            if (!component || !type) return {};
            auto* array = reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(componentsInChildren[component](type, includeInactive));
            return ArrayItems(array);
        }

        inline Obj* Pin(Obj* object, uintptr_t& handle) {
            if (object && gcNew && !handle) handle = gcNew(object, false);
            return object;
        }

        inline void Unpin(uintptr_t& handle) {
            if (handle && gcFree) gcFree(handle);
            handle = 0;
        }

        // Union of the enabled renderers' world bounds of a card view. False when it has none yet.
        inline bool WorldBounds(Obj* view, Bounds& out) {
            bool any = false;
            float minX = 0, minY = 0, maxX = 0, maxY = 0, z = 0;
            for (Obj* renderer : Children(view, rendererType, false)) {     // active objects only
                if (!renderer || (rendererEnabled.IsValid() && !rendererEnabled[renderer]())) continue;
                const Bounds b = rendererBounds[renderer]();
                if (b.extents.x <= 0.0001f && b.extents.y <= 0.0001f) continue;
                const float x0 = b.center.x - b.extents.x, x1 = b.center.x + b.extents.x;
                const float y0 = b.center.y - b.extents.y, y1 = b.center.y + b.extents.y;
                if (!any) { minX = x0; maxX = x1; minY = y0; maxY = y1; z = b.center.z; any = true; }
                else { minX = std::min(minX, x0); maxX = std::max(maxX, x1); minY = std::min(minY, y0); maxY = std::max(maxY, y1); }
            }
            if (!any) return false;
            out.center = Vec3((minX + maxX) / 2, (minY + maxY) / 2, z);
            out.extents = Vec3((maxX - minX) / 2, (maxY - minY) / 2, 0);
            return true;
        }

        inline Vec3 Viewport(Obj* camera, float x, float y, float depth) {
            return cameraViewportToWorld[camera](Vec3(x, y, depth));
        }

        // Puts every renderer (and sorting group) of the view above the game's own, and remembers the lowest order.
        inline void RaiseSorting(Obj* view, bool first) {
            for (Obj* renderer : Children(view, rendererType)) {
                if (!renderer) continue;
                const int order = rendererGetOrder[renderer]() + kSortingBoost;
                rendererSetOrder[renderer](order);
                if (first) { state.sortingLayer = rendererGetLayer[renderer](); state.lowestOrder = order; first = false; }
                state.lowestOrder = std::min(state.lowestOrder, order);
            }
            if (sortingGroupType) {
                for (Obj* group : Children(view, sortingGroupType)) {
                    if (!group) continue;
                    const int order = groupGetOrder[group]() + kSortingBoost;
                    groupSetOrder[group](order);
                    state.lowestOrder = std::min(state.lowestOrder, order);
                }
            }
        }

        inline void MakeScrim(Obj* camera, float depth) {
            Obj* texture = whiteTexture[static_cast<Obj*>(nullptr)]();
            if (!texture) return;
            Obj* sprite = spriteCreate[static_cast<Obj*>(nullptr)](texture, Rect{0, 0, 4, 4}, Vec2(0.5f, 0.5f), 4.0f);
            Obj* go = BNM::Class("UnityEngine", "GameObject").CreateNewInstance();
            if (!sprite || !go) return;
            gameObjectCtor[go](BNM::CreateMonoString("SprigDiscoverScrim"));
            if (state.layer >= 0 && gameObjectSetLayer.IsValid()) gameObjectSetLayer[go](state.layer);
            Obj* renderer = gameObjectAddComponent[go](spriteRendererType);
            if (!renderer) return;
            spriteRendererSetSprite[renderer](sprite);
            state.scrimRenderer = renderer;
            spriteRendererSetColor[renderer](Color{0, 0, 0, kScrimAlpha});
            rendererSetLayer[renderer](state.sortingLayer);
            rendererSetOrder[renderer](state.lowestOrder - 1);
            const Vec3 a = Viewport(camera, 0, 0, depth), b = Viewport(camera, 1, 1, depth);
            Obj* transform = gameObjectTransform[go]();
            transformSetPosition[transform](Vec3((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2 + 0.5f));
            transformSetLocalScale[transform](Vec3(std::fabs(b.x - a.x) * 1.1f, std::fabs(b.y - a.y) * 1.1f, 1));
            state.scrim = Pin(go, state.scrimHandle);
        }

        // ---- "Choose" buttons: copies of the mulligan's "Let's Rock!" button (MulliganViewComponent.DoneButton).
        inline Obj* FindMulliganView() {
            if (!mulliganViewType || !findObjectsOfType.IsValid()) return nullptr;
            auto* array = reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(
                findObjectsOfType[static_cast<Obj*>(nullptr)](mulliganViewType, true));
            for (Obj* view : ArrayItems(array)) if (view) return view;
            return nullptr;
        }

        inline void SetTexts(Obj* gameObject, const char* text) {
            // GetComponentsInChildren is a Component method: call it on the Transform, never on the GameObject itself
            // (round 30e crashed natively doing that, phone log 2026-10-21 04:38).
            Obj* root = gameObject ? gameObjectTransform[gameObject]() : nullptr;
            if (!root) return;
            int set = 0;
            for (BNM::MonoType* type : {tmpType, tmpUguiType}) {
                if (!type) continue;
                auto& setter = type == tmpType ? tmpSetText : tmpUguiSetText;
                if (!setter.IsValid()) continue;
                for (Obj* label : Children(root, type)) if (label) { setter[label](BNM::CreateMonoString(text)); set++; }
            }
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: button text set on %d label(s)", set);
        }

        // World point on the main camera's plane -> the same screen spot for the button's own canvas.
        inline void PlaceButton(Obj* button, Obj* camera, const Vec3& world) {
            Obj* transform = gameObjectTransform[button]();
            if (!transform) return;
            const Vec3 before = transformGetPosition[transform]();
            Obj* canvas = canvasType && componentInParent.IsValid() ? componentInParent[transform](canvasType) : nullptr;
            const int mode = canvas && canvasRenderMode.IsValid() ? canvasRenderMode[canvas]() : -1;
            Obj* canvasCamera = canvas && mode == 1 && canvasWorldCamera.IsValid() ? canvasWorldCamera[canvas]() : nullptr;
            Vec3 at = world;
            if (mode == 0 && cameraWorldToScreen.IsValid()) {
                const Vec3 screen = cameraWorldToScreen[camera](world);           // overlay canvas: pixels
                at = Vec3(screen.x, screen.y, before.z);
            } else if (canvasCamera && canvasCamera != camera && cameraWorldToScreen.IsValid() && cameraScreenToWorld.IsValid()) {
                const Vec3 screen = cameraWorldToScreen[camera](world);
                Obj* camTransform = componentTransform[canvasCamera]();
                const Vec3 camPos = camTransform ? transformGetPosition[camTransform]() : Vec3();
                at = cameraScreenToWorld[canvasCamera](Vec3(screen.x, screen.y, std::fabs(before.z - camPos.z)));
            } else {
                at = Vec3(world.x, world.y, before.z);
            }
            transformSetPosition[transform](at);
            const Vec3 scale = transformGetLocalScale[transform]();
            transformSetLocalScale[transform](Vec3(scale.x * kButtonScale, scale.y * kButtonScale, scale.z));
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: button at (%.2f, %.2f, %.2f) (was (%.2f, %.2f, %.2f); canvas mode %d, own camera %s)",
                               at.x, at.y, at.z, before.x, before.y, before.z, mode, canvasCamera ? "yes" : "no");
        }

        // The copied "Let's Rock!" button keeps the state the mulligan left it in (animated out) and draws in the
        // mulligan's canvas, below our cards and scrim. Freeze its animator, undo faded groups, and give it its own
        // canvas sorted above the cards.
        inline void MakeVisible(Obj* copy) {
            Obj* root = copy ? gameObjectTransform[copy]() : nullptr;
            if (!root) return;
            int animators = 0, groups = 0;
            if (animatorType && behaviourSetEnabled.IsValid())
                for (Obj* a : Children(root, animatorType)) if (a) { behaviourSetEnabled[a](false); animators++; }
            if (canvasGroupType && groupSetAlpha.IsValid())
                for (Obj* g : Children(root, canvasGroupType)) if (g) { groupSetAlpha[g](1.0f); groups++; }
            const Vec3 scale = transformGetLocalScale[root]();
            if (std::fabs(scale.x) < 0.05f || std::fabs(scale.y) < 0.05f) transformSetLocalScale[root](Vec3(1, 1, 1));
            bool ownCanvas = false;
            int wasEnabled = -1;
            if (canvasType && canvasSetOverride.IsValid() && canvasSetOrder.IsValid()) {
                // The button already carries its own Canvas (round 30g: "Can't add component 'Canvas'"): use it.
                Obj* canvas = componentGet.IsValid() ? componentGet[root](canvasType) : nullptr;
                if (!canvas) canvas = gameObjectAddComponent[copy](canvasType);
                if (canvas) {
                    if (behaviourGetEnabled.IsValid()) wasEnabled = behaviourGetEnabled[canvas]() ? 1 : 0;
                    if (behaviourSetEnabled.IsValid()) behaviourSetEnabled[canvas](true);
                    canvasSetOverride[canvas](true);
                    if (canvasSetLayer.IsValid()) canvasSetLayer[canvas](state.sortingLayer);
                    canvasSetOrder[canvas](state.lowestOrder + 500);
                    ownCanvas = true;
                }
            }
            if (canvasGroupType && groupSetAlpha.IsValid() && groupSetIgnoreParents.IsValid()) {
                Obj* group = gameObjectAddComponent[copy](canvasGroupType);
                if (group) { groupSetAlpha[group](1.0f); groupSetIgnoreParents[group](true); }
            }
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: button: scale was (%.2f, %.2f), %d animator(s) stopped, %d group(s) shown, canvas %s (was enabled %d)",
                               scale.x, scale.y, animators, groups, ownCanvas ? "sorted above the cards" : "missing", wasEnabled);
        }

        inline Obj* FindFirst(BNM::MonoType* type, bool includeInactive = true) {
            if (!type || !findObjectsOfType.IsValid()) return nullptr;
            auto* array = reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(
                findObjectsOfType[static_cast<Obj*>(nullptr)](type, includeInactive));
            for (Obj* o : ArrayItems(array)) if (o) return o;
            return nullptr;
        }

        // The game's "Choose a card." prompt goes to the top while the screen is open.
        // The visible "Choose a card." box is the hint view's CardToolTipViewComponent, which places itself every frame
        // from its StaticPosition / TargetPosition fields (rounds 30g-30i moved transforms: no effect). Write those.
        inline void MoveTip() {
            if (!state.laidOut || !promptViewType || !hintToolTip.IsValid() || !tipStatic.IsValid() || !tipTarget.IsValid()) return;
            if (!state.tip) {
                if (++state.tipFrames % 10 != 1) return;           // look every 10 frames until it shows
                Obj* hint = FindFirst(promptViewType, false);
                Obj* tip = hint ? hintToolTip[hint].Get() : nullptr;
                if (!tip) return;
                state.tip = Pin(tip, state.tipHandle);
                state.tipStaticWas = tipStatic[tip].Get();
                state.tipTargetWas = tipTarget[tip].Get();
                const Vec3 target = state.tipTargetWas;
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: prompt box found (static (%.2f, %.2f, %.2f), target (%.2f, %.2f, %.2f))",
                                   state.tipStaticWas.x, state.tipStaticWas.y, state.tipStaticWas.z, target.x, target.y, target.z);
            }
            const float y = state.screenBottom + (suppressStockHint ? -10.f : promptY) * state.worldPerScreenY;
            Vec3 st = state.tipStaticWas;
            st.y = y;
            tipStatic[state.tip] = st;
            Vec3 tg = tipTarget[state.tip].Get();
            tg.y = y;
            tipTarget[state.tip] = tg;
        }

        // Only an ACTIVE prompt (round 30h moved an inactive copy; the visible one stayed in the middle). Retried
        // every frame from OnUpdate until found.
        inline void MovePrompt(Obj* camera, float depth) {
            if (state.prompt) return;
            Obj* view = FindFirst(promptViewType, false);
            Obj* transform = view ? componentTransform[view]() : nullptr;
            if (!transform) return;
            state.prompt = Pin(transform, state.promptHandle);
            state.promptWas = transformGetPosition[transform]();
            const Vec3 at = Viewport(camera, promptX, promptY, depth);
            state.promptAt = Vec3(state.promptWas.x, at.y, state.promptWas.z);
            transformSetPosition[transform](state.promptAt);
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: prompt moved from y %.2f to %.2f", state.promptWas.y, at.y);
        }

        // The stock ShowCardInspectionOverlaySignal: the one an inspect command used, else from the game's binder.
        inline Obj* ShowSignal() {
            Obj* factory = cardViewMapper && factoryOfMapper.IsValid() ? factoryOfMapper[cardViewMapper].Get() : nullptr;
            Obj* binder = factory && binderOfFactory.IsValid() ? binderOfFactory[factory].Get() : nullptr;
            if (!binder || !showSignalType) return nullptr;
            BNM::Method<Obj*> getBinding = BNM::Class(binder).GetMethod("GetBinding", {std::string_view("key")});
            BNM::Method<Obj*> getInstance = BNM::Class(binder).GetMethod("GetInstance", {std::string_view("key")});
            if (!getBinding.IsValid() || !getInstance.IsValid() || !getBinding[binder](showSignalType)) return nullptr;
            return getInstance[binder](showSignalType);
        }

        // Our cards, scrim and buttons away / back (they draw above everything, so they leave while the card info is
        // open). Moved sideways, not switched off: switching a card view off and on resets its animator and lost the
        // card frame (round 30h).
        inline void Shift(Obj* transform, float dx) {
            if (!transform) return;
            const Vec3 p = transformGetPosition[transform]();
            transformSetPosition[transform](Vec3(p.x + dx, p.y, p.z));
        }
        inline void SetAllActive(bool on) {
            if (state.away == !on) return;
            state.away = !on;
            const float dx = on ? -kHiddenShift : kHiddenShift;
            for (int i = 0; i < state.count; i++) {
                Slot& slot = state.slots[i];
                if (slot.view) Shift(componentTransform[slot.view](), dx);
                if (slot.button && state.buttonsShown) Shift(gameObjectTransform[slot.button](), dx);
                if (slot.label && state.buttonsShown) Shift(gameObjectTransform[slot.label](), dx);
                if (slot.outline && state.buttonsShown) Shift(gameObjectTransform[slot.outline](), dx);
            }
            if (state.scrim) Shift(gameObjectTransform[state.scrim](), dx);
        }

        inline bool OverlayOpen() {
            if (!overlayViewType || !findObjectsOfType.IsValid()) return false;
            auto* array = reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(
                findObjectsOfType[static_cast<Obj*>(nullptr)](overlayViewType, false));     // active ones only
            for (Obj* o : ArrayItems(array)) if (o) return true;
            return false;
        }

        // Our own Choose buttons (round 30j; copies of "Let's Rock!" never drew, rounds 30e-30i): a SpriteRenderer with
        // the Let's Rock button's own sprite and a TextMeshPro label in the button's own font, both in world space next
        // to the cards and sorted above them.
        inline Obj* NewObject(const char* name) {
            Obj* go = BNM::Class("UnityEngine", "GameObject").CreateNewInstance();
            if (!go) return nullptr;
            gameObjectCtor[go](BNM::CreateMonoString(name));
            if (state.layer >= 0 && gameObjectSetLayer.IsValid()) gameObjectSetLayer[go](state.layer);
            return go;
        }

        inline void MakeButtons(Obj* camera, float depth) {
            Obj* mulligan = FindMulliganView();
            Obj* done = mulligan && mulliganDoneButton.IsValid() ? mulliganDoneButton[mulligan].Get() : nullptr;
            Obj* root = done ? componentTransform[done]() : nullptr;
            Obj* sprite = nullptr;
            float bestArea = 0;
            // Every sprite under the Let's Rock button (UI Images and SpriteRenderers), logged; the largest wins.
            int images = 0, spriteRenderers = 0;
            auto consider = [&](Obj* candidate, const char* kind) {
                if (!candidate) return;
                const Rect r = spriteGetRect[candidate]();
                auto* name = objectGetName.IsValid() ? objectGetName[candidate]() : nullptr;
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: Let's Rock sprite (%s) '%s' %.0fx%.0f", kind, name ? name->str().c_str() : "?", r.width, r.height);
                if (r.width * r.height > bestArea) { bestArea = r.width * r.height; sprite = candidate; }
            };
            if (root && imageType && imageGetSprite.IsValid())
                for (Obj* image : Children(root, imageType)) if (image) { images++; consider(imageGetSprite[image](), "image"); }
            if (root && spriteRendererGetSprite.IsValid())
                for (Obj* r : Children(root, spriteRendererType)) if (r) { spriteRenderers++; consider(spriteRendererGetSprite[r](), "sprite renderer"); }
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: Let's Rock button %p: %d image(s), %d sprite renderer(s) (Image class %s)",
                               root, images, spriteRenderers, imageType ? "found" : "missing");
            Obj* font = nullptr;
            if (root && tmpUguiType && tmpUguiGetFont.IsValid())
                for (Obj* label : Children(root, tmpUguiType)) if (label && (font = tmpUguiGetFont[label]())) break;
            state.plainButtons = !sprite;
            if (!sprite) {   // fallback: a plain green box, widened to its label once the label is measured
                Obj* white = whiteTexture[static_cast<Obj*>(nullptr)]();
                sprite = white ? spriteCreate[static_cast<Obj*>(nullptr)](white, Rect{0, 0, 4, 4}, Vec2(0.5f, 0.5f), 4.0f) : nullptr;
            }
            const Rect rect = sprite ? spriteGetRect[sprite]() : Rect{0, 0, 4, 4};
            const float ppu = sprite && spriteGetPPU.IsValid() ? spriteGetPPU[sprite]() : 100.0f;
            const float spriteHeight = rect.height / (ppu > 0 ? ppu : 100.0f), spriteWidth = rect.width / (ppu > 0 ? ppu : 100.0f);
            const float wantHeight = kButtonHeight * state.worldPerScreenY;
            const float s = spriteHeight > 0.0001f ? wantHeight / spriteHeight : 1;
            state.buttonHalfW = spriteWidth * s / 2 / std::max(0.01f, state.worldPerScreenX);
            state.buttonWorldHeight = wantHeight;
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: Choose buttons: sprite %s %.0fx%.0f px at %.0f px/unit -> scale %.2f (%.2f x %.2f world); font %s",
                               bestArea > 0 ? "from Let's Rock" : "plain box", rect.width, rect.height, ppu, s,
                               spriteWidth * s, wantHeight, font ? "from Let's Rock" : "default");

            for (int i = 0; i < state.count; i++) {
                Slot& slot = state.slots[i];
                if (!slot.view) continue;
                const Vec3 at = Viewport(camera, 0.5f, RowY(state.count,i) - kButtonBelow, depth);
                const float z = state.to[i].z - 0.2f;
                if (state.plainButtons && sprite) {
                    Obj* edge = NewObject("SprigDiscoverChooseEdge");
                    Obj* edgeRenderer = edge ? gameObjectAddComponent[edge](spriteRendererType) : nullptr;
                    if (edgeRenderer) {
                        spriteRendererSetSprite[edgeRenderer](sprite);
                        spriteRendererSetColor[edgeRenderer](Color{0.05f, 0.30f, 0.05f, 1});
                        rendererSetLayer[edgeRenderer](state.sortingLayer);
                        rendererSetOrder[edgeRenderer](state.lowestOrder + 499);
                        Obj* t = gameObjectTransform[edge]();
                        transformSetLocalScale[t](Vec3(s + 0.12f, s + 0.12f, 1));
                        transformSetPosition[t](Vec3(at.x + kHiddenShift, at.y, z + 0.05f));
                        slot.outline = Pin(edge, slot.outlineHandle);
                    }
                }
                Obj* bg = NewObject("SprigDiscoverChoose");
                Obj* bgRenderer = bg ? gameObjectAddComponent[bg](spriteRendererType) : nullptr;
                if (bgRenderer && sprite) {
                    spriteRendererSetSprite[bgRenderer](sprite);
                    if (bestArea <= 0) spriteRendererSetColor[bgRenderer](Color{0.22f, 0.72f, 0.12f, 1});
                    rendererSetLayer[bgRenderer](state.sortingLayer);
                    rendererSetOrder[bgRenderer](state.lowestOrder + 500);
                    Obj* t = gameObjectTransform[bg]();
                    transformSetLocalScale[t](Vec3(s, s, 1));
                    transformSetPosition[t](Vec3(at.x + kHiddenShift, at.y, z));
                    slot.button = Pin(bg, slot.buttonHandle);
                }
                Obj* text = NewObject("SprigDiscoverChooseText");
                Obj* tmp = text && tmpType ? gameObjectAddComponent[text](tmpType) : nullptr;
                if (tmp) {
                    if (font && tmpSetFont.IsValid()) tmpSetFont[tmp](font);
                    if (tmpSetWrap.IsValid()) tmpSetWrap[tmp](false);
                    if (tmpSetAlignment.IsValid()) tmpSetAlignment[tmp](kTmpCenter);
                    if (tmpSetFontSize.IsValid()) tmpSetFontSize[tmp](kLabelFontSize);
                    if (tmpSetColor.IsValid()) tmpSetColor[tmp](Color{1, 1, 1, 1});
                    tmpSetText[tmp](BNM::CreateMonoString("Choose"));
                    Obj* t = gameObjectTransform[text]();
                    Obj* labelRenderer = componentGet.IsValid() ? componentGet[t](rendererType) : nullptr;
                    if (labelRenderer) {
                        rendererSetLayer[labelRenderer](state.sortingLayer);
                        rendererSetOrder[labelRenderer](state.lowestOrder + 501);
                    }
                    transformSetPosition[t](Vec3(at.x + kHiddenShift, at.y, z - 0.1f));
                    slot.label = Pin(text, slot.labelHandle);
                }
            }
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: Choose buttons made (labels %s)", tmpType ? "TextMeshPro" : "missing");
        }

        // Localized PF3 targeting string, keyed by the ACTIVATING card prefab.
        // This is presentation-only: no changes to target IDs or stock input.
        // Wrap localized PF3 instructions ourselves as well as enabling TMP word wrap.
        // A world-space TextMeshPro object has no guaranteed UI RectTransform width;
        // explicit line breaks keep long localization strings inside phone viewports.
        inline std::string WrapTargetText(const std::string& input) {
            const int limit=std::clamp(promptMaxChars,12,60);
            std::string output;
            int column=0;
            size_t i=0;
            while(i<input.size()) {
                if(input[i]=='\n') { output+='\n'; column=0; ++i; continue; }
                while(i<input.size() && input[i]==' ') ++i;
                if(i>=input.size()) break;
                size_t end=i;
                while(end<input.size() && input[end]!=' ' && input[end]!='\n') ++end;
                // Count UTF-8 codepoints rather than bytes for non-English keys.
                int units=0;
                for(size_t j=i;j<end;++j) if((static_cast<unsigned char>(input[j])&0xc0)!=0x80) ++units;
                if(column && column+1+units>limit) { output+='\n'; column=0; }
                else if(column) { output+=' '; ++column; }
                // Also split an exceptionally long unspaced token. Never
                // split in the middle of a UTF-8 encoded codepoint.
                for(size_t j=i;j<end;) {
                    size_t next=j+1;
                    while(next<end && (static_cast<unsigned char>(input[next])&0xc0)==0x80) ++next;
                    if(column>=limit) { output+='\n'; column=0; }
                    output.append(input,j,next-j);
                    ++column;
                    j=next;
                }
                i=end;
            }
            return output;
        }
        inline void UpdateCustomPrompt() {
            if (!state.shown || !state.laidOut) return;
            if (state.secondaryTargetActive) return;
            // Update once per mapper generation. A missing translation no
            // longer triggers a managed scene scan or allocation every frame.
            if (state.customPrompt &&
                (state.localizedPrompt || state.promptMapperVersion == Pick3CardMetadata::localizationVersion)) return;
            Obj* mapper = Pick3CardMetadata::FindLocalizationMapper();
            std::string text = "Choose a card", key;
            const bool localized = currentPromptGuid > 0 && mapper &&
                Pick3CardMetadata::TryPickFromThreeTargeting(currentPromptGuid,mapper,text,&key);
            if(!state.customPrompt) {
                Obj* camera=cameraMain[static_cast<Obj*>(nullptr)]();
                if(!camera || !tmpType) return;
                Obj* go=NewObject("SprigPick3TargetingPrompt");
                Obj* tmp=go && gameObjectAddComponent.IsValid()?gameObjectAddComponent[go](tmpType):nullptr;
                if(!tmp)return;
                if(tmpSetAlignment.IsValid())tmpSetAlignment[tmp](kTmpCenter);
                if(tmpSetFontSize.IsValid())tmpSetFontSize[tmp](20.f*promptFontScale);
                if(tmpSetColor.IsValid())tmpSetColor[tmp](Color{1,1,1,1});
                if(tmpSetWrap.IsValid())tmpSetWrap[tmp](true);
                Obj* transform=gameObjectTransform[go]();
                if(!transform)return;
                const Vec3 cameraPosition=transformGetPosition[componentTransform[camera]() ]();
                const float depth=std::fabs(cameraPosition.z)>0.01f?std::fabs(cameraPosition.z):10.f;
                const Vec3 at=Viewport(camera,promptX,promptY,depth);
                transformSetPosition[transform](Vec3(at.x,at.y,state.to[0].z-0.35f));
                transformSetLocalScale[transform](Vec3(0.35f,0.35f,0.35f));
                Obj* renderer=componentGet.IsValid()?componentGet[transform](rendererType):nullptr;
                if(renderer) {
                    rendererSetLayer[renderer](state.sortingLayer);
                    rendererSetOrder[renderer](state.lowestOrder+(promptBehindCards?-3:700));
                }
                state.customPrompt=Pin(go,state.customPromptHandle);
            }
            Obj* transform=state.customPrompt?gameObjectTransform[state.customPrompt]():nullptr;
            if(!transform)return;
            for(Obj* label:Children(transform,tmpType))if(label && tmpSetText.IsValid())
                tmpSetText[label](BNM::CreateMonoString(WrapTargetText(text)));
            state.localizedPrompt=localized;
            state.promptMapperVersion=Pick3CardMetadata::localizationVersion;
            if(localized) CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: TARGETING_LOC guid=%d key=%s",currentPromptGuid,key.c_str());
        }

        // The mulligan's own draw-in values (MulliganViewComponent.DrawCardAnimProperties), with safe fallbacks.
        inline void ReadEntrance(Obj* camera, float depth) {
            const Vec3 below = Viewport(camera, 0.5f, -0.15f, depth);
            state.from[0] = below;
            state.fromScale = 0.1f;
            state.fromAngle = 0;
            state.duration = 0.35f;
            state.stagger = 0.1f;
            Obj* mulligan = FindMulliganView();
            Obj* props = mulligan && mulliganDrawProps.IsValid() ? mulliganDrawProps[mulligan].Get() : nullptr;
            if (props) {
                const Vec3 p = drawInitialPosition[props].Get();
                const Vec3 sc = drawInitialScale[props].Get();
                const float time = drawAnimationTime[props].Get(), gap = drawTimeBetween[props].Get();
                state.from[0] = Vec3(p.x, p.y, p.z);
                if (sc.x > 0.001f) state.fromScale = sc.x;
                state.fromAngle = drawInitialRotation[props].Get();
                if (time > 0.05f && time < 3) state.duration = time;
                if (gap >= 0 && gap < 2) state.stagger = gap;
            }
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: entrance from (%.2f, %.2f) scale %.2f angle %.1f, %.2f s each, %.2f s apart (%s)",
                               state.from[0].x, state.from[0].y, state.fromScale, state.fromAngle, state.duration, state.stagger,
                               props ? "the mulligan's values" : "fallback");
        }

        inline void Hide(const char* why);
        inline float EaseOutCubic(float t) { const float u = 1 - t; return 1 - u * u * u; }
        inline float Smooth(float t) { t=std::clamp(t,0.f,1.f);return t*t*(3-2*t); }
        inline void SpriteOpacity(Slot& slot,float alpha) {
            if (!spriteRendererSetColor.IsValid()) return;
            for (const auto& entry:slot.exitSprites) if (entry.first) {
                Color c=entry.second;c.a*=std::clamp(alpha,0.f,1.f);
                spriteRendererSetColor[entry.first](c);
            }
        }
        inline void BeginExit(int index) {
            if (!state.shown || index<0 || index>=state.count || state.exiting) {
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: EXIT_REJECT shown=%d laidOut=%d index=%d count=%d exiting=%d",
                    int(state.shown),int(state.laidOut),index,state.count,int(state.exiting));
                return;
            }
            if (!state.laidOut) {
                state.deferredExit=true;
                state.deferredExitChoice=index;
                state.deferredExitFrames=0;
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: EXIT_DEFERRED effect=%d group=%d loaded=%d/%d selected=%d",
                    state.source,state.serial,state.loaded,state.count,index);
                return;
            }
            state.animating=false;
            state.exiting=true;
            state.exitChosen=index;
            state.exitClock=0;
            for (int i=0;i<state.count;i++) {
                Slot& slot=state.slots[i];
                slot.exitSprites.clear();
                // Capture actual shader tints once. Never force all card layers white.
                if (spriteRendererGetColor.IsValid() && spriteRendererType && slot.view)
                    for (Obj* spr:Children(slot.view,spriteRendererType)) if (spr)
                        slot.exitSprites.push_back({spr,spriteRendererGetColor[spr]()});
                if (slot.button && gameObjectTransform.IsValid())
                    slot.exitButtonAt=transformGetPosition[gameObjectTransform[slot.button]()]();
                if (slot.label && gameObjectTransform.IsValid())
                    slot.exitLabelAt=transformGetPosition[gameObjectTransform[slot.label]()]();
                if (slot.outline && gameObjectTransform.IsValid())
                    slot.exitOutlineAt=transformGetPosition[gameObjectTransform[slot.outline]()]();
            }
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: EXIT_BEGIN choice=%d cards=%d duration=%.2f independentOfSubmit=1",
                               index,state.count,state.exitDuration);
        }
        inline void AnimateExit(float dt) {
            if (!state.shown || !state.exiting) return;
            state.exitClock+=std::clamp(dt,0.001f,0.08f);
            ++state.exitRenderedFrames;
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
            if(state.exitRenderedFrames==1 || state.exitRenderedFrames==8 || state.exitRenderedFrames==16 || state.exitRenderedFrames==24) {
                Obj* chosen=state.exitChosen>=0 && state.exitChosen<state.count?state.slots[state.exitChosen].view:nullptr;
                Obj* transform=chosen && componentTransform.IsValid()?componentTransform[chosen]():nullptr;
                const Vec3 position=transform&&transformGetPosition.IsValid()?transformGetPosition[transform]():Vec3{};
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: EXIT_FRAME effect=%d group=%d frame=%u clock=%.3f chosen=%d view=%p at=%.2f,%.2f,%.2f layout=%d",
                    state.source,state.serial,state.exitRenderedFrames,state.exitClock,state.exitChosen,(void*)chosen,
                    position.x,position.y,position.z,int(state.laidOut));
            }
#endif
            const float t=std::clamp(state.exitClock,0.f,state.exitDuration);
            const float side=Smooth(t/0.48f);
            const float buttonEase=Smooth(t/0.21f);
            const float selectedMove=Smooth(t/0.35f);
            const float selectedFade=Smooth((t-0.43f)/0.34f);
            const float centerY=state.to[std::min(state.count-1,1)].y;
            int outgoingRank=0;
            for(int i=0;i<state.count;i++) {
                Slot& slot=state.slots[i];
                Obj* trans=slot.view?componentTransform[slot.view]():nullptr;
                const bool chosen=(i==state.exitChosen);
                // When the first/last card is selected the other two must
                // STILL fly to opposite sides, rather than both to the same.
                const float direction=chosen ? (i<state.count/2?-1.f:1.f)
                                              : ((outgoingRank++%2)==0?-1.f:1.f);
                const float cardOpacity=chosen?1.f-selectedFade:1.f-side;
                if(trans) {
                    const Vec3 a=state.to[i];
                    if(chosen) {
                        const float pop=std::sin(3.14159265f*std::clamp(t/0.50f,0.f,1.f));
                        const float shrink=1.f-Smooth((t-0.52f)/0.30f);
                        const float scale=kCardScale*(1.f+0.22f*pop)*shrink;
                        transformSetPosition[trans](Vec3(a.x, a.y+(centerY-a.y)*selectedMove,a.z));
                        transformSetLocalScale[trans](Vec3(scale,scale,scale));
                    } else {
                        const float scale=kCardScale*(1.f-0.30f*side);
                        transformSetPosition[trans](Vec3(a.x+direction*state.worldPerScreenX*0.66f*side,
                                                         a.y+0.16f*state.worldPerScreenY*side,a.z));
                        transformSetLocalScale[trans](Vec3(scale,scale,scale));
                    }
                }
                SpriteOpacity(slot,cardOpacity);
                // Buttons travel off screen and fade much faster than the
                // selected card. Text alpha is explicit for TextMeshPro.
                const float buttonDx=direction*state.worldPerScreenX*0.8f*buttonEase;
                if(slot.button) {
                    Obj* tr=gameObjectTransform[slot.button]();
                    transformSetPosition[tr](Vec3(slot.exitButtonAt.x+buttonDx,slot.exitButtonAt.y,slot.exitButtonAt.z));
                    if(spriteRendererType && spriteRendererSetColor.IsValid())
                        for(Obj* spr:Children(tr,spriteRendererType)) if(spr)
                            spriteRendererSetColor[spr](Color{1,1,1,1.f-buttonEase});
                }
                if(slot.outline) {
                    Obj* tr=gameObjectTransform[slot.outline]();
                    transformSetPosition[tr](Vec3(slot.exitOutlineAt.x+buttonDx,slot.exitOutlineAt.y,slot.exitOutlineAt.z));
                    if(spriteRendererType && spriteRendererSetColor.IsValid())
                        for(Obj* spr:Children(tr,spriteRendererType)) if(spr)
                            spriteRendererSetColor[spr](Color{0.12f,0.12f,0.12f,1.f-buttonEase});
                }
                if(slot.label) {
                    Obj* tr=gameObjectTransform[slot.label]();
                    transformSetPosition[tr](Vec3(slot.exitLabelAt.x+buttonDx,slot.exitLabelAt.y,slot.exitLabelAt.z));
                    if(tmpSetColor.IsValid() && tmpType)
                        for(Obj* label:Children(tr,tmpType)) if(label)
                            tmpSetColor[label](Color{1,1,1,1.f-buttonEase});
                }
            }
            if(state.scrimRenderer && spriteRendererSetColor.IsValid())
                spriteRendererSetColor[state.scrimRenderer](Color{0,0,0,kScrimAlpha*(1.f-Smooth((t-0.27f)/0.40f))});
            if(state.customPrompt && tmpType && tmpSetColor.IsValid())
                for(Obj* label:Children(gameObjectTransform[state.customPrompt](),tmpType)) if(label)
                    tmpSetColor[label](Color{1,1,1,state.secondaryTargetActive?0.f:1.f-Smooth(t/0.24f)});
            if(t>=state.exitDuration){
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: EXIT_COMPLETE selected=%d",state.exitChosen);
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: EXIT_REPORT effect=%d group=%d frames=%u duration=%.3f selected=%d",
                    state.source,state.serial,state.exitRenderedFrames,state.exitClock,state.exitChosen);
                Hide("exit animation completed");
            }
        }
        inline void TickExit() {
            if(state.exiting) AnimateExit(timeDeltaTime.IsValid()?timeDeltaTime[static_cast<Obj*>(nullptr)]():1.f/30.f);
            else if(state.deferredExit && ++state.deferredExitFrames>=120) {
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: EXIT_LOAD_TIMEOUT effect=%d group=%d loaded=%d/%d noGameplayChange=1",
                    state.source,state.serial,state.loaded,state.count);
                Hide("deferred visual exit timed out");
            }
        }

        inline float EaseOutBack(float t) { const float c1 = 1.4f, c3 = c1 + 1; const float u = t - 1; return 1 + c3 * u * u * u + c1 * u * u; }

        // One frame of the entrance; true when every card has landed.
        inline bool Animate(float dt) {
            state.clock += dt;
            bool done = true;
            for (int i = 0; i < state.count; i++) {
                Slot& slot = state.slots[i];
                Obj* transform = slot.view ? componentTransform[slot.view]() : nullptr;
                if (!transform) continue;
                float t = (state.clock - i * state.stagger) / state.duration;
                t = std::clamp(t, 0.0f, 1.0f);
                if (t < 1) done = false;
                const float e = EaseOutCubic(t), b = EaseOutBack(t);
                const Vec3& a = state.from[i];
                const Vec3& z = state.to[i];
                const float shift = state.away ? kHiddenShift : 0;
                transformSetPosition[transform](Vec3(a.x + (z.x - a.x) * e + shift, a.y + (z.y - a.y) * e, z.z));
                const float sc = state.fromScale + (kCardScale - state.fromScale) * b;
                transformSetLocalScale[transform](Vec3(sc, sc, sc));
                if (transformSetEuler.IsValid()) transformSetEuler[transform](Vec3(0, 0, state.fromAngle * (1 - e)));
            }
            if (state.scrimRenderer) {
                const float f = std::clamp(state.clock / std::max(0.05f, state.duration), 0.0f, 1.0f);
                spriteRendererSetColor[state.scrimRenderer](Color{0, 0, 0, kScrimAlpha * f});
            }
            return done;
        }

        // Called when all views have loaded: column layout at hand-card size, sorting, scrim, Choose buttons.
        inline void BeginExit(int index);
        inline void LayOut() {
            Obj* camera = cameraMain[static_cast<Obj*>(nullptr)]();
            if (!camera) { LOG_ERR("CustomComponents/Pick3/DiscoverUI: no main camera"); return; }
            Obj* cameraTransform = componentTransform[camera]();
            const Vec3 cameraPosition = cameraTransform ? transformGetPosition[cameraTransform]() : Vec3(0, 0, -10);
            const float depth = std::fabs(cameraPosition.z) > 0.01f ? std::fabs(cameraPosition.z) : 10.0f;
            const Vec3 bottom = Viewport(camera, 0.5f, 0, depth), top = Viewport(camera, 0.5f, 1, depth);
            const Vec3 left = Viewport(camera, 0, 0.5f, depth), right = Viewport(camera, 1, 0.5f, depth);
            state.worldPerScreenY = std::fabs(top.y - bottom.y);
            state.worldPerScreenX = std::fabs(right.x - left.x);
            state.screenBottom = std::min(bottom.y, top.y);
            state.screenLeft = std::min(left.x, right.x);
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: camera (%.2f, %.2f, %.2f) size %.2f; screen %.2f x %.2f world units",
                               cameraPosition.x, cameraPosition.y, cameraPosition.z, cameraOrthographicSize[camera](),
                               state.worldPerScreenX, state.worldPerScreenY);

            ReadEntrance(camera, depth);
            bool first = true;
            for (int i = 0; i < state.count; i++) {
                Slot& slot = state.slots[i];
                if (!slot.view) continue;
                Obj* transform = componentTransform[slot.view]();
                if (!transform) continue;
                if (first && gameObjectGetLayer.IsValid()) state.layer = gameObjectGetLayer[componentGameObject[slot.view]()]();
                // Final: a little above the hand's card scale, by the card's own pivot. Start: the mulligan's draw start.
                const Vec3 now = transformGetPosition[transform]();
                const float row = RowY(state.count,i);
                const Vec3 target = Viewport(camera, 0.5f, row, depth);
                state.to[i] = Vec3(target.x, target.y, now.z);
                state.from[i] = Vec3(state.from[0].x, state.from[0].y, now.z);
                transformSetLocalScale[transform](Vec3(state.fromScale, state.fromScale, state.fromScale));
                transformSetPosition[transform](state.from[i]);
                if (transformSetEuler.IsValid()) transformSetEuler[transform](Vec3(0, 0, state.fromAngle));
                RaiseSorting(slot.view, first);
                first = false;
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: card %d at (%.2f, %.2f), scale %.2f", slot.id, target.x, target.y, kCardScale);
            }
            MakeScrim(camera, depth);
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: scrim %s", state.scrim ? "made" : "missing");
            if (state.scrimRenderer) spriteRendererSetColor[state.scrimRenderer](Color{0, 0, 0, 0});
            state.animating = true;
            state.clock = 0;
            // (MovePrompt retired: moving the hint view's transform has no visible effect; MoveTip does it.)
            MakeButtons(camera, depth);
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: laid out %d card(s); sorting layer %d, lowest order %d, object layer %d; scrim %s",
                               state.count, state.sortingLayer, state.lowestOrder, state.layer, state.scrim ? "made" : "missing");
            state.laidOut = true;
            if (state.deferredExit) {
                const int deferredChoice=state.deferredExitChoice;
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: EXIT_LATE_LAYOUT effect=%d group=%d loaded=%d/%d choice=%d",
                    state.source,state.serial,state.loaded,state.count,deferredChoice);
                state.deferredExit=false;
                BeginExit(deferredChoice);
            }
        }

        // HandleBoardTappedCommand.PickTarget (DiscardPileViewer's hook, first thing): a tap inside a slot's band
        // (its card and its Choose button) picks that option. True = handled, *picked = the option's entity id.
        inline BNM::Field<Vec3> tapPosition;          // HandleBoardTappedCommand.<TapPosition>
        inline int ChooseAt(Obj* command) {
            if (!command || !state.shown || !state.laidOut || state.animating ||
                state.exiting || state.inspecting || !tapPosition.IsValid()) return -1;
            const Vec3 tap=tapPosition[command].Get();
            const float fx=state.worldPerScreenX>0?(tap.x-state.screenLeft)/state.worldPerScreenX:-1;
            const float fy=state.worldPerScreenY>0?(tap.y-state.screenBottom)/state.worldPerScreenY:-1;
            for (int i=0;i<state.count;i++) if(state.slots[i].guid>0 &&
                std::fabs(fx-0.5f)<=std::max(kButtonHalfW,state.buttonHalfW) &&
                std::fabs(fy-(RowY(state.count,i)-kButtonBelow))<=kButtonHalfH) return i;
            return -1;
        }
        inline bool PickAt(Obj* command, int* picked) {
            if (!ready || !state.shown || !command) return false;
            // Crucial: while the card views are still loading, block the stock
            // board/hero target underneath our overlay. The previous early
            // return forwarded those taps to the stock selection handler.
            if (!state.laidOut || !tapPosition.IsValid()) {
                if (picked) *picked = -1;
                return true;
            }
            if (state.animating || state.exiting) { *picked = -1; return true; }
            if (state.inspecting) {
                // A board tap means the overlay no longer covers the screen: bring the screen back, swallow this tap.
                state.inspecting = false;
                SetAllActive(true);
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: card info closed (board tap)");
                *picked = -1;
                return true;
            }
            const Vec3 tap = tapPosition[command].Get();
            // Fractions of the screen (the tap is a world point of the main camera, like the board's).
            const float fx = state.worldPerScreenX > 0 ? (tap.x - state.screenLeft) / state.worldPerScreenX : -1;
            const float fy = state.worldPerScreenY > 0 ? (tap.y - state.screenBottom) / state.worldPerScreenY : -1;
            int button = ChooseAt(command), card = -1;
            for (int i = 0; i < state.count; i++) {
                if (state.slots[i].guid <= 0) continue;
                if (std::fabs(fx - 0.5f) <= std::max(kButtonHalfW, state.buttonHalfW) && std::fabs(fy - (RowY(state.count,i) - kButtonBelow)) <= kButtonHalfH) button = i;
                else if (std::fabs(fx - 0.5f) <= kCardHalfW && std::fabs(fy - (RowY(state.count,i) + kCardCentreAbovePivot)) <= kCardHalfH) card = i;
            }
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: tap at screen (%.2f, %.2f) -> %s %d",
                               fx, fy, button >= 0 ? "Choose button of" : card >= 0 ? "card info of" : "nothing,",
                               button >= 0 ? state.slots[button].id : card >= 0 ? state.slots[card].id : -1);
            if (button >= 0) {
                *picked = button;
                return true;
            }
            if (card >= 0) {
                int ids[kMaxOptions], guids[kMaxOptions], n = 0, initial = 0;
                InspectionStats::Snapshot stats[kMaxOptions];
                for (int i = 0; i < state.count; i++) {
                    if (state.slots[i].guid <= 0) continue;
                    if (i == card) initial = n;
                    ids[n] = state.slots[i].id;
                    guids[n] = state.slots[i].guid;
                    stats[n] = state.slots[i].stats;
                    n++;
                }
                const bool shown = showCards && showCards(ShowSignal(), ids, guids, n, initial, stats);
                if (!shown) PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: card info could not be opened (no inspection signal)");
                else {
                    state.inspecting = true;
                    state.overlaySeen = false;
                    state.inspectFrames = 0;
                    SetAllActive(false);
                }
                *picked = -1;                 // swallow the tap: reading is not picking
                return true;
            }
            *picked = -1;                     // a tap elsewhere does nothing while the screen is open
            return true;
        }

        inline void DestroyObject(Obj* object) {
            if (object) objectDestroy[static_cast<Obj*>(nullptr)](object);
        }

        // Restore native target hints at the instant the holder advances to a
        // subsequent target phase. The card exit can continue independently.
        inline void SetSecondaryTargetMode(bool secondary) {
            if (!state.shown || state.secondaryTargetActive == secondary) return;
            state.secondaryTargetActive = secondary;
            if (secondary) {
                // Secondary / additional targeting belongs to stock UI. Its
                // tip must return immediately, not after the 0.82s card exit.
                if (state.customPrompt && tmpType && tmpSetColor.IsValid())
                    for (Obj* label:Children(gameObjectTransform[state.customPrompt](),tmpType))
                        if (label) tmpSetColor[label](Color{1,1,1,0});
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: SECONDARY_STOCK_PROMPT source=%d group=%d",state.source,state.serial);
            }
        }
        inline void RestoreStockTargetPrompt() {
            if (state.tip && tipStatic.IsValid()) {
                tipStatic[state.tip] = state.tipStaticWas;
                if (tipTarget.IsValid()) tipTarget[state.tip] = state.tipTargetWas;
                Unpin(state.tipHandle);
                state.tip = nullptr;
            }
            if (state.prompt && transformSetPosition.IsValid()) {
                transformSetPosition[state.prompt](state.promptWas);
                Unpin(state.promptHandle);
                state.prompt = nullptr;
            }
        }
        inline void Hide(const char* why) {
            if (!state.shown) return;
            Obj* viewMapper = cardViewMapper ? mapperViewMapper[cardViewMapper].Get() : nullptr;
            for (int i = 0; i < state.count; i++) {
                Slot& slot = state.slots[i];
                if (slot.view) {
                    if (viewMapper && viewMapperGet[viewMapper](slot.renderId) == slot.view)
                        viewMapperRemove[viewMapper](slot.renderId);
                    DestroyObject(componentGameObject[slot.view]());
                }
                if (slot.button) DestroyObject(slot.button);
                Unpin(slot.buttonHandle);
                if (slot.label) DestroyObject(slot.label);
                Unpin(slot.labelHandle);
                if (slot.outline) DestroyObject(slot.outline);
                Unpin(slot.outlineHandle);
                Unpin(slot.handle);
                if (slot.callback && !slot.loaded && slot.callbackHandle) {
                    retiredCallbacks.push_back({slot.callback,slot.callbackHandle,slot.renderId});
                    slot.callbackHandle=0;
                } else Unpin(slot.callbackHandle);
                slot = Slot{};
            }
            if (state.scrim) DestroyObject(state.scrim);
            Unpin(state.scrimHandle);
            if (state.customPrompt) DestroyObject(state.customPrompt);
            Unpin(state.customPromptHandle);
            if (state.prompt) transformSetPosition[state.prompt](state.promptWas);
            Unpin(state.promptHandle);
            if (state.tip) {
                tipStatic[state.tip] = state.tipStaticWas;
                if (tipTarget.IsValid()) tipTarget[state.tip] = state.tipTargetWas;
            }
            Unpin(state.tipHandle);
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: screen for card %d #%d closed (%s)", state.source, state.serial, why);
            state = State{};
        }

        inline void OnPicked(int source, int serial) {
            if (state.shown && state.source == source && state.serial == serial)
                Hide("picked");
        }

        // Action<CardView> callback (invoked by the CardViewMapper closure: invoke_impl(invoke_impl_this, view, method)).
        static void OnCardLoaded(Obj* self, Obj* view, const void*) {
            for (auto it=retiredCallbacks.begin();it!=retiredCallbacks.end();++it) {
                if(it->callback!=self) continue;
                Obj* mapper=cardViewMapper ? mapperViewMapper[cardViewMapper].Get() : nullptr;
                if(mapper && viewMapperGet[mapper](it->renderId)==view)
                    viewMapperRemove[mapper](it->renderId);
                if(view) DestroyObject(componentGameObject[view]());
                Unpin(it->handle);
                retiredCallbacks.erase(it);
                return;
            }
            if (!state.shown) {
                // Too late: the screen closed while loading. Throw the view away.
                if (view) DestroyObject(componentGameObject[view]());
                return;
            }
            for (int i = 0; i < state.count; i++) {
                Slot& slot = state.slots[i];
                if (slot.callback != self || slot.loaded) continue;
                slot.loaded = true;
                slot.view = Pin(view, slot.handle);
                if (view) {
                    cardViewShowInHand[view]();
                    auto frame=BNM::Class(view).GetField("Frame").cast<Obj*>();
                    if(frame.IsValid()) CardStats::Apply(frame[view].Get(),slot.stats.stats);
                }
                state.loaded++;
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: VIEW_LOADED gameplayId=%d renderId=%d guid=%d loaded=%d/%d view=%p",
                                   slot.id, slot.renderId, slot.guid, state.loaded, state.count, static_cast<void*>(view));
                break;
            }
            if (state.loaded >= state.count && !state.laidOut) LayOut();
        }

        // A managed Action<CardView> whose invoke goes to OnCardLoaded (layout of Il2CppDelegate, Unity 2022.2: the
        // CardViewMapper closure calls [d+0x18](d[+0x40], view, d[+0x28])).
        inline Obj* MakeCallback() {
            if (!callbackClass) return nullptr;
            Obj* object = BNM::Class(callbackClass).CreateNewInstance();
            if (!object) return nullptr;
            auto* d = reinterpret_cast<BNM::IL2CPP::Il2CppDelegate*>(object);
            d->method_ptr = reinterpret_cast<BNM::IL2CPP::Il2CppMethodPointer>(&OnCardLoaded);
            d->invoke_impl = reinterpret_cast<BNM::IL2CPP::Il2CppMethodPointer>(&OnCardLoaded);
            d->target = object;
            d->method = callbackMethodInfo;
            d->invoke_impl_this = object;
            return object;
        }

        inline void Show(Obj* holder, const Group& group) {
            if (!cardViewMapper) {
                if (missingMapperLoggedSource!=group.source || missingMapperLoggedSerial!=group.serial) {
                    missingMapperLoggedSource=group.source;
                    missingMapperLoggedSerial=group.serial;
                    LOG_WARN("CustomComponents/Pick3/REPAIR1719: VIEW_MAPPER_WAIT effect=%d group=%d retryOnLoader=1 noPermanentBlacklist=1",
                             group.source,group.serial);
                }
                return;
            }
            if(state.shown) Hide("replaced by new request");
            state = State{};
            state.shown = true;
            state.source = group.source;
            state.serial = group.serial;
            state.holder = holder;
            state.count = std::min<int>(static_cast<int>(group.ids.size()), kMaxOptions);
            Obj* viewMapper = mapperViewMapper[cardViewMapper].Get();
            PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: screen for card %d #%d: %d option(s); engine model %p, view mapper model %p",
                               group.source, group.serial, state.count, group.model,
                               viewMapper ? BNM::Class("PvZCards.Game", "EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>()[viewMapper].Get() : nullptr);
            for (int i = 0; i < state.count; i++) {
                Slot& slot = state.slots[i];
                slot.id = group.ids[i];
                slot.guid = group.guids[i];
                if(std::size_t(i)<group.stats.size()) slot.stats=group.stats[i];
                slot.renderId = AllocateVirtualRenderId(); // Never reserve the real card's view-map key.
                slot.callback = Pin(MakeCallback(),slot.callbackHandle);
                Obj* data = cardIdDataClass.CreateNewObjectParameters(slot.renderId, slot.guid);
                PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: VIEW_REQUEST option=%d gameplayId=%d renderId=%d guid=%d",
                                   i, slot.id, slot.renderId, slot.guid);
                if (!slot.callback || !data) {
                    LOG_ERR("CustomComponents/Pick3/DiscoverUI: could not make the view request for card %d", slot.id);
                    continue;
                }
                mapperCreateCardAsync[cardViewMapper](data, slot.callback, rootView, false);
            }
        }

        // GameInputStateHolder.Update (after the original and TargetingPresentationRefresh).
        inline void OnUpdate(Obj* holder, bool choosing, int source, int serial) {
            if (!ready) return;
            if (state.exiting || state.deferredExit) return; // The visual exit can outlive stock targeting.
            if (state.shown) {
                if (!choosing || state.source != source || state.serial != serial ||
                    !IsGroupOpen(state.source, state.serial)) {
                    Hide("stock choice left or new effect");
                    return;
                }
                if (state.buttonsShown) {
                    for (int i=0;i<state.count;i++) {
                        Slot& slot=state.slots[i];
                        if (!slot.label || slot.labelFitted) continue;
                        Obj* t=gameObjectTransform[slot.label]();
                        Obj* r=componentGet.IsValid() && t ? componentGet[t](rendererType) : nullptr;
                        if (!r) { slot.labelFitted=true;continue; }
                        const Bounds b=rendererBounds[r]();
                        if (b.extents.y<=0.0001f) continue;
                        const float factor=(state.buttonWorldHeight*0.5f)/(b.extents.y*2);
                        const Vec3 sc=transformGetLocalScale[t]();
                        transformSetLocalScale[t](Vec3(sc.x*factor,sc.y*factor,sc.z));
                        slot.labelFitted=true;
                        if (state.plainButtons && slot.button) {
                            const float width=b.extents.x*2*factor*1.35f;
                            transformSetLocalScale[gameObjectTransform[slot.button]()](Vec3(width,state.buttonWorldHeight,1));
                            if(slot.outline)transformSetLocalScale[gameObjectTransform[slot.outline]()](Vec3(width+0.12f,state.buttonWorldHeight+0.12f,1));
                            state.buttonHalfW=width/2/std::max(0.01f,state.worldPerScreenX);
                        }
                    }
                }
                MoveTip();
                UpdateCustomPrompt();
                if (state.prompt && state.laidOut) transformSetPosition[state.prompt](state.promptAt);
                if (state.animating) {
                    const float dt=timeDeltaTime.IsValid()?timeDeltaTime[static_cast<Obj*>(nullptr)]():1.f/30.f;
                    if(Animate(dt)) {
                        state.animating=false;
                        for(int i=0;i<state.count;i++) {
                            if(state.slots[i].button) Shift(gameObjectTransform[state.slots[i].button](),state.away?0:-kHiddenShift);
                            if(state.slots[i].label) Shift(gameObjectTransform[state.slots[i].label](),state.away?0:-kHiddenShift);
                            if(state.slots[i].outline) Shift(gameObjectTransform[state.slots[i].outline](),state.away?0:-kHiddenShift);
                        }
                        state.buttonsShown=true;
                    }
                }
                if(state.inspecting) {
                    const bool open=OverlayOpen();
                    state.overlaySeen|=open;
                    state.inspectFrames+=1;
                    if((state.overlaySeen && !open)||(!state.overlaySeen && state.inspectFrames>300)) {
                        state.inspecting=false;
                        SetAllActive(true);
                    }
                }
                return;
            }
            if(!choosing) return;
            Group group;
            if(FindOpenGroup(source,group) && group.serial==serial)
                Show(holder,group);
        }

        // LoadCardsTask.Start: the game's own card loader (every draw / board card goes through it).
        using StartFn = Obj* (*)(void*, const void*);
        inline StartFn originalStart = nullptr;
        static Obj* StartHook(void* self, const void* method) {
            if (self) {
                Obj* mapper = taskMapper[static_cast<Obj*>(self)].Get();
                Obj* root = taskRootView[static_cast<Obj*>(self)].Get();
                if (mapper && mapper != cardViewMapper) {
                    cardViewMapper = mapper;
                    rootView = root;
                    PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: card loader found (CardViewMapper %p, root view %p)", mapper, root);
                }
            }
            return originalStart(self, method);
        }

        inline bool Initialize() {
            const char* game = "PvZCards.Game";
            auto task = BNM::Class(game, "LoadCardsTask");
            auto mapper = BNM::Class(game, "CardViewMapper");
            auto viewMapper = BNM::Class(game, "EntityViewMapper");
            auto cardView = BNM::Class(game, "CardViewComponent");
            auto holder = BNM::Class(game, "GameInputStateHolder");
            cardIdDataClass = BNM::Class("PvZCards.Engine.Records", "CardIdData");
            auto camera = BNM::Class("UnityEngine", "Camera");
            auto component = BNM::Class("UnityEngine", "Component");
            auto transform = BNM::Class("UnityEngine", "Transform");
            auto gameObject = BNM::Class("UnityEngine", "GameObject");
            auto object = BNM::Class("UnityEngine", "Object");
            auto renderer = BNM::Class("UnityEngine", "Renderer");
            auto spriteRenderer = BNM::Class("UnityEngine", "SpriteRenderer");
            auto sprite = BNM::Class("UnityEngine", "Sprite");
            auto texture2d = BNM::Class("UnityEngine", "Texture2D");
            auto sortingGroup = BNM::Class("UnityEngine.Rendering", "SortingGroup");

            bool ok = task.IsValid() && mapper.IsValid() && viewMapper.IsValid() && cardView.IsValid() &&
                      holder.IsValid() && cardIdDataClass.IsValid() && camera.IsValid() && component.IsValid() &&
                      transform.IsValid() && gameObject.IsValid() && object.IsValid() && renderer.IsValid() &&
                      spriteRenderer.IsValid() && sprite.IsValid() && texture2d.IsValid();
            if (!ok) { LOG_ERR("CustomComponents/Pick3/DiscoverUI: screen classes missing"); return false; }

            taskMapper = task.GetField("<CardViewMapper>k__BackingField");
            taskRootView = task.GetField("<RootView>k__BackingField");
            mapperViewMapper = mapper.GetField("<ViewMapper>k__BackingField");
            holderTappedSignal = holder.GetField("<EntityTappedSignal>k__BackingField");
            auto createCardAsync = mapper.GetMethod("CreateCardAsync", 4);
            mapperCreateCardAsync = createCardAsync.cast<void>();
            viewMapperRemove = viewMapper.GetMethod("RemoveViewObject", {std::string_view("viewId")}).cast<void>();
            viewMapperGet = viewMapper.GetMethod("GetViewObject", {std::string_view("entityId")}).cast<Obj*>();
            cardViewShowInHand = cardView.GetMethod("ShowInHand", 0).cast<void>();
            cardViewDesiredScale = cardView.GetMethod("GetDesiredScale", 0).cast<Vec3>();

            cameraMain = camera.GetMethod("get_main", 0).cast<Obj*>();
            cameraOrthographic = camera.GetMethod("get_orthographic", 0).cast<bool>();
            cameraOrthographicSize = camera.GetMethod("get_orthographicSize", 0).cast<float>();
            cameraViewportToWorld = camera.GetMethod("ViewportToWorldPoint", {std::string_view("position")}).cast<Vec3>();
            componentTransform = component.GetMethod("get_transform", 0).cast<Obj*>();
            componentGameObject = component.GetMethod("get_gameObject", 0).cast<Obj*>();
            componentsInChildren = component.GetMethod("GetComponentsInChildren", {std::string_view("t"), std::string_view("includeInactive")}).cast<Obj*>();
            transformGetPosition = transform.GetMethod("get_position", 0).cast<Vec3>();
            transformSetPosition = transform.GetMethod("set_position", 1).cast<void>();
            transformGetLocalScale = transform.GetMethod("get_localScale", 0).cast<Vec3>();
            transformSetLocalScale = transform.GetMethod("set_localScale", 1).cast<void>();
            gameObjectCtor = gameObject.GetMethod(".ctor", {std::string_view("name")}).cast<void>();
            gameObjectAddComponent = gameObject.GetMethod("AddComponent", {std::string_view("componentType")}).cast<Obj*>();
            gameObjectTransform = gameObject.GetMethod("get_transform", 0).cast<Obj*>();
            gameObjectGetLayer = gameObject.GetMethod("get_layer", 0).cast<int>();
            gameObjectSetLayer = gameObject.GetMethod("set_layer", 1).cast<void>();
            // Choose buttons (optional: without them a tap on the card still picks).
            gameObjectSetActive = gameObject.GetMethod("SetActive", 1).cast<void>();
            transformGetParent = transform.GetMethod("get_parent", 0).cast<Obj*>();
            objectInstantiate = object.GetMethod("Instantiate", {std::string_view("original"), std::string_view("parent"), std::string_view("instantiateInWorldSpace")}).cast<Obj*>();
            findObjectsOfType = object.GetMethod("FindObjectsOfType", {std::string_view("type"), std::string_view("includeInactive")}).cast<Obj*>();
            componentInParent = component.GetMethod("GetComponentInParent", {std::string_view("t")}).cast<Obj*>();
            cameraWorldToScreen = camera.GetMethod("WorldToScreenPoint", {std::string_view("position")}).cast<Vec3>();
            cameraScreenToWorld = camera.GetMethod("ScreenToWorldPoint", {std::string_view("position")}).cast<Vec3>();
            {
                auto canvas = BNM::Class("UnityEngine", "Canvas");
                if (canvas.IsValid()) {
                    canvasType = canvas.GetMonoType();
                    canvasRenderMode = canvas.GetMethod("get_renderMode", 0).cast<int>();
                    canvasWorldCamera = canvas.GetMethod("get_worldCamera", 0).cast<Obj*>();
                }
                auto tmp = BNM::Class("TMPro", "TextMeshPro"), ugui = BNM::Class("TMPro", "TextMeshProUGUI");
                if (tmp.IsValid()) { tmpType = tmp.GetMonoType(); tmpSetText = tmp.GetMethod("set_text", 1).cast<void>(); }
                if (ugui.IsValid()) { tmpUguiType = ugui.GetMonoType(); tmpUguiSetText = ugui.GetMethod("set_text", 1).cast<void>(); }
                auto mulligan = BNM::Class("PvZCards.Game", "MulliganViewComponent");
                if (mulligan.IsValid()) { mulliganViewType = mulligan.GetMonoType(); mulliganDoneButton = mulligan.GetField("DoneButton"); }
                auto behaviour = BNM::Class("UnityEngine", "Behaviour");
                if (behaviour.IsValid()) {
                    behaviourSetEnabled = behaviour.GetMethod("set_enabled", 1).cast<void>();
                    behaviourGetEnabled = behaviour.GetMethod("get_enabled", 0).cast<bool>();
                }
                componentGet = component.GetMethod("GetComponent", {std::string_view("type")}).cast<Obj*>();
                gameObjectActiveInHierarchy = gameObject.GetMethod("get_activeInHierarchy", 0).cast<bool>();
                transformSetEuler = transform.GetMethod("set_eulerAngles", 1).cast<void>();
                {
                    auto image = BNM::Class("UnityEngine.UI", "Image");
                    if (image.IsValid()) { imageType = image.GetMonoType(); imageGetSprite = image.GetMethod("get_sprite", 0).cast<Obj*>(); }
                    auto ugui = BNM::Class("TMPro", "TextMeshProUGUI");
                    if (ugui.IsValid()) tmpUguiGetFont = ugui.GetMethod("get_font", 0).cast<Obj*>();
                    auto tmp = BNM::Class("TMPro", "TextMeshPro");
                    if (tmp.IsValid()) {
                        tmpSetFont = tmp.GetMethod("set_font", 1).cast<void>();
                        tmpSetFontSize = tmp.GetMethod("set_fontSize", 1).cast<void>();
                        tmpSetAlignment = tmp.GetMethod("set_alignment", 1).cast<void>();
                        tmpSetColor = tmp.GetMethod("set_color", 1).cast<void>();
                        tmpSetWrap = tmp.GetMethod("set_enableWordWrapping", 1).cast<void>();
                    }
                    spriteGetRect = sprite.GetMethod("get_rect", 0).cast<Rect>();
                    spriteRendererGetSprite = spriteRenderer.GetMethod("get_sprite", 0).cast<Obj*>();
                    spriteRendererGetColor = spriteRenderer.GetMethod("get_color", 0).cast<Color>();
                    objectGetName = object.GetMethod("get_name", 0).cast<BNM::Structures::Mono::String*>();
                    spriteGetPPU = sprite.GetMethod("get_pixelsPerUnit", 0).cast<float>();
                    auto hint = BNM::Class("PvZCards.Game", "TargetToolTipHintViewComponent");
                    auto tipClass = BNM::Class("PvZCards.Game", "CardToolTipViewComponent");
                    if (hint.IsValid() && tipClass.IsValid()) {
                        hintToolTip = hint.GetField("ToolTip");
                        tipStatic = tipClass.GetField("StaticPosition");
                        tipTarget = tipClass.GetField("TargetPosition");
                    }
                }
                {
                    auto time = BNM::Class("UnityEngine", "Time");
                    if (time.IsValid()) timeDeltaTime = time.GetMethod("get_deltaTime", 0).cast<float>();
                    auto draw = BNM::Class("PvZCards.Game", "LocalDrawCardForMulliganAnimationProperties");
                    if (draw.IsValid()) {
                        drawInitialPosition = draw.GetField("InitialPosition");
                        drawInitialScale = draw.GetField("InitialScale");
                        drawInitialRotation = draw.GetField("InitialRotationDegrees");
                        drawAnimationTime = draw.GetField("AnimationTime");
                        drawTimeBetween = draw.GetField("TimeBetweenEachCard");
                    }
                    auto mulliganClass = BNM::Class("PvZCards.Game", "MulliganViewComponent");
                    if (mulliganClass.IsValid() && drawInitialPosition.IsValid() && drawInitialScale.IsValid() &&
                        drawInitialRotation.IsValid() && drawAnimationTime.IsValid() && drawTimeBetween.IsValid())
                        mulliganDrawProps = mulliganClass.GetField("DrawCardAnimProperties");
                }
                {
                    auto overlay = BNM::Class("PvZCards.CardInspection", "CardInspectionOverlayView");
                    if (!overlay.IsValid()) overlay = BNM::Class("PvZCards.Game", "CardInspectionOverlayView");
                    if (overlay.IsValid()) overlayViewType = overlay.GetMonoType();
                }
                auto animator = BNM::Class("UnityEngine", "Animator");
                if (animator.IsValid()) animatorType = animator.GetMonoType();
                auto group = BNM::Class("UnityEngine", "CanvasGroup");
                if (group.IsValid()) {
                    canvasGroupType = group.GetMonoType();
                    groupSetAlpha = group.GetMethod("set_alpha", 1).cast<void>();
                    groupSetIgnoreParents = group.GetMethod("set_ignoreParentGroups", 1).cast<void>();
                }
                if (canvas.IsValid()) {
                    canvasSetOverride = canvas.GetMethod("set_overrideSorting", 1).cast<void>();
                    canvasSetOrder = canvas.GetMethod("set_sortingOrder", 1).cast<void>();
                    canvasSetLayer = canvas.GetMethod("set_sortingLayerID", 1).cast<void>();
                    canvasGetOrder = canvas.GetMethod("get_sortingOrder", 0).cast<int>();
                }
                auto prompt = BNM::Class("PvZCards.Game", "TargetToolTipHintViewComponent");
                if (prompt.IsValid()) promptViewType = prompt.GetMonoType();
                factoryOfMapper = mapper.GetField("<CardViewFactory>k__BackingField");
                binderOfFactory = BNM::Class("PvZCards.Game", "CardViewFactory").GetField("<Binder>k__BackingField");
                auto boardTap = BNM::Class("PvZCards.Game", "HandleBoardTappedCommand");
                if (boardTap.IsValid()) tapPosition = boardTap.GetField("<TapPosition>k__BackingField");
            }
            objectDestroy = object.GetMethod("Destroy", 1).cast<void>();
            rendererBounds = renderer.GetMethod("get_bounds", 0).cast<Bounds>();
            rendererEnabled = renderer.GetMethod("get_enabled", 0).cast<bool>();
            rendererGetOrder = renderer.GetMethod("get_sortingOrder", 0).cast<int>();
            rendererSetOrder = renderer.GetMethod("set_sortingOrder", 1).cast<void>();
            rendererGetLayer = renderer.GetMethod("get_sortingLayerID", 0).cast<int>();
            rendererSetLayer = renderer.GetMethod("set_sortingLayerID", 1).cast<void>();
            spriteRendererSetSprite = spriteRenderer.GetMethod("set_sprite", 1).cast<void>();
            spriteRendererSetColor = spriteRenderer.GetMethod("set_color", 1).cast<void>();
            spriteCreate = sprite.GetMethod("Create", 4).cast<Obj*>();
            whiteTexture = texture2d.GetMethod("get_whiteTexture", 0).cast<Obj*>();
            rendererType = renderer.GetMonoType();
            spriteRendererType = spriteRenderer.GetMonoType();
            if (sortingGroup.IsValid()) {
                groupGetOrder = sortingGroup.GetMethod("get_sortingOrder", 0).cast<int>();
                groupSetOrder = sortingGroup.GetMethod("set_sortingOrder", 1).cast<void>();
                groupGetLayer = sortingGroup.GetMethod("get_sortingLayerID", 0).cast<int>();
                groupSetLayer = sortingGroup.GetMethod("set_sortingLayerID", 1).cast<void>();
                if (groupGetOrder.IsValid() && groupSetOrder.IsValid()) sortingGroupType = sortingGroup.GetMonoType();
            }

            // Action<CardView>: the type of CreateCardAsync's second parameter.
            if (createCardAsync.IsValid()) {
                const auto* info = createCardAsync.GetInfo();
                if (info && info->parameters && info->parameters[1])
                    callbackClass = BNM::Class(info->parameters[1]).GetClass();
                callbackMethodInfo = info;
            }

            if (void* il2cpp = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD)) {
                gcNew = reinterpret_cast<decltype(gcNew)>(dlsym(il2cpp, "il2cpp_gchandle_new"));
                gcFree = reinterpret_cast<decltype(gcFree)>(dlsym(il2cpp, "il2cpp_gchandle_free"));
            }

            ok = taskMapper.IsValid() && taskRootView.IsValid() && mapperViewMapper.IsValid() &&
                 mapperCreateCardAsync.IsValid() && viewMapperRemove.IsValid() && viewMapperGet.IsValid() &&
                 cardViewShowInHand.IsValid() && cardViewDesiredScale.IsValid() && callbackClass && cameraMain.IsValid() &&
                 cameraOrthographicSize.IsValid() && cameraViewportToWorld.IsValid() &&
                 componentTransform.IsValid() && componentGameObject.IsValid() && componentsInChildren.IsValid() &&
                 transformGetPosition.IsValid() && transformSetPosition.IsValid() && transformGetLocalScale.IsValid() &&
                 transformSetLocalScale.IsValid() && gameObjectCtor.IsValid() && gameObjectAddComponent.IsValid() &&
                 gameObjectTransform.IsValid() && objectDestroy.IsValid() && rendererBounds.IsValid() &&
                 rendererGetOrder.IsValid() && rendererSetOrder.IsValid() &&
                 rendererGetLayer.IsValid() && rendererSetLayer.IsValid() && spriteRendererSetSprite.IsValid() &&
                 spriteRendererSetColor.IsValid() && spriteCreate.IsValid() && whiteTexture.IsValid() && gcNew && gcFree;
            if (!ok) {
                LOG_ERR("CustomComponents/Pick3/DiscoverUI: screen members missing (task %d/%d mapper %d/%d remove %d get %d showInHand %d callback %d camera %d/%d/%d/%d component %d/%d/%d transform %d/%d/%d/%d gameObject %d/%d/%d destroy %d renderer %d/%d/%d/%d/%d/%d sprite %d/%d/%d/%d gc %d)",
                        taskMapper.IsValid(), taskRootView.IsValid(), mapperViewMapper.IsValid(), mapperCreateCardAsync.IsValid(),
                        viewMapperRemove.IsValid(), viewMapperGet.IsValid(), cardViewShowInHand.IsValid(), callbackClass != nullptr,
                        cameraMain.IsValid(), cameraOrthographic.IsValid(), cameraOrthographicSize.IsValid(), cameraViewportToWorld.IsValid(),
                        componentTransform.IsValid(), componentGameObject.IsValid(), componentsInChildren.IsValid(),
                        transformGetPosition.IsValid(), transformSetPosition.IsValid(), transformGetLocalScale.IsValid(), transformSetLocalScale.IsValid(),
                        gameObjectCtor.IsValid(), gameObjectAddComponent.IsValid(), gameObjectTransform.IsValid(), objectDestroy.IsValid(),
                        rendererBounds.IsValid(), rendererEnabled.IsValid(), rendererGetOrder.IsValid(), rendererSetOrder.IsValid(),
                        rendererGetLayer.IsValid(), rendererSetLayer.IsValid(), spriteRendererSetSprite.IsValid(), spriteRendererSetColor.IsValid(),
                        spriteCreate.IsValid(), whiteTexture.IsValid(), gcNew != nullptr && gcFree != nullptr);
                return false;
            }

            auto start = task.GetMethod("Start", 0);
            if (!start.IsValid()) { LOG_ERR("CustomComponents/Pick3/DiscoverUI: LoadCardsTask.Start missing"); return false; }
            CustomComponents::Core::Hooks::Install(start, StartHook, originalStart);
            if (!originalStart) { LOG_ERR("CustomComponents/Pick3/DiscoverUI: LoadCardsTask.Start hook failed"); return false; }
            ready = true;
            return true;
        }

    } // namespace Ui

    inline bool enabled=false;
    inline bool useStockTargeting=false;
    inline bool maskStockTargetUI=false;
    inline Pick3::PreviewRequest activeRequest;
    // Root the exact EntityModel used when this choice was activated. The
    // holder may briefly switch targeting phase before the player submits;
    // that must suspend this *unsubmitted* offer, not retire it permanently.
    inline Obj* activeOfferModel=nullptr;
    inline uintptr_t activeOfferModelHandle=0;
    inline void ReleaseActiveOfferModel() {
        Ui::Unpin(activeOfferModelHandle);
        activeOfferModel=nullptr;
    }
    inline bool hasRequest=false, awaitingResult=false, autoSolePending=false;
    inline bool awaitingStockCommand=false;
    inline bool awaitingAdditionalBoard=false,choosingAdditionalCards=false;
    inline bool selectingAdditionalFirst=false;
    inline int firstAdditionalTarget=-1;
    inline std::vector<Pick3::Candidate> firstPrimaryCandidates;
    inline std::vector<int> additionalEligible;
    inline std::vector<Pick3::Candidate> additionalOptions;
    inline int lastLightsEffect=-1,lastLightsGroup=-1,lastLightsStage=-1;
    inline unsigned lightsRetryTicks=0;
    inline unsigned stockCommandWaitTicks=0;
    inline unsigned autoSoleWaitTicks=0;
    inline std::vector<int> chosenIndices;
    inline int lastCaptureEffect=-1,lastCaptureGroup=-1;
    inline void ConfigureTargetingText(bool behind,int x,int y,int size,int maxChars=32) {
        // Preserve the old 720x1280 authored positioning (Y upwards).
        promptBehindCards=behind;
        promptFontScale=std::clamp(size,40,180)/100.f;
        promptMaxChars=std::clamp(maxChars,12,60);
        Ui::promptX=std::clamp(0.5f+static_cast<float>(x)/720.f,0.05f,0.95f);
        Ui::promptY=std::clamp(0.5f+static_cast<float>(y)/1280.f,0.08f,0.97f);
    }
    inline void Finish(bool accepted) {
        if(!hasRequest)return;
        Pick3::Submission::SetPopupInputCapture(false);
        Pick3::Submission::FinishTransaction(activeRequest,accepted);
        CloseGroup(activeRequest.effectEntityId,activeRequest.effectGroupId,-1);
        if (!accepted || (!Ui::state.exiting && !Ui::state.deferredExit))
            Ui::Hide(accepted?"stock selection completed":"selection failed");
        CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: FINISH effect=%d group=%d accepted=%d",
                 activeRequest.effectEntityId,activeRequest.effectGroupId,accepted?1:0);
        activeRequest={};hasRequest=false;awaitingResult=false;awaitingStockCommand=false;
        awaitingAdditionalBoard=choosingAdditionalCards=selectingAdditionalFirst=false;firstAdditionalTarget=-1;firstPrimaryCandidates.clear();additionalEligible.clear();additionalOptions.clear();
        ReleaseActiveOfferModel();
        autoSolePending=false;chosenIndices.clear();currentPromptGuid=-1;
    }
    inline void ResetMatchView() {
        Ui::Hide("match changed");
        ReleaseActiveOfferModel();
        hasRequest=awaitingResult=autoSolePending=awaitingStockCommand=false;
        awaitingAdditionalBoard=choosingAdditionalCards=selectingAdditionalFirst=false;firstAdditionalTarget=-1;firstPrimaryCandidates.clear();additionalEligible.clear();additionalOptions.clear();
        activeRequest={};chosenIndices.clear();currentPromptGuid=-1;
        lastLightsEffect=lastLightsGroup=lastLightsStage=-1;
        stockCommandWaitTicks=autoSoleWaitTicks=0;
        lastCaptureEffect=lastCaptureGroup=-1;
        {std::lock_guard<std::mutex> lock(groupsMutex);groups.clear();}
    }
    inline bool ExactTurn(const Pick3::PreviewRequest& request,bool remote);
    inline Obj* PrimaryCard(int index) {
        if(index<0 || index>=int(activeRequest.options.size()))return nullptr;
        const auto& option=activeRequest.options[index];
        return activeRequest.mode==Pick3::SelectionMode::CardDatabase?Engine::BoardPlacement::Definition(option.cardGuid,Engine::EntityAccess::Find(activeOfferModel,activeRequest.effectEntityId)):Engine::EntityAccess::Find(activeOfferModel,option.entityId);
    }
    using LightsFn=void(*)(Obj*,Obj*,Obj*,const void*);
    inline LightsFn originalTargetLights=nullptr;
    inline BNM::Method<void> activateTargetLight;

    // The stock recipient proxy is an execution bridge, never a UI offer.
    static void TargetLights(Obj* command,Obj* blocking,Obj* game,const void* method) {
        const auto& h=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        const auto identity=Systems::TargetingPresentationRefreshSystem::ReadIdentity(h.holder,h.state,blocking);
        const bool exact=hasRequest && identity.valid && identity.phase==0 &&
            identity.effectEntityId==activeRequest.effectEntityId && identity.effectGroupId==activeRequest.effectGroupId &&
            ExactTurn(activeRequest,Pick3::Submission::Remote());
        if(exact) {
            if(awaitingAdditionalBoard && activateTargetLight.IsValid()) {
                Obj* effect=Engine::EntityAccess::Find(activeOfferModel,activeRequest.effectEntityId);
                for(int id:additionalEligible)activateTargetLight[command](id,identity.sourceId,effect,game);
            }
            return; // Card popups never expose the additional board targets.
        }
        // Commands can run before the holder observer publishes the popup.
        Obj* model=Pick3::Submission::Target(Pick3::Submission::boundEntityModelRoot);
        Obj* filter=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,identity.effectEntityId),"PrimaryTargetFilter");
        if(identity.valid && identity.phase==0 && Pick3::EnumModes::IsPickFromThree(Pick3::TargetBridge::LogicalSelectionMode(filter)))return;
        originalTargetLights(command,blocking,game,method);
    }
    inline void RefreshStageLights() {
        if(!hasRequest || awaitingStockCommand || awaitingResult || !ExactTurn(activeRequest,Pick3::Submission::Remote()))return;
        const int stage=awaitingAdditionalBoard?(selectingAdditionalFirst?2:1):0;
        if(lastLightsEffect==activeRequest.effectEntityId && lastLightsGroup==activeRequest.effectGroupId && lastLightsStage==stage)return;
        if(lightsRetryTicks){--lightsRetryTicks;return;}
        lightsRetryTicks=30; // Bound retries if a frontend dependency is temporarily unavailable.
        namespace S=Pick3::Submission;
        Obj* binder=S::Target(S::binderRoot);Obj* injector=nullptr;Obj* clear=nullptr;
        if(!binder || !S::getInjectionBinder.IsValid() || !S::getInjectedInstance.IsValid())return;
        static auto type=BNM::Class("PvZCards.Game","DeactivateHighlightSignal");
        static auto dispatch=type.GetMethod("Dispatch",0).cast<void>();
        const auto error=BNM::TryInvoke([&]{
            injector=S::getInjectionBinder[binder]();
            if(injector)clear=S::getInjectedInstance[injector](type.GetMonoType(),static_cast<Obj*>(nullptr));
            if(clear && dispatch.IsValid())dispatch[clear]();
        });
        if(error.IsValid() || !clear || !dispatch.IsValid())return;
        const auto h=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        if(!Systems::TargetingPresentationRefreshSystem::RefreshStockPresentation(h.holder,h.state))return;
        lightsRetryTicks=0;
        lastLightsEffect=activeRequest.effectEntityId;lastLightsGroup=activeRequest.effectGroupId;lastLightsStage=stage;
    }
    inline bool CompleteFirstAdditional(int target);
    inline bool StageAdditional(int target,int* result) {
        if(!hasRequest || chosenIndices.size()!=1 || !ExactTurn(activeRequest,Pick3::Submission::Remote()) ||
           std::find(additionalEligible.begin(),additionalEligible.end(),target)==additionalEligible.end())return false;
        const int primary=activeRequest.mode==Pick3::SelectionMode::CardDatabase?target:activeRequest.options[chosenIndices[0]].entityId;
        if(!Pick3::AdditionalSelection::Authorize(activeOfferModel,activeRequest.effectEntityId,activeRequest.effectGroupId,primary,target,PrimaryCard(chosenIndices[0])))return false;
        if(activeRequest.mode==Pick3::SelectionMode::CardDatabase && !Pick3::TargetBridge::ChooseDatabaseRecipient(activeRequest.effectEntityId,activeRequest.effectGroupId,target))return false;
        activeRequest.additionalTargetId=target;
        const int recipient=Pick3::Submission::StockTapRecipient(activeRequest,chosenIndices[0]);
        if(recipient<0 || !Pick3::Submission::StageStockHandoff(activeRequest,chosenIndices))return false;
        awaitingAdditionalBoard=choosingAdditionalCards=false;awaitingStockCommand=true;stockCommandWaitTicks=0;
        if(result)*result=recipient;
        return true;
    }
    inline bool AdditionalBoardTap(Obj*,Obj* overlaps,int* result) {
        if(!awaitingAdditionalBoard || !hasRequest || !ExactTurn(activeRequest,Pick3::Submission::Remote()))return false;
        // Stock can resolve an overlapped fighter to its lane. Prefer that
        // validated board target before inspecting the raw overlap identities.
        const int stockTarget=result?*result:-1;
        if(result)*result=-1;
        if(selectingAdditionalFirst){if(stockTarget>=0 && CompleteFirstAdditional(stockTarget))return true;}
        else if(stockTarget>=0 && StageAdditional(stockTarget,result))return true;
        auto* ids=reinterpret_cast<MonoList<int>*>(overlaps);
        if(!ids || !ids->items || ids->size<0 || ids->size>4096 || ids->items->capacity<size_t(ids->size))return true;
        for(int i=0;i<ids->size;++i)if(selectingAdditionalFirst?CompleteFirstAdditional(ids->items->m_Items[i]):StageAdditional(ids->items->m_Items[i],result))break;
        return true;
    }
    inline bool BeginAdditional(int index) {
        Obj* entity=Engine::EntityAccess::Find(activeOfferModel,activeRequest.effectEntityId);
        Obj* filter=Engine::EntityAccess::Component(entity,"PrimaryTargetFilter");
        const auto setting=Pick3::AdditionalSelection::Get(filter);
        if(!Pick3::AdditionalSelection::Interactive(setting.mode))return false;
        if(activeRequest.requiredTargets!=1){LOG_ERR("CustomComponents/AdditionalSelection: interactive additional choice requires NumTargets=1");chosenIndices.clear();return true;}
        const int primary=activeRequest.mode==Pick3::SelectionMode::CardDatabase?activeRequest.ownerPlayerId:activeRequest.options[index].entityId;
        additionalEligible=Pick3::AdditionalSelection::Enumerate(activeOfferModel,activeRequest.effectEntityId,primary,PrimaryCard(index));
        if(additionalEligible.empty()){LOG_ERR("CustomComponents/AdditionalSelection: no legal additional targets");chosenIndices.clear();return true;}
        Ui::BeginExit(index);
        if(setting.mode==Pick3::AdditionalSelection::Manual) {
            awaitingAdditionalBoard=true;Pick3::Submission::SetPopupInputCapture(false);return true;
        }
        std::vector<Pick3::Candidate> cards;
        for(int id:additionalEligible) {
            Obj* card=Engine::EntityAccess::Find(activeOfferModel,id);const int guid=InspectionStats::Guid(card);
            if(guid>0)cards.push_back({id,guid});
        }
        if(cards.empty()){LOG_ERR("CustomComponents/AdditionalSelection: PF3 additional targets must be cards");chosenIndices.clear();return true;}
        const auto mode=setting.mode==Pick3::AdditionalSelection::UniqueGuid?Pick3::SelectionMode::UniqueGuidTargets:Pick3::SelectionMode::RandomTargets;
        auto request=Pick3::MakeStableNetworkPreviewRequest(activeRequest.effectEntityId,activeRequest.effectGroupId,mode,cards,1);
        additionalOptions=request.options;
        if(Pick3::IsAutomaticUniqueChoice(request) && StageAdditional(additionalOptions[0].entityId,nullptr)) {
            // StageAdditional prepares the request; dispatch through the normal signal.
            Pick3::Submission::ClearStockHandoff();
            awaitingStockCommand=Pick3::Submission::DispatchAutomaticTap(activeRequest,chosenIndices);
            if(awaitingStockCommand)return true;
        }
        choosingAdditionalCards=true;
        Group group;group.source=activeRequest.effectEntityId;group.serial=activeRequest.effectGroupId;group.open=true;
        for(auto option:additionalOptions){group.ids.push_back(option.entityId);group.guids.push_back(option.cardGuid);}
        CaptureGroupStats(group,activeOfferModel);Publish(group);return true;
    }
    inline void PublishPrimaryChoices() {
        Group group;group.source=activeRequest.effectEntityId;group.serial=activeRequest.effectGroupId;group.open=true;
        for(const auto& option:activeRequest.options){group.ids.push_back(option.entityId);group.guids.push_back(option.cardGuid);}
        CaptureGroupStats(group,activeOfferModel);Publish(group);
    }
    inline bool CompleteFirstAdditional(int target) {
        if(!selectingAdditionalFirst || !ExactTurn(activeRequest,Pick3::Submission::Remote()) || std::find(additionalEligible.begin(),additionalEligible.end(),target)==additionalEligible.end())return false;
        Obj* effect=Engine::EntityAccess::Find(activeOfferModel,activeRequest.effectEntityId);
        Obj* filter=Engine::EntityAccess::Component(effect,"PrimaryTargetFilter");
        Obj* query=Pick3::TargetBridge::OriginalDatabaseQuery(filter,activeRequest.effectEntityId,activeRequest.effectGroupId);
        std::vector<Pick3::Candidate> candidates;
        Engine::PlacementQueryContext::Scope placement({activeOfferModel,effect,nullptr,target});
        auto context=Engine::BoardPlacement::Context(activeOfferModel,effect,target);
        if(!context)return false;
        for(const auto& candidate:firstPrimaryCandidates) {
            Obj* card=activeRequest.mode==Pick3::SelectionMode::CardDatabase?Engine::BoardPlacement::Definition(candidate.cardGuid,effect):Engine::EntityAccess::Find(activeOfferModel,candidate.entityId);
            if(!card || !Engine::BoardPlacement::Matches(query,card,context->Get()))continue;
            const int primary=activeRequest.mode==Pick3::SelectionMode::CardDatabase?activeRequest.ownerPlayerId:candidate.entityId;
            const auto additional=Pick3::AdditionalSelection::Enumerate(activeOfferModel,activeRequest.effectEntityId,primary,card);
            if(std::find(additional.begin(),additional.end(),target)!=additional.end())candidates.push_back(candidate);
        }
        if(candidates.empty())return false; // First-stage input remains available; no orphaned PF3.
        auto offer=Pick3::MakeStableNetworkPreviewRequest(activeRequest.effectEntityId,activeRequest.effectGroupId,activeRequest.mode,candidates,activeRequest.requiredTargets);
        if(offer.options.empty())return false;
        activeRequest.options=std::move(offer.options);activeRequest.additionalTargetId=target;firstAdditionalTarget=target;
        if(activeRequest.mode==Pick3::SelectionMode::CardDatabase && !Pick3::TargetBridge::ChooseDatabaseRecipient(activeRequest.effectEntityId,activeRequest.effectGroupId,target))return false;
        awaitingAdditionalBoard=choosingAdditionalCards=selectingAdditionalFirst=false;chosenIndices.clear();
        if(Pick3::IsAutomaticUniqueChoice(activeRequest)) {
            chosenIndices={0};
            if(StageAdditional(target,nullptr)) {
                Pick3::Submission::ClearStockHandoff();
                if(Pick3::Submission::DispatchAutomaticTap(activeRequest,chosenIndices))return true;
            }
            awaitingStockCommand=false;chosenIndices.clear();
        }
        PublishPrimaryChoices();return true;
    }
    inline bool BeginFirstAdditional() {
        Obj* effect=Engine::EntityAccess::Find(activeOfferModel,activeRequest.effectEntityId);Obj* filter=Engine::EntityAccess::Component(effect,"PrimaryTargetFilter");
        const auto setting=Pick3::AdditionalSelection::Get(filter);
        if(!setting.first || !Pick3::AdditionalSelection::Interactive(setting.mode) || activeRequest.requiredTargets!=1)return false;
        Obj* query=Pick3::TargetBridge::OriginalDatabaseQuery(filter,activeRequest.effectEntityId,activeRequest.effectGroupId);
        if(activeRequest.mode==Pick3::SelectionMode::CardDatabase)firstPrimaryCandidates=Pick3::TargetBridge::EnumerateCardDatabase(query,activeRequest.effectEntityId,activeOfferModel);
        else {std::lock_guard<std::mutex> lock(Pick3::TargetBridge::snapshotMutex);firstPrimaryCandidates=Pick3::TargetBridge::latestEligiblePairs;}
        additionalEligible.clear();
        for(auto candidate:firstPrimaryCandidates) {
            Obj* card=activeRequest.mode==Pick3::SelectionMode::CardDatabase?Engine::BoardPlacement::Definition(candidate.cardGuid,effect):Engine::EntityAccess::Find(activeOfferModel,candidate.entityId);
            const int primary=activeRequest.mode==Pick3::SelectionMode::CardDatabase?activeRequest.ownerPlayerId:candidate.entityId;
            for(int id:Pick3::AdditionalSelection::Enumerate(activeOfferModel,activeRequest.effectEntityId,primary,card))if(std::find(additionalEligible.begin(),additionalEligible.end(),id)==additionalEligible.end())additionalEligible.push_back(id);
        }
        if(additionalEligible.empty())return false;
        selectingAdditionalFirst=true;
        if(setting.mode==Pick3::AdditionalSelection::Manual){awaitingAdditionalBoard=true;Pick3::Submission::SetPopupInputCapture(false);return true;}
        std::vector<Pick3::Candidate> cards;
        for(int id:additionalEligible){Obj* card=Engine::EntityAccess::Find(activeOfferModel,id);int guid=InspectionStats::Guid(card);if(guid>0)cards.push_back({id,guid});}
        const auto mode=setting.mode==Pick3::AdditionalSelection::UniqueGuid?Pick3::SelectionMode::UniqueGuidTargets:Pick3::SelectionMode::RandomTargets;
        auto offer=Pick3::MakeStableNetworkPreviewRequest(activeRequest.effectEntityId,activeRequest.effectGroupId,mode,cards,1);
        additionalOptions=offer.options;
        if(additionalOptions.empty()){selectingAdditionalFirst=false;return false;}
        if(Pick3::IsAutomaticUniqueChoice(offer) && CompleteFirstAdditional(additionalOptions[0].entityId))return true;
        choosingAdditionalCards=true;Group group;group.source=activeRequest.effectEntityId;group.serial=activeRequest.effectGroupId;group.open=true;
        for(auto option:additionalOptions){group.ids.push_back(option.entityId);group.guids.push_back(option.cardGuid);}
        CaptureGroupStats(group,activeOfferModel);Publish(group);return true;
    }
    inline bool PickTap(Obj* command,int* result) {
        if(!enabled || useStockTargeting)return false;
        if(!hasRequest || autoSolePending)return false;
        // Only intercept this exact phase-0 PF3 choice. Revive's later lane
        // targeting and every unrelated stock input must remain untouched.
        if(!ExactTurn(activeRequest,Pick3::Submission::Remote()))return false;
        if(awaitingAdditionalBoard)return false;
        if(Ui::state.exiting || Ui::state.deferredExit){if(result)*result=-1;return true;}
        if(awaitingResult || awaitingStockCommand) { if(result)*result=-1; return true; }
        int index=-1;
        if(!Ui::PickAt(command,&index)) {
            // If the presentation hasn't completed Show yet, still do not
            // allow picking a board hero or lane through the unfinished UI.
            if(result)*result=-1;
            return true;
        }
        if(result)*result=-1;
        if(choosingAdditionalCards) {
            if(index>=0 && index<int(additionalOptions.size())) {
                if(selectingAdditionalFirst){Ui::BeginExit(index);CompleteFirstAdditional(additionalOptions[index].entityId);}
                else if(StageAdditional(additionalOptions[index].entityId,result))Ui::BeginExit(index);
            }
            return true;
        }
        if(result)*result=-1; // swallowed; never synthesize a stock board target
        if(index<0 || index>=static_cast<int>(activeRequest.options.size()))return true;
        if(std::find(chosenIndices.begin(),chosenIndices.end(),index)!=chosenIndices.end())return true;
        chosenIndices.push_back(index);
        PICK3_DISCOVER_LOG("CustomComponents/Pick3/DiscoverUI: CHOOSE index=%d selected=%zu required=%d",
             index,chosenIndices.size(),activeRequest.requiredTargets);
        if(static_cast<int>(chosenIndices.size())<std::max(1,activeRequest.requiredTargets))return true;
        if(firstAdditionalTarget>=0){StageAdditional(firstAdditionalTarget,result);return true;}
        if(BeginAdditional(index))return true;
        if (Pick3::Submission::preferStockCommandHandoff) {
            const int recipient=Pick3::Submission::StockTapRecipient(activeRequest,index);
            if (recipient>=0 && Pick3::Submission::StageStockHandoff(activeRequest,chosenIndices)) {
                awaitingStockCommand=true;
                stockCommandWaitTicks=0;
                if(result)*result=recipient; // STOCK EntityTappedSignal drives SelectTargetCommand.
                CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_HANDOFF: PICKER_TO_STOCK effect=%d group=%d recipient=%d",
                         activeRequest.effectEntityId,activeRequest.effectGroupId,recipient);
            } else {
                LOG_ERR("CustomComponents/Pick3/STOCK_HANDOFF: STAGE_REJECTED effect=%d group=%d recipient=%d",
                        activeRequest.effectEntityId,activeRequest.effectGroupId,recipient);
                chosenIndices.clear();
            }
            return true;
        }
        Pick3::Submission::SetPopupInputCapture(false);
        Pick3::Submission::BeginDeadline();
        if(Pick3::Submission::Submit(activeRequest,chosenIndices,false)) {
            awaitingResult=true;
            Ui::BeginExit(index);
            // A dispatched Play must never re-enable the UI card-play guard.
            // Only an unsubmitted modal offer owns input capture.
        } else if(Pick3::Submission::CanRetryLastSubmission()) {
            chosenIndices.clear();
            Group group;group.source=activeRequest.effectEntityId;group.serial=activeRequest.effectGroupId;
            for(auto& o:activeRequest.options){group.ids.push_back(o.entityId);group.guids.push_back(o.cardGuid);}
            group.open=true;CaptureGroupStats(group,activeOfferModel);Publish(group);
        } else {
            // A service-entered failure needs stock completion, not a reroll.
            awaitingResult=true;
        }
        return true;
    }
    // The stock GameInputStateHolder already identifies the active targeting
    // effect, group and phase. In PvP the separately injected bound queue can
    // describe a different service/projection and may be Unknown even while
    // the holder is presenting that *exact* choice. Do not suppress the UI on
    // an unrelated queue identity; the Submission system still validates the
    // concrete queue during Submit().
    inline bool ExactTurn(const Pick3::PreviewRequest& request,bool remote) {
        (void)remote;
        const auto& h=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        return h.valid && h.state==6 && h.phase==0 &&
               h.effectEntityId==request.effectEntityId &&
               h.effectGroupId==request.effectGroupId;
    }
    inline void OnPick3PlayDispatch(int effect,int group) {
        if(hasRequest && activeRequest.effectEntityId==effect && activeRequest.effectGroupId==group &&
           !chosenIndices.empty()) {
            Ui::BeginExit(chosenIndices.back());
            CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: EXIT_TRIGGERED_BY_DISPATCH effect=%d group=%d",
                     effect,group);
        }
    }
    inline bool ShouldForwardStockChooseTap(Obj* command) {
        return enabled && !useStockTargeting && hasRequest && !awaitingResult &&
               !awaitingStockCommand && Pick3::Submission::preferStockCommandHandoff &&
               ExactTurn(activeRequest,Pick3::Submission::Remote()) &&
               Ui::ChooseAt(command)>=0;
    }
    inline void LogQueueEvidence(int effect,int group,bool remote) {
        Pick3::PreviewRequest identity;
        identity.effectEntityId=effect;
        identity.effectGroupId=group;
        const auto queue=remote?Pick3::Submission::InspectBoundBlockingQueue(identity):
            Pick3::Submission::InspectBlockingQueue(identity,Pick3::Submission::Target(Pick3::Submission::serviceRoot));
        CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: HOLDER_QUEUE_CROSSCHECK effect=%d group=%d queueState=%d holderIsAuthority=1 readOnly=1",
            effect,group,static_cast<int>(queue));
    }
    // The model injected into SelectTargetService is the live EntityModel used
    // by stock SelectTarget validation. boundEntityModelRoot belonged to the
    // old, now-quarantined post-submit dependency resolver and stays null.
    // Resolve via ALREADY captured BNM fields: no binder invoke, no new hooks.
    inline Obj* LiveSelectionEntityModel() {
        Obj* fallback=Pick3::Submission::Target(Pick3::Submission::boundEntityModelRoot);
        Obj* service=Pick3::Submission::Target(Pick3::Submission::serviceRoot);
        if(!service || !Pick3::Submission::localClass.IsValid() ||
           !BNM::IsA(service,Pick3::Submission::localClass) ||
           !Pick3::Submission::localServiceFinder.IsValid() ||
           !Pick3::TargetBridge::finderEntityFinder.IsValid() ||
           !Pick3::TargetBridge::entityFinderModel.IsValid())return fallback;
        Obj* finder=Pick3::Submission::localServiceFinder[service].Get();
        Obj* entityFinder=finder?Pick3::TargetBridge::finderEntityFinder[finder].Get():nullptr;
        Obj* current=entityFinder?Pick3::TargetBridge::entityFinderModel[entityFinder].Get():nullptr;
        return current?current:fallback;
    }
    inline void Tick() {
        if(!enabled)return;
        Ui::TickExit();
        RefreshStageLights();
        const auto& h=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;

#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
        {
            static int priorState=-999,priorPhase=-999,priorEffect=-999,priorGroup=-999;
            static unsigned transitions=0;
            if((h.state!=priorState || h.phase!=priorPhase || h.effectEntityId!=priorEffect || h.effectGroupId!=priorGroup) && ++transitions<=180) {
                // GameInputStateHolder's injected pointers are fixed in this
                // exact IL2CPP build (dump.cs: queue=0x10, gameModel=0x40).
                // Observation only: never dereference the referenced model.
                Obj* holderQueue=h.holder?*reinterpret_cast<Obj**>(reinterpret_cast<std::uint8_t*>(h.holder)+0x10):nullptr;
                Obj* holderGameModel=h.holder?*reinterpret_cast<Obj**>(reinterpret_cast<std::uint8_t*>(h.holder)+0x40):nullptr;
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: PHASE_OWNER seq=%u holderGameModel=%p holderQueue=%p targetingEffect=%p inputState=%d",
                    transitions,(void*)holderGameModel,(void*)holderQueue,(void*)h.effect,h.state);
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: TURN_PHASE seq=%u holder=%p valid=%d state=%d targetPhase=%d effect=%d group=%d owner=%d local=%d remote=%d service=%p boundModel=%p offers=%d queued=%d shown=%d exit=%d deferred=%d",
                    transitions,(void*)h.holder,int(h.valid),h.state,h.phase,h.effectEntityId,h.effectGroupId,
                    Pick3::TargetBridge::LocalPlayerId(),int(Pick3::Submission::Local()),int(Pick3::Submission::Remote()),
                    (void*)Pick3::Submission::Target(Pick3::Submission::serviceRoot),
                    (void*)Pick3::Submission::Target(Pick3::Submission::boundEntityModelRoot),
                    int(Pick3::TargetBridge::HasPendingOffer()),int(Pick3::TargetBridge::HasQueuedChoice()),
                    int(Ui::state.shown),int(Ui::state.exiting),int(Ui::state.deferredExit));
                priorState=h.state;priorPhase=h.phase;priorEffect=h.effectEntityId;priorGroup=h.effectGroupId;
            }
        }
#endif
        // Target 0 uses the custom PF3 prompt. On additional target stages,
        // restore the normal stock tooltip immediately, even during exit.
        const bool holderLeftPicker = Ui::state.shown && h.valid &&
            (h.effectEntityId!=Ui::state.source || h.effectGroupId!=Ui::state.serial);
        // A second target can be represented by a new blocking effect at
        // phase 0, not necessarily by Phase=1 on the same effect.
        if (h.valid && (h.phase!=0 || h.state!=6 || holderLeftPicker)) {
            Ui::SetSecondaryTargetMode(h.state==6 && (h.phase!=0 || holderLeftPicker));
            Ui::RestoreStockTargetPrompt();
            if (h.state==6 && (h.phase!=0 || holderLeftPicker)) {
                static int lastPromptEffect=-1,lastPromptGroup=-1,lastPromptPhase=-1;
                if (lastPromptEffect!=h.effectEntityId || lastPromptGroup!=h.effectGroupId || lastPromptPhase!=h.phase) {
                    CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: STOCK_SECONDARY_PROMPT_RESTORED effect=%d group=%d phase=%d previous=%d/%d",
                             h.effectEntityId,h.effectGroupId,h.phase,Ui::state.source,Ui::state.serial);
                    lastPromptEffect=h.effectEntityId;lastPromptGroup=h.effectGroupId;lastPromptPhase=h.phase;
                }
            }
        }
        // Patch 1.6: the picker observes the SAME stock holder update which
        // establishes this identity. Do not trust an unrelated EventSystem tick
        // to run after targeting / submission ownership has been bound.
        static unsigned holderFrames=0;
        static int lastState=-999, lastEffect=-999, lastGroup=-999, lastPhase=-999;
        ++holderFrames;
        if (holderFrames==1 || h.state!=lastState || h.effectEntityId!=lastEffect ||
            h.effectGroupId!=lastGroup || h.phase!=lastPhase ||
            (h.valid && holderFrames%180==0)) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: HOLDER_TICK frame=%u valid=%d state=%d phase=%d effect=%d group=%d local=%d remote=%d pending=%d queued=%d", 
                holderFrames, h.valid?1:0, h.state, h.phase, h.effectEntityId, h.effectGroupId,
                Pick3::Submission::Local()?1:0, Pick3::Submission::Remote()?1:0,
                Pick3::TargetBridge::HasPendingOffer()?1:0, Pick3::TargetBridge::HasQueuedChoice()?1:0);
            lastState=h.state;lastPhase=h.phase;lastEffect=h.effectEntityId;lastGroup=h.effectGroupId;
        }
        // Only attempt a first-service bind for a real stock blocking choice.
        // Never resolve injected services during mulligan or from repair timers.
        if(h.valid && h.state==6 && h.phase==0 &&
           Pick3::TargetBridge::HasPendingOffer() &&
           !Pick3::Submission::Local() && !Pick3::Submission::Remote())
            Pick3::Submission::PumpDependencies();
        const bool remote=Pick3::Submission::Remote()&&Pick3::Submission::allowPvp;
        const bool local=Pick3::Submission::Local()&&Pick3::Submission::allowPve;
        const bool matching=hasRequest && (remote||local) && ExactTurn(activeRequest,remote);
        if(awaitingAdditionalBoard && matching)return;
        if (awaitingStockCommand) {
            using H=Pick3::Submission::StockHandoffState;
            if (Pick3::Submission::stockHandoffState==H::Dispatched) {
                awaitingStockCommand=false;
                awaitingResult=true;
                Ui::BeginExit(chosenIndices.empty()?0:chosenIndices.back());
                // Stock Execute has taken ownership; allow normal card play.
            } else if (Pick3::Submission::stockHandoffState==H::Failed) {
                LOG_ERR("CustomComponents/Pick3/STOCK_HANDOFF: PRE_DISPATCH_FAILURE effect=%d group=%d stockCommandMayBeRetained=1",
                        activeRequest.effectEntityId,activeRequest.effectGroupId);
                awaitingStockCommand=false;
                // Never retry a Play that could have entered a stock command.
                // Keep ownership visible for tracing the stock failure.
                Pick3::Submission::ClearStockHandoff();
                chosenIndices.clear();
            } else if (Pick3::Submission::stockHandoffState==H::Armed && ++stockCommandWaitTicks>10) {
                LOG_ERR("CustomComponents/Pick3/STOCK_HANDOFF: STOCK_COMMAND_NOT_DISPATCHED effect=%d group=%d stockDidNotCallSelectTarget=1 noDirectFallback",
                        activeRequest.effectEntityId,activeRequest.effectGroupId);
                Pick3::Submission::ClearStockHandoff();
                chosenIndices.clear();
                awaitingStockCommand=false;
                if(matching && !useStockTargeting) {
                    // Armed means no stock selection command was entered. Return
                    // control to the user without replaying a gameplay action.
                    awaitingAdditionalBoard=choosingAdditionalCards=false;
                    activeRequest.additionalTargetId=-1;
                    Group group;group.source=activeRequest.effectEntityId;group.serial=activeRequest.effectGroupId;group.open=true;
                    for(const auto& option:activeRequest.options){group.ids.push_back(option.entityId);group.guids.push_back(option.cardGuid);}
                    CaptureGroupStats(group,activeOfferModel);Publish(group);
                }
            }
            return;
        }
        if(awaitingResult) {
            auto status=Pick3::Submission::PollResult();
            if(status==SelectionCompletion::Result::Success)Finish(true);
            else if(status==SelectionCompletion::Result::Failure || status==SelectionCompletion::Result::Uncertain)Finish(false);
            else if(!matching) {
                static int lastInFlightEffect=-1,lastInFlightGroup=-1;
                if(lastInFlightEffect!=activeRequest.effectEntityId || lastInFlightGroup!=activeRequest.effectGroupId) {
                    lastInFlightEffect=activeRequest.effectEntityId;lastInFlightGroup=activeRequest.effectGroupId;
                    LOG_WARN("CustomComponents/Pick3/DiscoverUI: INFLIGHT_STOCK_ADVANCED effect=%d group=%d holder=%d/%d completion=%d awaitingStock=1 noForcedFinish=1",activeRequest.effectEntityId,activeRequest.effectGroupId,h.effectEntityId,h.effectGroupId,static_cast<int>(status));
                }
            }
            return;
        }
        // A newly cached offer must not overwrite the exiting screen before
        // its card-view objects have been retired. Gameplay polling above
        // continues independently; only presentation waits for 0.82s.
        if (Ui::state.exiting || Ui::state.deferredExit) return;
        // A one-option live PickFromThree is still a stock blocking command.
        // The historical AUTO_SOLE branch invoked Submission::Submit directly
        // without passing through SelectTargetCommand.Execute/SelectTarget.
        // That leaves the retained command and its target subturn unresolved,
        // especially when revive must advance to manual lane selection.
        // Never dispatch that direct-service auto-selection here. The single
        // card uses the same Choose/stock-command handoff as 2/3-card menus.
        if(hasRequest && !matching){
            // An animation/target phase transition is not an authoritative
            // completion. Keep the exact unsubmitted options/model available
            // for a later return to this stock blocking choice.
            const bool suspended=activeOfferModelHandle && activeOfferModel &&
                Pick3::TargetBridge::SuspendUnsubmittedOffer(activeRequest,activeOfferModel);
            CC_SELECTION_DIAG("CustomComponents/Pick3/REPAIR1719: UNDISPATCHED_INTERRUPTED effect=%d group=%d next=%d/%d suspended=%d submitted=0",
                activeRequest.effectEntityId,activeRequest.effectGroupId,
                h.effectEntityId,h.effectGroupId,int(suspended));
            if(!suspended) {
                // If the model/owner cannot be proven, preserve old fail-closed
                // behavior rather than publishing a speculative replacement.
                Pick3::TargetBridge::CompleteLocalSelection(activeRequest.effectEntityId,activeRequest.effectGroupId,false);
            }
            Ui::Hide("stock effect advanced before submission");
            CloseGroup(activeRequest.effectEntityId,activeRequest.effectGroupId,-1);
            Pick3::Submission::SetPopupInputCapture(false);
            hasRequest=false;autoSolePending=false;chosenIndices.clear();activeRequest={};currentPromptGuid=-1;
            awaitingAdditionalBoard=choosingAdditionalCards=selectingAdditionalFirst=false;firstAdditionalTarget=-1;firstPrimaryCandidates.clear();additionalEligible.clear();additionalOptions.clear();
            ReleaseActiveOfferModel();
        }
        if(!(remote||local) || !h.valid || h.state!=6 || h.phase!=0){Ui::OnUpdate(h.holder,false,-1,-1);return;}
        // An active popup already owns a pinned model and exact offer. Re-running
        // publication recovery here can inspect stale prediction models on every frame.
        if(hasRequest && matching) {
            Ui::OnUpdate(h.holder,!useStockTargeting,activeRequest.effectEntityId,activeRequest.effectGroupId);
            return;
        }
        Obj* model=LiveSelectionEntityModel();
        const int localOwner=Pick3::TargetBridge::LocalPlayerId();
        // PvP's RemoteSelectTargetService has no <Finder> backing field.
        // Its service pointer must never be interpreted as LocalSelectTargetService.
        // The stock finder that produced this precise offer already supplied
        // a model snapshot; use it ONLY for presentation activation, and only
        // for the holder's exact effect/group. Submission remains unchanged.
        if(!model && Pick3::TargetBridge::HasPendingOffer()) {
            model=Pick3::TargetBridge::CachedModelForHolderChoice(
                h.effectEntityId,h.effectGroupId,localOwner);
            if(!model && local && !remote && localOwner>=0)
                model=Pick3::TargetBridge::VerifiedCachedModelForActivePve(
                    h.effectEntityId,h.effectGroupId,localOwner);
            static int lastFallbackEffect=-1,lastFallbackGroup=-1;
            if(lastFallbackEffect!=h.effectEntityId || lastFallbackGroup!=h.effectGroupId) {
                lastFallbackEffect=h.effectEntityId;lastFallbackGroup=h.effectGroupId;
                CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: OFFER_MODEL_FALLBACK effect=%d group=%d owner=%d model=%p source=stock-finder-cache presentationOnly=1",
                         h.effectEntityId,h.effectGroupId,localOwner,static_cast<void*>(model));
            }
        }
        static int missingModelEffect=-1,missingModelGroup=-1;
        if(!model && (h.effectEntityId!=missingModelEffect || h.effectGroupId!=missingModelGroup)) {
            missingModelEffect=h.effectEntityId;missingModelGroup=h.effectGroupId;
            CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: WAIT_LIVE_MODEL effect=%d group=%d service=%p finderField=%d entityFinderField=%d entityModelField=%d boundModel=%p pending=%d",
                h.effectEntityId,h.effectGroupId,
                Pick3::Submission::Target(Pick3::Submission::serviceRoot),
                Pick3::Submission::localServiceFinder.IsValid()?1:0,
                Pick3::TargetBridge::finderEntityFinder.IsValid()?1:0,
                Pick3::TargetBridge::entityFinderModel.IsValid()?1:0,
                Pick3::Submission::Target(Pick3::Submission::boundEntityModelRoot),
                Pick3::TargetBridge::HasPendingOffer()?1:0);
        }
        if(model && (local || localOwner>=0) && [&]{
            const int mode=Pick3::TargetBridge::ReadActiveSelectionMode(h.effectEntityId,const_cast<Obj*>(model));
            const bool normalized=mode==1 && Pick3::TargetBridge::HasNormalizedActiveChoice(
                h.effectEntityId,h.effectGroupId);
            if(normalized) PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1719: ACTIVE_NORMALIZED_MANUAL effect=%d group=%d mode=1 exactOverride=1",
                h.effectEntityId,h.effectGroupId);
            return mode<0 || Pick3::EnumModes::IsPickFromThree(mode) || normalized;
        }()) {
            Pick3::PreviewRequest identity;identity.effectEntityId=h.effectEntityId;identity.effectGroupId=h.effectGroupId;
            if(ExactTurn(identity,remote)) {
                if(lastCaptureEffect!=h.effectEntityId || lastCaptureGroup!=h.effectGroupId){
                    lastCaptureEffect=h.effectEntityId;lastCaptureGroup=h.effectGroupId;
                    LogQueueEvidence(h.effectEntityId,h.effectGroupId,remote);
                    const auto c=Pick3VisualBarrierTracker::CaptureOffer(h.effectEntityId,h.effectGroupId);
                    CC_SELECTION_DIAG("CustomComponents/Pick3/VISUAL_BARRIER: event=BARRIER_CAPTURE effect=%d group=%d candidateQueue=%p deps=%d ownership=UNKNOWN policy=OBSERVE_ONLY",h.effectEntityId,h.effectGroupId,reinterpret_cast<void*>(c.queue),c.dependencies);
                }
                bool published=Pick3::TargetBridge::PublishForActiveEffect(h.effectEntityId,h.effectGroupId,const_cast<Obj*>(model),localOwner);
                // Start-of-tricks PvE effects may advance their stock holder
                // beyond the model still bound in LocalSelectTargetService.
                // Retry presentation ONLY for the exact active holder, using
                // a uniquely validated stock-finder snapshot. PvP retains
                // its existing path. Submit() still performs stock checks.
                if(!published && local && !remote && localOwner>=0 &&
                   !Pick3::TargetBridge::HasQueuedChoice()) {
                    Obj* cached=Pick3::TargetBridge::VerifiedCachedModelForActivePve(
                        h.effectEntityId,h.effectGroupId,localOwner);
                    if(cached && cached!=model) {
                        CC_SELECTION_DIAG("CustomComponents/Pick3/PVE_START_TRICKS: phase=RETRY effect=%d group=%d owner=%d serviceModel=%p cachedModel=%p holderExact=1",
                            h.effectEntityId,h.effectGroupId,localOwner,(void*)model,(void*)cached);
                        published=Pick3::TargetBridge::PublishForActiveEffect(
                            h.effectEntityId,h.effectGroupId,cached,localOwner);
                        CC_SELECTION_DIAG("CustomComponents/Pick3/PVE_START_TRICKS: phase=RETRY_RESULT effect=%d group=%d accepted=%d",
                            h.effectEntityId,h.effectGroupId,int(published));
                        if(published) model=cached;
                    } else {
                        static int lastMissEffect=-1,lastMissGroup=-1;
                        if(lastMissEffect!=h.effectEntityId || lastMissGroup!=h.effectGroupId) {
                            lastMissEffect=h.effectEntityId;lastMissGroup=h.effectGroupId;
                            CC_SELECTION_DIAG("CustomComponents/Pick3/PVE_START_TRICKS: phase=NO_VERIFIED_CACHE effect=%d group=%d owner=%d fallback=blocked",
                                h.effectEntityId,h.effectGroupId,localOwner);
                        }
                    }
                }
                static int lastPublishEffect=-1,lastPublishGroup=-1;
                if(lastPublishEffect!=h.effectEntityId || lastPublishGroup!=h.effectGroupId || published) {
                    CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: HOLDER_PUBLISH effect=%d group=%d accepted=%d queued=%d pending=%d owner=%d model=%p",
                        h.effectEntityId,h.effectGroupId,published?1:0,
                        Pick3::TargetBridge::HasQueuedChoice()?1:0,Pick3::TargetBridge::HasPendingOffer()?1:0,
                        localOwner,const_cast<Obj*>(model));
                    lastPublishEffect=h.effectEntityId;lastPublishGroup=h.effectGroupId;
                }
            }
        }
        if(!hasRequest && Pick3::TargetBridge::HasQueuedChoice()) {
            Pick3::PreviewRequest candidate;
            {std::lock_guard<std::mutex> l(Pick3::TargetBridge::snapshotMutex);
             if(Pick3::TargetBridge::requestReady)candidate=Pick3::TargetBridge::nextRequest;}
            if(!candidate.options.empty() && ExactTurn(candidate,remote) &&
                (localOwner<0 || candidate.ownerPlayerId==localOwner) &&
                Pick3::TargetBridge::Consume(activeRequest)){
                hasRequest=true;chosenIndices.clear();currentPromptGuid=activeRequest.sourceCardGuid;
                ReleaseActiveOfferModel();
                activeOfferModel=Ui::Pin(model,activeOfferModelHandle);
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1719: OFFER_MODEL_PIN effect=%d group=%d model=%p pinned=%d",
                    activeRequest.effectEntityId,activeRequest.effectGroupId,(void*)model,int(activeOfferModelHandle!=0));
                if(BeginFirstAdditional())return;
                if(Pick3::IsAutomaticUniqueChoice(activeRequest)) {
                    chosenIndices={0};
                    if(!BeginAdditional(0)) {
                        awaitingStockCommand=true;stockCommandWaitTicks=0;
                        if(Pick3::Submission::DispatchAutomaticTap(activeRequest,chosenIndices))return;
                        awaitingStockCommand=false;chosenIndices.clear();
                        LOG_WARN("CustomComponents/Pick3: automatic choice dependencies unavailable; showing the sole choice");
                    } else if(awaitingAdditionalBoard || choosingAdditionalCards || awaitingStockCommand)return;
                }
                if(activeRequest.requiredTargets==1 && activeRequest.options.size()==1) {
                    CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: SOLE_STOCK_CHOICE effect=%d group=%d mode=%d entity=%d manualFallback=1 directSubmit=0",
                        activeRequest.effectEntityId,activeRequest.effectGroupId,
                        static_cast<int>(activeRequest.mode),activeRequest.options[0].entityId);
                    // Safety-first: stock command must own this selection.
                    // No fabricated board tap, no direct Process() call.
                }
                if(!useStockTargeting){
                    Group group;group.source=activeRequest.effectEntityId;group.serial=activeRequest.effectGroupId;
                    for(const auto& item:activeRequest.options){group.ids.push_back(item.entityId);group.guids.push_back(item.cardGuid);}
                    group.open=true;CaptureGroupStats(group,activeOfferModel);Publish(group);
                    CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: PRESENT effect=%d group=%d options=%zu mode=%d",
                             group.source,group.serial,group.ids.size(),static_cast<int>(activeRequest.mode));
                }
            }
        }
        Ui::OnUpdate(h.holder,hasRequest && !useStockTargeting && ExactTurn(activeRequest,remote),
                     hasRequest?activeRequest.effectEntityId:-1,hasRequest?activeRequest.effectGroupId:-1);
    }
    inline BNM::Method<int> frameCount;
    inline Core::PresentationRefresh::FrameGate presentationFrames;
    inline void TickOncePerFrame() {
        if(frameCount.IsValid()) {
            if(!presentationFrames.Enter(frameCount()))return;
        }
        Tick();
    }
    inline void OnStockHolderUpdated(Obj* holder) {
        const auto& identity=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        if(identity.holder && holder && identity.holder!=holder)return;
        TickOncePerFrame();
    }
    inline void OnFrontendTick() {
        if(!enabled || useStockTargeting)return;
        namespace S=Systems::TargetingPresentationRefreshSystem;
        Obj* holder=Pick3::Submission::Target(Pick3::Submission::matchHolderRoot);
        const auto previous=S::observedTargetingIdentity;
        if(!holder || previous.holder!=holder || !S::gameInputState.IsValid() || !S::getFirstEffectInQueue.IsValid())return;
        Obj* front=S::getFirstEffectInQueue[holder]();
        const auto next=S::ReadIdentity(holder,S::kSelectTriggeredEffectPrimaryTarget,front);
        Obj* model=LiveSelectionEntityModel();
        Obj* filter=next.valid?Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,next.effectEntityId),"PrimaryTargetFilter"):nullptr;
        const int owner=front && next.valid && Pick3::TargetBridge::blockingPlayerId.IsValid()?Pick3::TargetBridge::blockingPlayerId[front].Get():-1;
        const int local=Pick3::TargetBridge::LocalPlayerId();
        const bool ownPrimary=next.valid && next.phase==0 && local>=0 && owner==local &&
            Pick3::EnumModes::IsPickFromThree(Pick3::TargetBridge::LogicalSelectionMode(filter));
        const bool changed=Core::PresentationRefresh::NeedsRefresh(ownPrimary,
            {previous.effect,previous.state,previous.phase,previous.effectEntityId,previous.effectGroupId},
            {front,next.state,next.phase,next.effectEntityId,next.effectGroupId});
        if(changed && S::originalUpdate) {
            // Refresh only stock frontend state for the rooted holder's actual
            // local blocking choice. Never simulate a tap or submit a Play.
            S::UpdateHook(holder,nullptr);
            return;
        }
        if(hasRequest || Ui::state.shown || Ui::state.exiting || Ui::state.deferredExit ||
           Pick3::TargetBridge::HasPendingOffer() || Pick3::TargetBridge::HasQueuedChoice())TickOncePerFrame();
    }
    inline bool Initialize(bool requested,bool stockTarget,bool maskStock) {
        if(!requested)return true;
        Pick3::Submission::onMatchReset=&ResetMatchView;
        useStockTargeting=stockTarget;maskStockTargetUI=maskStock;
        suppressStockHint=maskStock;
        if(useStockTargeting) {
            // No Discover UI hooks in stock-only presentation mode.
            enabled=true;
            CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: stock-only input selected; no Discover UI hooks");
            return true;
        }
        Pick3CardMetadata::Initialize();
        // Keep the original custom _PickFromThreeTargeting localization.
        // Only game-driven SetCardData supplies prefab names; no invented
        // display key or unmanaged CardDataMapper getter call.
        Pick3CardMetadata::mapperCaptureActive=true;
        if(!Pick3CardMetadata::InstallMapperCapture())
            LOG_WARN("CustomComponents/Pick3/DiscoverUI: CardDataMapper snapshot hook unavailable; generic prompt fallback active");
        if(!Ui::Initialize())return false;
        frameCount=BNM::Class("UnityEngine","Time").GetMethod("get_frameCount",0).cast<int>();
        if(!frameCount.IsValid() || !Core::FrontendTickDispatcher::RegisterPresentationTick(&OnFrontendTick))return false;
        auto lights=BNM::Class("PvZCards.Game","HighlightTriggeredEffectTargetsCommand");
        activateTargetLight=lights.GetMethod("ActivateLightForTarget",4).cast<void>();
        if(!activateTargetLight.IsValid() || !Core::Hooks::Install(lights.GetMethod("ActivateLightsForChooseTargetEffect",2),TargetLights,originalTargetLights))return false;
        if (!Pick3CardMetadata::InstallLocalizationCapture(Ui::gcNew,Ui::gcFree))
            LOG_WARN("CustomComponents/Pick3/DiscoverUI: stock localization capture unavailable; generic prompt safe fallback (no scene scan)");
        Ui::showCards = &Utilities::DiscardPileViewer::ShowCardGuidList;
        Ui::showSignalType = BNM::Class("PvZCards.CardInspection", "ShowCardInspectionOverlaySignal").GetMonoType();
        CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: INSPECTION_BRIDGE installed=%d signalType=%d", Ui::showCards?1:0, Ui::showSignalType?1:0);
        Utilities::DiscardPileViewer::externalPickerTap=&PickTap;
        Utilities::DiscardPileViewer::externalAdditionalTap=&AdditionalBoardTap;
        Pick3::Submission::isStockChooseBoardTap=&ShouldForwardStockChooseTap;
        Pick3::Submission::onPick3PlayDispatch=&OnPick3PlayDispatch;
        if(!Systems::TargetingPresentationRefreshSystem::RegisterAfterSubmissionPresentationObserver(&OnStockHolderUpdated)) {
            Utilities::DiscardPileViewer::externalPickerTap=nullptr;
            Pick3::Submission::isStockChooseBoardTap=nullptr;
            Pick3::Submission::onPick3PlayDispatch=nullptr;
            LOG_ERR("CustomComponents/Pick3/DiscoverUI: stock GameInputStateHolder observer unavailable");return false;
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: TICK_SOURCE=STOCK_GAME_INPUT_HOLDER afterSubmission=1");
        enabled=true;
        CC_SELECTION_DIAG("CustomComponents/Pick3/DiscoverUI: port initialized stockTarget=%d maskStock=%d",stockTarget,maskStock);
        return true;
    }
#undef PICK3_DISCOVER_LOG
}


