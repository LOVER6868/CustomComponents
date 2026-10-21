#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// EXPERIMENTAL PICK3 UI. In external gameplay mode it submits target Play DTOs
// using the game-owned service. Must run on the Unity UI thread.
// Enabled only when [PickFromThreePreview] Enabled=true in config.ini.
// The existing FrontendTickDispatcher calls Tick AFTER stock EventSystem.Update.
// Uses user-packaged original inspection shell art independently of portrait
// availability. Static in-hand card art remains game-owned. The unsafe native
// avatar load and experimental inspection-prefab discovery stay isolated.

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <ctime>
#include <string>

#include "presentation/detail/Pick3RewardArt.hpp"
#include "presentation/detail/Pick3GameOwnedAssetRegistry.hpp"
#include "presentation/detail/Pick3AsyncAssetReceiver.hpp"
#include "presentation/detail/Pick3InspectionPrefabReceiver.hpp"
#include "presentation/detail/Pick3CardAssetsCapture.hpp"
#include "presentation/detail/Pick3SpriteCache.hpp"
#include "presentation/detail/Pick3CardMetadata.hpp"
#include "pick3/PickFromThreeTargetBridge.hpp"
#include "pick3/PickFromThreeCore.hpp"
#include "pick3/PickFromThreeSubmission.hpp"
#include "presentation/detail/Pick3NativeAvatarLoad.hpp"
#include "presentation/detail/Pick3DiagnosticMode.hpp"
#include "presentation/detail/Pick3MetadataIsolation.hpp"
#include "presentation/detail/Pick3PostOpenIsolation.hpp"
#include "presentation/detail/Pick3LocalAssetBundles.hpp"
#include "core/SceneViewContext.hpp"

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Exceptions.hpp>

#include "log.h"
#include "core/FrontendTickDispatcher.hpp"

namespace CustomComponents::Presentation::PickFromThreePreviewSystem {
    using Obj = BNM::IL2CPP::Il2CppObject;
    struct Vec2 { float x, y; };
    struct Vec3 { float x, y, z; };
    struct Color { float r, g, b, a; };
    struct Rect { float x, y, width, height; };
    // Layout screen coordinates, Y measured from the bottom (Unity Input).
    struct HitRect { float x, y, halfW, halfH; };

    // Use the native monotonic clock; no managed Unity Time API calls.
    inline std::uint64_t PerfClockMs() {
        struct timespec now{};
        if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
        return static_cast<std::uint64_t>(now.tv_sec) * 1000u +
               static_cast<std::uint64_t>(now.tv_nsec) / 1000000u;
    }
    inline constexpr const char* kRootName = "Sprig_PickFromThree_UI";
    inline bool enabled = false;
    inline bool externalOnly = false;
    inline int activeChoiceCount = 3;
    inline Pick3::PreviewRequest activeRequest{};
    inline bool requestActive = false;
    inline bool submissionServiceError = false;
    inline bool gameplaySubmissionEnabled = false;
    inline bool pvpTimeoutAttempted = false;
    inline Pick3::PickAccumulator picked;
    inline bool wasReady = false;
    inline bool notReadyLogged = false;
    inline bool showCloseButton = false; // opt-in: default to mandatory choice
    inline bool shownThisMatch = false;
    inline bool matchWasPresent = false;
    inline bool isOpen = false;
    // A rooted reference to OUR canvas, so Close() is not dependent on
    // GameObject.Find succeeding during a frontend/scene transition.
    inline std::uint32_t popupRootHandle = 0;
    inline bool verifyClosedNextTick = false;
    inline unsigned int frameCounter = 0;
    inline HitRect choiceRects[3]{};
    inline HitRect closeRect{};
    inline Obj* sceneFont = nullptr; // legacy fallback only; never substitutes for Cafeteria
    inline Obj* cafeteriaFont = nullptr; // borrowed scene resource, used during Open only
    inline BNM::Class tmpTextClass, tmpFontClass;
    inline BNM::Method<Obj*> tmpGetFont, tmpGetSharedMaterial;
    inline BNM::Method<void> tmpSetSharedMaterial;
    inline BNM::Method<float> tmpGetPreferredWidth, tmpGetPreferredHeight;
    inline Obj* cafeteriaMaterial = nullptr; // borrowed from a stock TMP label

    inline BNM::Method<void> tmpSetFont, tmpSetText, tmpSetFontSize,
                             tmpSetAlignment, tmpSetFontStyle, tmpSetAutoSize;
    inline BNM::Method<BNM::Structures::Mono::String*> getUnityObjectName;
    inline bool tmpReady = false;
    inline Obj* rewardSprites[Pick3RewardArt::Count]{};
    inline Obj* rewardTextures[Pick3RewardArt::Count]{};
    // Match-scoped, fixed-size fallback cache. Root sprites/textures while
    // their match is active and release handles on match teardown.
    inline std::uint32_t rewardSpriteRoots[Pick3RewardArt::Count]{};
    inline std::uint32_t rewardTextureRoots[Pick3RewardArt::Count]{};
    // Diagnostic only: avoid repeated native checks, which have been producing
    // false negatives on freshly created sprites on this game build.
    inline bool artCacheNativeCheckLogged[Pick3RewardArt::Count]{};
    inline bool cafeteriaNotFoundLogged = false;
    // Legacy prewarm machinery remains for source compatibility, but is
    // disabled unconditionally. Never create Unity textures or sprites in the
    // frontend idle dispatcher: use popup-demand loading instead.
    inline bool startupPrewarmEnabled = false;
    inline bool startupPrewarmFallbackSprites = false;
    inline int startupPrewarmStartFrames = 180;
    inline int startupPrewarmIntervalFrames = 12;
    inline int startupPrewarmMatchFrames = 180;
    inline unsigned int startupPrewarmTick = 0;
    inline unsigned int startupPrewarmLastStep = 0;
    inline unsigned int startupPrewarmMatchStable = 0;
    inline unsigned int startupPrewarmLastMatchCheck = 0;
    inline int startupPrewarmStage = 0; // CPU-only embedded PNG checks
    inline int startupPrewarmFallbackStage = 0;
    inline bool startupPrewarmCpuDone = false;
    inline bool startupPrewarmDone = false;
    inline bool startupPrewarmPngVerified[Pick3RewardArt::Count]{};
    inline std::array<bool,Pick3RewardArt::Count> artCacheReuseLogged{};
    inline bool sharedStaticReady = false;
    inline int sharedStaticReadyCount = 0;
    // CRASH ISOLATION: preserve the eight-item popup preload sequence from
    // the last Android-confirmed good build. The expanded 10-item preload and
    // 8-item match prewarm preceded a crash on the first picker after a match
    // transition. The evidence does not yet distinguish a stale native Sprite
    // from another Unity operation, so do NOT pre-create grass, full rim, or
    // split-rim halves across match boundaries in this diagnostic build.
    inline constexpr Pick3RewardArt::Kind kSharedStaticKinds[] = {
        Pick3RewardArt::InspectionGrass, Pick3RewardArt::InspectionReferenceRim,
        Pick3RewardArt::CostSun, Pick3RewardArt::CostBrain,
        Pick3RewardArt::AttackLeaf, Pick3RewardArt::HealthHeart,
        Pick3RewardArt::DescriptionPlate, Pick3RewardArt::InlineTooltipPlate
    };
    // CPU-only validation is still safe and useful for all ten known PNGs.
    inline constexpr Pick3RewardArt::Kind kCpuVerifyKinds[] = {
        Pick3RewardArt::InspectionGrass, Pick3RewardArt::InspectionReferenceRim,
        Pick3RewardArt::CostSun, Pick3RewardArt::CostBrain,
        Pick3RewardArt::AttackLeaf, Pick3RewardArt::HealthHeart,
        Pick3RewardArt::DescriptionPlate, Pick3RewardArt::InlineTooltipPlate,
        Pick3RewardArt::InspectionRimLeft, Pick3RewardArt::InspectionRimRight
    };
    // Four kinds were match-prewarmed successfully in the stable version;
    // keep each one on its own interval. Other assets remain popup-on-demand.
    inline constexpr Pick3RewardArt::Kind kMatchFallbackKinds[] = {
        Pick3RewardArt::CostSun, Pick3RewardArt::HealthHeart,
        Pick3RewardArt::DescriptionPlate, Pick3RewardArt::InlineTooltipPlate
    };
    // 0=disabled, 1=metadata-only (no factory/camera), 2=one native load
    // without RT, 3=one native RT preview, 4=up to 3 RT previews. Levels
    // 2-4 require two *explicit* user opt-ins and are unsafe on this APK.
    inline int avatarExperimentStage=0;
    inline int configuredAvatarExperimentStage=0;
    inline int avatarExperimentChoice=0;
    inline int avatarExperimentSpacingFrames=120;
    inline bool avatarExperimentPreflightLogged=false;
    inline unsigned int avatarExperimentLastStartFrame=0;
    inline int avatarExperimentNextIndex=0;
    inline bool avatarExperimentTimeoutLogged=false;
    inline void ConfigureAvatarExperiment(int stage,int firstChoice,int spacingFrames){
        configuredAvatarExperimentStage=avatarExperimentStage=std::clamp(stage,0,4);
        avatarExperimentChoice=std::clamp(firstChoice-1,0,2);
        avatarExperimentSpacingFrames=std::clamp(spacingFrames,0,600);
        LOG_INFO("CustomComponents/Pick3/ANIMATION_EXPERIMENT: CONFIG stage=%d firstOption=%d spacingFrames=%d nativeFactory=%d",avatarExperimentStage,avatarExperimentChoice+1,avatarExperimentSpacingFrames,avatarExperimentStage>=2?1:0);
    }
    inline void ConfigureStartupPrewarm(bool requested,int delayFrames,int intervalFrames,
                                       int matchFrames,bool allowFallbackSprites) {
        startupPrewarmEnabled=requested;
        startupPrewarmStartFrames=std::clamp(delayFrames,60,3600);
        startupPrewarmIntervalFrames=std::clamp(intervalFrames,1,120);
        startupPrewarmMatchFrames=std::clamp(matchFrames,60,3600);
        // Unity asset construction is never performed from the idle dispatcher.
        // Preserve CPU-only metadata checks when requested, regardless of old config.
        startupPrewarmFallbackSprites=false;
        if (allowFallbackSprites)
            LOG_INFO("CustomComponents/Pick3/PREWARM: ignoring legacy match asset prewarm request (crash isolation)");
        LOG_INFO("CustomComponents/Pick3/PREWARM: CONFIG enabled=%d cpuOnlyMenu=1 matchFallback=%d startFrames=%d intervalFrames=%d matchStableFrames=%d nativeEarly=0",
                 requested?1:0,allowFallbackSprites?1:0,startupPrewarmStartFrames,
                 startupPrewarmIntervalFrames,startupPrewarmMatchFrames);
    }
    // Original frame colors from the DefaultFrameColors serialized game asset.
    // The native class Sprite/Material pipeline is distinct from these values:
    // these are literal game asset values, not a reconstructed shader.
    inline Color CardClassColor(int oneBit){
        Pick3GameOwnedAssetRegistry::RGBA stock{};
        if (Pick3GameOwnedAssetRegistry::ClassColor(oneBit,stock))
            return Color{stock.r,stock.g,stock.b,stock.a};
        switch(oneBit){
            case 1: return {201.f/255.f,50.f/255.f,50.f/255.f,1.f};      // Kabloom
            case 2: return {53.f/255.f,174.f/255.f,69.f/255.f,1.f};     // Mega-Grow
            case 4: return {111.f/255.f,65.f/255.f,36.f/255.f,1.f};     // Guardian
            case 8: return {188.f/255.f,187.f/255.f,189.f/255.f,1.f};   // Smarty
            case 16: return {246.f/255.f,249.f/255.f,16.f/255.f,1.f};  // Solar
            case 32: return {182.f/255.f,58.f/255.f,168.f/255.f,1.f};  // Brainy
            case 64: return {223.f/255.f,162.f/255.f,30.f/255.f,1.f};  // Hearty
            case 128: return {68.f/255.f,67.f/255.f,67.f/255.f,1.f};   // Sneaky
            case 256: return {67.f/255.f,132.f/255.f,197.f/255.f,1.f}; // Hungry
            case 512: return {127.f/255.f,55.f/255.f,233.f/255.f,1.f}; // Madcap
            default: return {1.f,1.f,1.f,1.f};
        }
    }
    inline bool NativeFullUIEnabled(){
        return Pick3LocalAssetBundles::directBundleLoadingEnabled &&
               Pick3LocalAssetBundles::probeMode>=4;
    }
    // Legacy stock field probes (unused in the active artwork path). The new
    // registry GC-roots only verified Unity Sprite objects, never scene views.
    inline BNM::Class stockFrameClass, liveFrameClass, stockIconsClass, stockBgClass, stockColorSchemeClass, taggedMediatorClass, inlineConverterClass;
    inline BNM::Field<Obj*> stockPlantCost, stockZombieCost, stockIcons, stockFrames;
    inline BNM::Field<Obj*> stockAttackSprite, stockHealthSprite, stockFighterBackground, liveFrameSprites, liveFrameIcons, liveFrameScheme;
    inline BNM::Field<Obj*> mediatorConverter;
    inline BNM::Method<BNM::Structures::Mono::String*> convertTaggedText;
    inline Obj* gameSunIcon = nullptr;
    inline Obj* gameBrainIcon = nullptr;
    inline BNM::Field<Obj*> stockCostImage;
    inline BNM::Method<Obj*> imageGetSprite, spriteRendererGetSprite, getComponentInChildren;
    inline BNM::Class inspectionCircleClass, inspectionCostClass, spriteRendererClass;
    inline BNM::Field<Obj*> inspectionClassSprites[10];
    inline BNM::Field<Obj*> inspectionSunGO, inspectionBrainGO;
    inline Obj* nativeClassSprites[10]{}; // borrowed: no globally scanned Sprite objects
    inline Obj* inspectedCircleCache[10]{}; // game-texture-backed upper-circle sprites
    inline BNM::Method<Rect> nativeSpriteTextureRect;
    inline BNM::Method<Obj*> nativeSpriteTexture;
    inline BNM::Class inspectionPrefabReceiverClass, inspectorGameObjectClass;
    inline BNM::Method<Obj*> gameGetComponentsInChildren;
    inline BNM::Method<Obj*> prefabLoadAsync;
    inline BNM::Class inspectorActionClass;
    inline bool enableInspectionPrefabRequest=false;
    inline bool inspectionPrefabRequested=false,inspectionPrefabCompleted=false,inspectionPrefabHandled=false;
    inline unsigned int inspectionPrefabCompletedFrame=0;
    inline std::uint32_t inspectionPrefabReceiverRoot=0,inspectionPrefabActionRoot=0;
    inline std::uint32_t inspectionPrefabRoot=0;
    inline int inspectionLoadResult=-1; // -1 pending, 0 missing, 1 loaded
    inline void ConfigureInspectionPrefabLoading(bool enabled){
        // Independent static-art mode: never request or overlay a borrowed
        // inspection prefab even if an old phone config sets this true.
        if(enabled) LOG_INFO("CustomComponents/Pick3/STATIC_ASSETS: experimental prefab request ignored in self-owned mode");
        enableInspectionPrefabRequest=false;
    }
    inline void RequestInspectionPrefab();
    inline void ServiceInspectionPrefab();
    // Compatibility config retained for old installations. Only the grass
    // scale changes our self-contained, verified inspection shell.
    inline float inspectionGrassSize=.94f;
    inline bool useStockClassRimTint = true;

    inline int rimClassFlags[3]{-999,-999,-999};
    inline void ConfigureInspectionCircle(bool,int,int,int,bool,int grassSize){
        inspectionGrassSize=std::clamp(grassSize,75,100)/100.f;
    }
    inline bool nativeClassAssetsLogged=false;
    inline bool allowStockCostFallback=true;

    inline Obj* gameAttackIcon = nullptr;
    inline Obj* gameHealthIcon = nullptr;
    inline Obj* gameFighterBackground = nullptr;
    inline Obj* gameColorScheme = nullptr;
    inline BNM::Method<Color> gameGetColorForClass;
    inline bool submitPending = false;
    inline float submitAt = 0.f;
    inline bool exitAnimating = false;
    inline float exitStarted = 0.f;
    inline bool choiceTransitionActive=false;
    inline float choiceTransitionStarted=0.f;
    // Independent, time-based motion controls. Frame-independent durations
    // keep the selected reward visible even on high-refresh displays.
    inline float otherChoiceDismissSeconds=.24f;
    inline float chosenGlideSeconds=.40f;
    inline float chosenGlideExponent=3.5f;
    inline float chosenCenterHoldSeconds=.07f;
    inline float chosenFadeSeconds=.18f;
    inline float chosenArcHeight=.055f;
    inline float chosenPuffScale=.31f;
    inline float pressFeedbackSeconds = .70f;
    inline void ConfigureChoiceMotion(int glideMs, int glideExpoHundredths,
                                      int holdMs, int fadeMs, int dismissMs,
                                      int lowerArcPercent, int puffPercent) {
        chosenGlideSeconds=std::clamp(glideMs,150,1200)/1000.f;
        chosenGlideExponent=std::clamp(glideExpoHundredths,100,950)/100.f;
        chosenCenterHoldSeconds=std::clamp(holdMs,0,400)/1000.f;
        chosenFadeSeconds=std::clamp(fadeMs,80,600)/1000.f;
        otherChoiceDismissSeconds=std::clamp(dismissMs,100,1000)/1000.f;
        chosenArcHeight=std::clamp(lowerArcPercent,0,20)/100.f;
        chosenPuffScale=std::clamp(puffPercent,0,70)/100.f;
        // Submission cannot overtake the visual transition. Never shorten the
        // existing user-configured press-feedback interval.
        pressFeedbackSeconds=std::max(pressFeedbackSeconds,
            chosenGlideSeconds+chosenCenterHoldSeconds+chosenFadeSeconds+.02f);
        LOG_INFO("CustomComponents/Pick3/CHOICE_MOTION: glideMs=%d exponent=%.2f holdMs=%d fadeMs=%d dismissMs=%d pressMs=%d",
            static_cast<int>(chosenGlideSeconds*1000.f),chosenGlideExponent,
            static_cast<int>(chosenCenterHoldSeconds*1000.f),
            static_cast<int>(chosenFadeSeconds*1000.f),
            static_cast<int>(otherChoiceDismissSeconds*1000.f),
            static_cast<int>(pressFeedbackSeconds*1000.f));
    }
    // Per-option offsets are bounded to a few milliseconds. The sequence is
    // generated once per opening, not randomly changed on every frame.
    inline float revealTimingOffset[3]{};
    inline float dismissTimingOffset[3]{};
    inline void ResetChoiceTiming() {
        const unsigned int seed = static_cast<unsigned int>(PerfClockMs());
        for (int i=0; i<3; ++i) {
            const unsigned int r = (seed ^ (0x9e3779b9u * static_cast<unsigned int>(i+1)));
            revealTimingOffset[i] = static_cast<float>((r ^ (r >> 13)) % 13u) * .001f;
            dismissTimingOffset[i] = static_cast<float>(((r >> 7) ^ (r >> 17)) % 13u) * .001f;
        }
    }
    inline float exitSeconds = .18f;
    inline unsigned int stockArtRefreshCount = 0;
    // Never enumerate resident Unity assets while our live popup is open.
    // The previous repeated scan occasionally passed unloaded C# wrappers to
    // IL2CPP, causing unrecoverable native pointer dereferences.
    inline bool residentArtScanDisabled = true;
    // ADB experiment selectors. Read once per popup (not on every frame),
    // so managed requests and UI references cannot be switched mid-flight.
    // Missing/invalid selector uses the configured default.
    inline constexpr const char* kAsyncArtSelector =
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_async_art.txt";
    inline constexpr const char* kResidentArtSelector =
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_resident_art.txt";
    // Explicit experimental override. 0=respect safety suppression (default),
    // 1=allow ADB async selector to bypass the legacy diagnostic suppression.
    // Does not start requests unless pick3_async_art.txt is also 1.
    inline constexpr const char* kAsyncArtOverrideSelector =
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_async_override.txt";
    // ADB artwork tracer: 0=off, 1=summary/lookup, 2=summary + existing async stages.
    // This selector never starts asset requests by itself.
    inline constexpr const char* kArtTraceSelector =
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_art_trace.txt";
    inline int artTraceModeForPopup = 0;
    // Read-only diagnostics for loaded CardFaceSpriteHolder objects.
    inline constexpr const char* kHolderInspectSelector =
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_holder_inspect.txt";
    inline bool holderInspectForPopup = false;
    // Isolated native-card factory feasibility probe. 0=off; 1=API-only;
    // 2=match-bound DI; 3=signature readiness; 4=loader readiness;
    // 5=one native load; 6=one rendered preview; 7=up to three previews.
    // Modes 5-7 are intentionally unsafe opt-ins. 0-4 do not create cards.
    inline constexpr const char* kFactoryProbeSelector =
        "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_factory_probe.txt";
    inline int factoryProbeModeForPopup = 0;

    inline int ReadArtTraceMode() {
        int mode=0;
        if (FILE* f=std::fopen(kArtTraceSelector,"rb")) {
            if (std::fscanf(f,"%d",&mode)!=1) mode=0;
            std::fclose(f);
        }
        return std::clamp(mode,0,2);
    }
    inline bool configuredAsyncGameArt = false;
    inline bool residentArtEnabledForPopup = true;
    inline bool ReadArtSelector(const char* filename, bool defaultValue) {
        int value=-1;
        if (FILE* file=std::fopen(filename,"rb")) {
            if (std::fscanf(file,"%d",&value)!=1) value=-1;
            std::fclose(file);
        }
        return value==0 ? false : value==1 ? true : defaultValue;
    }

    inline std::string unformattedDescriptions[3]{};
    inline bool richDescriptionReady[3]{};
    // The game's TMP rich links are hit-tested without hooking EventSystem or
    // disturbing the Choose button's existing input path.
    struct TmpLinkInfo {
        int firstCharacterIndex, lastCharacterIndex, characterCount, padding;
        BNM::Structures::Mono::String* link;
        BNM::Structures::Mono::String* param;
    };
    inline BNM::Class tmpTextInfoClass, keywordInfoProviderClass;
    inline BNM::Method<Obj*> tmpGetTextInfo;
    inline BNM::Field<BNM::Structures::Mono::List<TmpLinkInfo>*> tmpLinks;
    inline BNM::Method<int> tmpFindLink;
    inline BNM::Method<BNM::Structures::Mono::String*> keywordDescription;
    inline bool inlineLinkApiReady=false, inlineTooltipShowing=false;
    inline bool inlineTooltipClosing=false;
    inline float inlineTooltipStart=0.f;
    inline float inlineTooltipCloseStart=0.f;
    inline float inlineTooltipCooldownUntil=0.f;
    inline int inlineTooltipTappedOption=-1;
    inline bool inlineTooltipTappedPulse=false;
    inline float inlineTooltipWidth=0.f, inlineTooltipHeight=0.f;
    inline float tooltipFontScale=1.f;
    inline float tooltipMaxWidthScale=.64f;
    inline float tooltipMaxHeightScale=.17f;
    inline float tooltipEnterSeconds=.12f,tooltipExitSeconds=.09f;
    inline void ConfigureTooltipStyle(int font,int width,int height,int enterMs,int exitMs){
        tooltipFontScale=std::clamp(font,70,160)/100.f;
        tooltipMaxWidthScale=std::clamp(width,40,85)/100.f;
        tooltipMaxHeightScale=std::clamp(height,8,30)/100.f;
        tooltipEnterSeconds=std::clamp(enterMs,40,350)/1000.f;
        tooltipExitSeconds=std::clamp(exitMs,40,350)/1000.f;
    }
    inline void AnimateInlineTooltip();
    inline void HandleDescriptionTap(Vec3 screenPoint, float screenW, float screenH);
    inline void HideDescriptionTooltip();
    inline BNM::Method<void> tmpSetWordWrap, tmpSetOverflowMode, tmpSetMinFontSize, tmpSetMaxFontSize, tmpSetRichText;
    inline void SetLabelValue(Obj* gameObject, const char* value);
    inline Obj* LoadRewardSprite(Pick3RewardArt::Kind kind);
    inline void CollectStockRewardSprites();
    inline void SetStockSprite(const char* prefix, int index, Obj* sprite, Color tint={1,1,1,1});
    inline void ApplyClassBackground(int index, int color);
    inline std::string FormatAbilityText(const std::string& source);
    inline std::string ReadableInlineLinks(std::string source);
    // No artificial asset-wait. Open the screen immediately on the first
    // picker request and animate the visual entrance independently of art loads.
    // The static card sprite remains available while native avatars load.
    inline float preloadSeconds = 0.f;
    inline float enterSeconds = .30f;
    inline float battlefieldDimAlpha = .42f;
    // Controls only the fullscreen scrim's visible darkness. Input blocking
    // is handled independently and remains active even at opacity zero.
    inline void ConfigureBattlefieldDimming(int opacityPercent) {
        battlefieldDimAlpha=std::clamp(opacityPercent,0,100)/100.f;
        LOG_INFO("CustomComponents/Pick3/OVERLAY: battlefieldDarkeningPercent=%d", 
                 static_cast<int>(std::lround(battlefieldDimAlpha*100.f)));
    }
    inline float presentationStart = 0.f;
    inline unsigned int presentationStartFrame = 0;
    inline bool presentationVisible = false;
    inline bool localizationResolved = false;
    inline BNM::Class canvasGroupClass;
    inline BNM::Method<void> setCanvasGroupAlpha, setLocalScale;
    inline BNM::Method<float> realtimeSinceStartup;
    inline bool entranceApiReady = false;
    // Read-only first native-avatar test. Before scheduling asynchronous
    // CardViewFactory creations, verify the *existing* injected factory and
    // the actual avatar animation methods in this match. Nothing is loaded
    // or instantiated by this probe; we must not risk a stray CardView
    // callback during match exit before its rendering/cancel path is ready.
    inline bool nativeAvatarProbeEnabled = true;
    inline bool nativeAvatarProbeResolved = false;
    inline bool nativeAvatarProbeReported = false;
    inline bool nativeAvatarLoadTestEnabled = false;
    inline int nativeAvatarLoadStartAttempts[3]{};
    inline bool nativeAvatarLoadStarted[3]{};
    inline bool nativeAvatarRenderBound[3]{};
    inline bool nativeAvatarRenderTestEnabled = false;
    inline int nativeAvatarProbeAttempts = 0;
    inline BNM::Class nativeCardViewFactoryClass, nativeCardViewConfigClass,
                      nativeAvatarViewClass, nativeCardViewClass;
    inline BNM::Method<Obj*> nativeFactoryCreateAsync;
    inline BNM::Method<void> nativeAvatarEnter, nativeAvatarIdle;
    inline BNM::Method<Obj*> nativeCardGetAvatar;

    inline void ProbeNativeAvatarFactory() {
        if ((factoryProbeModeForPopup < 2 && !nativeAvatarProbeEnabled) ||
            nativeAvatarProbeResolved || !isOpen || !requestActive ||
            activeChoiceCount<1 || nativeAvatarProbeAttempts>=12) return;
        if (nativeAvatarProbeAttempts>0 && frameCounter%15 != 0) return;
        ++nativeAvatarProbeAttempts;
        if(nativeAvatarProbeAttempts==1)
            LOG_INFO("CustomComponents/Pick3/NATIVE_AVATAR: PROBE_START guid=%d noAsyncCardViewAllocations=1",
                     activeRequest.options.empty()?-1:activeRequest.options[0].cardGuid);
        // It is important that this is the already-injected game binder,
        // not a freshly constructed, unbound CardViewFactory instance.
        if (factoryProbeModeForPopup >= 2 && nativeAvatarProbeAttempts == 1)
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: INSTANCE_STEP=BEFORE_BINDER_TARGET root=%d", Pick3::Submission::binderRoot != 0);
        Obj* binder=Pick3::Submission::Target(Pick3::Submission::binderRoot);
        if (factoryProbeModeForPopup >= 2 && nativeAvatarProbeAttempts == 1)
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: INSTANCE_STEP=AFTER_BINDER_TARGET binder=%d getInjection=%d", binder != nullptr, Pick3::Submission::getInjectionBinder.IsValid());
        Obj* injection=nullptr;
        if (binder && Pick3::Submission::getInjectionBinder.IsValid()) {
            if (factoryProbeModeForPopup >= 2 && nativeAvatarProbeAttempts == 1)
                LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: INSTANCE_STEP=BEFORE_GET_INJECTION");
            const auto exception=BNM::TryInvoke([&] {
                injection=Pick3::Submission::getInjectionBinder[binder]();
            });
            if(exception.IsValid()) injection=nullptr;
            if (factoryProbeModeForPopup >= 2 && nativeAvatarProbeAttempts == 1)
                LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: INSTANCE_STEP=AFTER_GET_INJECTION ok=%d exception=%d", injection != nullptr, exception.IsValid());
        }
        Obj* factory=nullptr;
        if(injection && nativeCardViewFactoryClass.IsValid() &&
           Pick3::Submission::getInjectedInstance.IsValid()) {
            if (factoryProbeModeForPopup >= 2 && nativeAvatarProbeAttempts == 1)
                LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: INSTANCE_STEP=BEFORE_GET_INJECTED_INSTANCE");
            const auto exception=BNM::TryInvoke([&] {
                factory=Pick3::Submission::getInjectedInstance[injection](
                    nativeCardViewFactoryClass.GetMonoType(),static_cast<Obj*>(nullptr));
            });
            if(exception.IsValid()) factory=nullptr;
            if (factoryProbeModeForPopup >= 2 && nativeAvatarProbeAttempts == 1)
                LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: INSTANCE_STEP=AFTER_GET_INJECTED_INSTANCE ok=%d exception=%d", factory != nullptr, exception.IsValid());
        }
        if (factoryProbeModeForPopup >= 2 && nativeAvatarProbeAttempts == 1)
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: INSTANCE_STEP=BEFORE_TYPE_CHECK factory=%d", factory != nullptr);
        if(factory && BNM::IsA(factory,nativeCardViewFactoryClass) &&
           nativeFactoryCreateAsync.IsValid() && nativeAvatarEnter.IsValid() &&
           nativeAvatarIdle.IsValid() && nativeCardGetAvatar.IsValid()) {
            nativeAvatarProbeResolved=true;
            LOG_INFO("CustomComponents/Pick3/NATIVE_AVATAR: FACTORY_READY option=1 guid=%d source=match-bound-DI avatarMethods=1 loadsStarted=0",
                     activeRequest.options.empty()?-1:activeRequest.options[0].cardGuid);
        } else if (nativeAvatarProbeAttempts>=12 && !nativeAvatarProbeReported) {
            nativeAvatarProbeReported=true;
            LOG_INFO("CustomComponents/Pick3/NATIVE_AVATAR: FACTORY_NOT_BOUND guid=%d binder=%d injection=%d factory=%d create=%d enter=%d idle=%d (no load requested)",
                activeRequest.options.empty()?-1:activeRequest.options[0].cardGuid,binder!=nullptr,injection!=nullptr,factory!=nullptr,
                nativeFactoryCreateAsync.IsValid(),nativeAvatarEnter.IsValid(),nativeAvatarIdle.IsValid());
        }
    }

    inline constexpr float cardEntranceStaggerSeconds = .075f;
    inline constexpr float cardEntranceDurationSeconds = .34f;
    inline float cardOriginX[3]{}, cardOriginY[3]{}, portraitOriginY[3]{};
    struct RewardLayout {
        float topY=.205f,lowerY=-.245f,sideGap=.242f;
        float portraitY=.275f,portraitSize=1.04f;
        float nameBelowStats=.80f,plateBelowName=.28f,buttonBelowPlate=.31f;
        float plateWidth=1.36f,plateHeight=.39f,buttonWidth=.98f,buttonHeight=1.20f;
        float badgeScale=.235f,statSpacing=.126f,statsBelowPortrait=.40f;
        float nameScale=1.40f,statScale=1.26f,buttonTextScale=1.08f;
        float buttonTextOffset=0.f;
    };
    inline RewardLayout layout{};
    inline void ConfigureLayout(int topPercent,int lowerPercent,int sidePercent,
                                int portraitYPercent,int portraitSizePercent,
                                int nameGapPercent,int plateGapPercent,int buttonGapPercent,
                                int plateWidthPercent,int plateHeightPercent,
                                int buttonWidthPercent,int buttonHeightPercent,
                                int nameSizePercent,int statSizePercent,int buttonTextPercent,
                                int buttonTextYOffset,int badgeSizePercent,
                                int statSpacingPercent,int statVerticalPercent) {
        layout.topY=std::clamp(topPercent,-35,45)/100.f;
        layout.lowerY=std::clamp(lowerPercent,-48,10)/100.f;
        layout.sideGap=std::clamp(sidePercent,16,40)/100.f;
        layout.portraitY=std::clamp(portraitYPercent,0,55)/100.f;
        layout.portraitSize=std::clamp(portraitSizePercent,70,135)/100.f;
        layout.nameBelowStats=std::clamp(nameGapPercent,45,130)/100.f;
        layout.plateBelowName=std::clamp(plateGapPercent,18,65)/100.f;
        layout.buttonBelowPlate=std::clamp(buttonGapPercent,15,60)/100.f;
        layout.plateWidth=std::clamp(plateWidthPercent,100,165)/100.f;
        layout.plateHeight=std::clamp(plateHeightPercent,28,55)/100.f;
        layout.buttonWidth=std::clamp(buttonWidthPercent,72,150)/100.f;
        layout.buttonHeight=std::clamp(buttonHeightPercent,75,160)/100.f;
        layout.nameScale=std::clamp(nameSizePercent,75,175)/100.f;
        layout.statScale=std::clamp(statSizePercent,70,170)/100.f;
        layout.buttonTextScale=std::clamp(buttonTextPercent,75,165)/100.f;
        layout.buttonTextOffset=std::clamp(buttonTextYOffset,-15,15)/100.f;
        layout.badgeScale=std::clamp(badgeSizePercent,15,36)/100.f;
        layout.statSpacing=std::clamp(statSpacingPercent,8,21)/100.f;
        layout.statsBelowPortrait=std::clamp(statVerticalPercent,22,57)/100.f;
    }
    // Per-element typography: font sizes remain relative to the stock
    // StatSizePercent for backwards compatibility. Offsets are percentages
    // of the individual icon size (description uses its plate size).
    struct StatTextLayout {
        float attackSize=1.1f,healthSize=1.1f,costSize=1.1f,descriptionSize=1.20f;
        float attackX=0.f,attackY=0.f,healthX=0.f,healthY=0.f;
        float costX=0.f,costY=0.f,descriptionX=0.f,descriptionY=0.f;
        float attackWidth=.90f,healthWidth=.90f,costWidth=.90f;
        float descriptionWidth=.93f,descriptionHeight=.87f;
    };
    inline StatTextLayout statText{};
    struct DescriptionLayout {
        bool autoSize=true, autoFit=true;
        float minWidth=.75f,maxWidth=1.55f,minHeight=.25f,maxHeight=.65f,padding=.08f;
    };
    inline DescriptionLayout descriptionLayout{};
    inline float descriptionPlateY[3]{}, descriptionButtonY[3]{}, descriptionCardW[3]{}, descriptionCardH[3]{};
    inline float descriptionBasePlateHeight[3]{};
    inline void ConfigureDescription(bool enabled,int minW,int maxW,int minH,int maxH,int padding,bool autoFit){
        descriptionLayout.autoSize=enabled;descriptionLayout.autoFit=autoFit;
        descriptionLayout.minWidth=std::clamp(minW,50,250)/100.f;
        descriptionLayout.maxWidth=std::max(descriptionLayout.minWidth,std::clamp(maxW,50,300)/100.f);
        descriptionLayout.minHeight=std::clamp(minH,10,150)/100.f;
        descriptionLayout.maxHeight=std::max(descriptionLayout.minHeight,std::clamp(maxH,10,180)/100.f);
        descriptionLayout.padding=std::clamp(padding,1,25)/100.f;
    }
    // Screen-space coordinates are authored in 720x1280 reference units and
    // scaled uniformly (min of width and height scale). No screen percentages:
    // this avoids different card geometry on phones with different aspects.
    inline int referenceWidth=720,referenceHeight=1280;
    inline void ConfigureReferenceResolution(int width,int height){
        referenceWidth=std::clamp(width,320,2560);
        referenceHeight=std::clamp(height,480,4096);
    }
    inline bool targetingBehindCards=false;
    inline float targetingX=0.f,targetingY=520.f,targetingFontScale=1.25f;
    inline void ConfigureTargetingText(bool behind,int x,int y,int size){
        targetingBehindCards=behind;
        targetingX=static_cast<float>(std::clamp(x,-1600,1600));
        targetingY=static_cast<float>(std::clamp(y,-2000,2000));
        targetingFontScale=std::clamp(size,55,250)/100.f;
    }
    // No extra "Choose a card" header. The localized targeting subtitle is
    // the single top prompt; dark shadow and bright outline-like face match
    // the stock targeting label's styling without altering its wording.
    inline void ResizeDescriptionPlate(int option);

    inline void ConfigureStatText(int attackSize,int healthSize,int costSize,int descriptionSize,
                                  int attackX,int attackY,int healthX,int healthY,
                                  int costX,int costY,int descriptionX,int descriptionY,
                                  int attackWidth,int healthWidth,int costWidth,
                                  int descriptionWidth,int descriptionHeight) {
        auto scale=[](int n,int lo,int hi){return std::clamp(n,lo,hi)/100.f;};
        statText.attackSize=scale(attackSize,35,220);
        statText.healthSize=scale(healthSize,35,220);
        statText.costSize=scale(costSize,35,220);
        statText.descriptionSize=scale(descriptionSize,35,220);
        statText.attackX=scale(attackX,-75,75); statText.attackY=scale(attackY,-75,75);
        statText.healthX=scale(healthX,-75,75); statText.healthY=scale(healthY,-75,75);
        statText.costX=scale(costX,-75,75); statText.costY=scale(costY,-75,75);
        statText.descriptionX=scale(descriptionX,-50,50);
        statText.descriptionY=scale(descriptionY,-50,50);
        statText.attackWidth=scale(attackWidth,45,150);
        statText.healthWidth=scale(healthWidth,45,150);
        statText.costWidth=scale(costWidth,45,150);
        statText.descriptionWidth=scale(descriptionWidth,55,145);
        statText.descriptionHeight=scale(descriptionHeight,55,145);
    }
    // Native artwork is a separate UGUI layer, not a child of the round
    // portrait. These are dimensions relative to the card width (not screen
    // pixels) so layouts remain comparable across phones/tablets.
    inline float nativeViewScale=2.5f,environmentViewScale=2.5f;
    inline float nativeViewHeightScale=1.f;

    inline float environmentViewOffsetY=0.f;
    inline float nativeArtworkOriginY[3]{},nativeArtworkCardWidth[3]{};
    inline void ConfigureNativeArtwork(int viewPercent,int environmentViewPercent,
                                       int environmentYOffsetPercent,int viewHeightPercent=100) {
        nativeViewScale=std::clamp(viewPercent,100,600)/100.f;
        environmentViewScale=std::clamp(environmentViewPercent,100,600)/100.f;
        environmentViewOffsetY=std::clamp(environmentYOffsetPercent,-150,150)/100.f;
        nativeViewHeightScale=1.f; // Square RenderTexture: never stretch vertically.
    }
    inline float cardUnit = 1.f;
    inline Obj* GetRect(Obj* go);
    // No target icon hiding or TargetIconViewComponent hooks. Gameplay input
    // remains guarded by Submission while this picker owns the card choice.
    inline BNM::Class gameObjectClass;
    inline BNM::Class rectClass;
    inline BNM::Class imageClass;
    inline BNM::Class textClass;
    inline BNM::Class canvasClass;
    inline BNM::Class raycasterClass;
    inline BNM::Class fontClass;
    inline BNM::Class localHandClass;
    inline BNM::Class typeClass;
    inline BNM::Class textureClass;
    inline BNM::Class spriteClass;
    inline BNM::Class byteClass;
    inline BNM::Method<bool> loadPng;
    inline BNM::Method<Obj*> spriteCreate;
    inline BNM::Method<void> setImageSprite;
    inline BNM::Method<void> setImagePreserveAspect;
    inline bool pngArtReady = false;
    // The game's CardFaceSpriteHolder is a ScriptableObject, not a game CardView.
    // FindObjectsOfTypeAll finds only holders that are ALREADY resident; this
    // test deliberately does not call the game's asynchronous bundle loader.
    inline BNM::Class faceHolderClass;
    inline BNM::Method<BNM::Structures::Mono::Array<Obj*>*> findLoadedUnityObjects;
    inline BNM::Method<Obj*> getFaceSprite;
    inline int previewCardIds[3]{114, 628, 684};
    inline bool cardFaceLookupReady = false;
    inline bool liveImageReady[3]{};
    inline int spriteProbeCount = 0;
    inline constexpr int kMaxSpriteProbes = 6;
    // Experimental ASYNC path. These are CardColor bit values for the sample
    // definitions (see config.ini docs); not entity IDs or card-position values.
    inline int previewColors[3]{4, 32, 128};
    inline bool useAsyncGameArt = true;
    inline bool probeLiveOwners = false;
    inline bool traceCardAssetsTask = false;
    inline bool nativeCacheTest = false;
    // The AntFarm sound wrapper is ONLY a discovery path. We never use its
    // LoadResourceAsync method. The injected HeroesSocialResourceLoader carries
    // the real PvZCards.ResourceLoader used by the asset-bundle interpreters.
    inline BNM::Class soundManagerClass;
    inline Obj* lastInspectedArtWrapper = nullptr;
    inline BNM::Class socialWrapperClass;
    inline BNM::Class socialBridgeClass;
    inline BNM::Class cardAssetLoaderClass;
    inline BNM::Field<Obj*> socialWrapperLoader;
    inline BNM::Method<Obj*> getPvzCardAssetLoader;
    inline BNM::Class callbackReceiverClass;
    inline BNM::Class actionOfCardFaceHolder;
    inline BNM::Method<Obj*> getSoundManager;
    inline BNM::Method<Obj*> getGameResourceLoader;
    inline BNM::Method<Obj*> loadGameResourceAsync;
    inline BNM::Method<BNM::Structures::Mono::String*> getHolderPathForColor;
    inline bool asyncMethodsReady = false;
    inline int asyncGeneration = 0;
    inline bool asyncRequested[3]{};
    inline bool asyncFinished[3]{};
    inline bool asyncApplied[3]{};
    inline std::uint32_t assetGCHandles[3]{};
    inline int asyncTickCount = 0;
    // A ResourceLoader request may outlive the match UI. Never free its
    // callback/receiver roots until the callback has actually returned.
    // Bound unresolved requests to avoid unlimited GCHandle growth if the
    // game never invokes a completion callback. No unsafe timeout release.
    struct AsyncInFlightRequest {
        std::uint32_t receiverRoot = 0;
        std::uint32_t delegateRoot = 0;
        int option = -1;
        int generation = -1;
        unsigned int submittedFrame = 0;
        unsigned int completedFrame = 0;
        bool callbackSeen = false;
        bool pendingLogged = false;
    };
    inline constexpr int kMaxAsyncInFlight = 24;
    inline constexpr unsigned int kPendingReportFrames = 600;
    inline AsyncInFlightRequest asyncInFlight[kMaxAsyncInFlight]{};
    // UnityEngine.Objects remain owned by Image->Sprite->Texture throughout the
    // life of the overlay; do not cache unmanaged pointers between matches.

    inline BNM::Method<Obj*> findByName;
    inline BNM::Method<Obj*> addComponent;
    inline BNM::Method<Obj*> getComponent;
    inline BNM::Method<Obj*> getTransform;
    inline BNM::Method<void> setGameObjectActive;
    inline BNM::Method<void> setParent;
    inline BNM::Method<void> destroyObject;
    inline BNM::Method<bool> unityObjectAlive;
    inline BNM::Method<void> setOwnedArtHideFlags; // UnityEngine.Object.set_hideFlags(int)
    inline BNM::Method<Obj*> findObjectOfType;
    inline BNM::Method<Obj*> getBuiltinResource;
    inline BNM::Method<Obj*> getFont;
    inline BNM::Method<void> setFont;
    inline BNM::Method<void> setText;
    inline BNM::Method<void> setFontSize;
    inline BNM::Method<void> setTextAnchor;
    inline BNM::Method<void> setFontStyle;
    inline BNM::Method<void> setBestFit;
    inline BNM::Method<void> setRaycastTarget;
    inline BNM::Method<void> setGraphicColor;
    inline BNM::Method<void> setAnchorMin;
    inline BNM::Method<void> setAnchorMax;
    inline BNM::Method<void> setPivot;
    inline BNM::Method<void> setSizeDelta;
    inline BNM::Method<void> setAnchoredPosition;
    inline BNM::Method<void> setCanvasRenderMode;
    inline BNM::Method<void> setCanvasOverrideSorting;
    inline BNM::Method<void> setCanvasSortingOrder;
    inline BNM::Method<int> screenWidth;
    inline BNM::Method<int> screenHeight;
    inline BNM::Method<bool> mouseDown;
    inline BNM::Method<Vec3> mousePosition;

    inline float PresentationClock() {
        if (realtimeSinceStartup.IsValid()) {
            const auto now = realtimeSinceStartup();
            if (std::isfinite(now)) return now;
        }
        // Old Unity versions without a real-time clock: approximate rather
        // than deadlock the picker. The expected build has realtimeSinceStartup.
        return static_cast<float>(frameCounter) / 60.f;
    }
    inline float PresentationAge() {
        return std::max(0.f, PresentationClock() - presentationStart);
    }
    inline bool EntranceComplete() { return presentationVisible; }
    inline void AnimateCardMotion() {
        // Animator-like state machine: Play (fade/scale/slide with overshoot)
        // transitions into Idle (subtle endless float), for EACH reward card.
        if (!isOpen || !setLocalScale.IsValid() || !findByName.IsValid()) return;
        const float age=PresentationAge();
        for (int i=0;i<activeChoiceCount;++i) {
            char name[40]{};
            std::snprintf(name,sizeof(name),"Pick3_AnimatedPortrait_%d",i+1);
            Obj* go=findByName(BNM::CreateMonoString(name));
            if(!go)continue;
            const float local=age-preloadSeconds-cardEntranceStaggerSeconds*i-revealTimingOffset[i];
            const float t=std::clamp(local/cardEntranceDurationSeconds,0.f,1.f);
            const float ease=1.f+2.15f*std::pow(t-1.f,3.f)+1.15f*std::pow(t-1.f,2.f);
            const float amount=std::max(0.f,ease);
            float scale=.66f+.34f*amount;
            float rise=(-24.f*(1.f-t))*cardUnit;
            if(t>=1.f){
                const float wave=PresentationClock()*1.55f+i*1.25f;
                scale=1.f+.006f*std::sin(wave);
                rise=2.f*cardUnit*std::sin(wave);
            }
            Obj* tr=getTransform[go]();
            if(tr)setLocalScale[tr](Vec3{scale,scale,1.f});
            Obj* rt=GetRect(go);
            if(rt)setAnchoredPosition[rt](Vec2{0.f,portraitOriginY[i]+rise});
            if (canvasGroupClass.IsValid() && setCanvasGroupAlpha.IsValid()) {
                Obj* group=getComponent[go](canvasGroupClass.GetMonoType());
                if(group)setCanvasGroupAlpha[group](t*t*(3.f-2.f*t));
            }
        }
    }
    inline void AnimateEntrance() {
        if (!isOpen) return;
        AnimateCardMotion();
        if (presentationVisible) return;
        const float age = PresentationAge();
        // Animate each entire option, not just its portrait. Its button,
        // card text, and decoration remain synchronized through the parent.
        for (int i=0; i<activeChoiceCount; ++i) {
            char name[40]{};
            std::snprintf(name,sizeof(name),"Pick3_RewardCard_%d",i+1);
            Obj* card=findByName.IsValid()?findByName(BNM::CreateMonoString(name)):nullptr;
            if (!card) continue;
            const float local=age-preloadSeconds-cardEntranceStaggerSeconds*i-revealTimingOffset[i];
            const float t=std::clamp(local/cardEntranceDurationSeconds,0.f,1.f);
            const float eased=t*t*(3.f-2.f*t);
            if (canvasGroupClass.IsValid() && setCanvasGroupAlpha.IsValid()) {
                Obj* group=getComponent[card](canvasGroupClass.GetMonoType());
                if (group)setCanvasGroupAlpha[group](eased);
            }
            if (setLocalScale.IsValid()) {
                Obj* transform=getTransform[card]();
                if (transform)setLocalScale[transform](Vec3{.96f+.04f*eased,.96f+.04f*eased,1.f});
            }
        }
        const float progress = std::clamp((age - preloadSeconds) / enterSeconds, 0.f, 1.f);
        const float eased = progress * progress * (3.f - 2.f * progress);
        Obj* go = findByName.IsValid() ? findByName(BNM::CreateMonoString("Pick3_Content")) : nullptr;
        if (go && entranceApiReady) {
            Obj* group = getComponent[go](canvasGroupClass.GetMonoType());
            if (group) setCanvasGroupAlpha[group](eased);
            Obj* transform = getTransform[go]();
            if (transform && setLocalScale.IsValid()) {
                // Small spring-like overshoot similar to the game's reward reveal.
                const float scale = .90f + .10f * eased +
                                    .045f * std::sin(3.14159265f*progress) * progress;
                setLocalScale[transform](Vec3{scale, scale, 1.f});
            }
        }
        if (age >= std::max(enterSeconds,cardEntranceDurationSeconds+
                   cardEntranceStaggerSeconds*std::max(0,activeChoiceCount-1)+.012f)) {
            presentationVisible = true;
            CC_DIAG("CustomComponents/Pick3/UI: REVEAL_DONE age=%.3f", age);
        }
    }

    inline bool HasMethods() {
        return findByName.IsValid() && addComponent.IsValid() &&
            getComponent.IsValid() && getTransform.IsValid() && setParent.IsValid() &&
            setGameObjectActive.IsValid() &&
            destroyObject.IsValid() && findObjectOfType.IsValid() &&
            getFont.IsValid() && setFont.IsValid() && setText.IsValid() &&
            setFontSize.IsValid() && setTextAnchor.IsValid() &&
            setRaycastTarget.IsValid() && setGraphicColor.IsValid() &&
            setAnchorMin.IsValid() && setAnchorMax.IsValid() && setPivot.IsValid() &&
            setSizeDelta.IsValid() && setAnchoredPosition.IsValid() &&
            setCanvasRenderMode.IsValid() && setCanvasSortingOrder.IsValid() &&
            screenWidth.IsValid() && screenHeight.IsValid() &&
            mouseDown.IsValid() && mousePosition.IsValid();
    }

    inline bool Contains(const HitRect& rect, float x, float y) {
        return std::fabs(x - rect.x) <= rect.halfW &&
               std::fabs(y - rect.y) <= rect.halfH;
    }

    // This diagnostic does not retain scene Unity objects across frame/scene
    // changes; locate the root by name whenever it must be removed.
    inline Obj* Root() {
        return findByName.IsValid() ? findByName(BNM::CreateMonoString(kRootName)) : nullptr;
    }

    inline void ReleaseAsyncAssets();

    // Disable the Canvas *synchronously* before Unity's deferred Destroy().
    // This also disables all of its Graphics' raycast targets immediately.
    // Using a GC handle allows cleanup even when GameObject.Find cannot see
    // the owned root (for example during a view transition). Managed exceptions
    // are reported rather than silently claiming that the canvas was removed.
    inline void DestroyPopupRoot(const char* reason, bool preserveHandle = false) {
        Obj* root = popupRootHandle && Core::SceneViewContext::getGCHandle
            ? Core::SceneViewContext::getGCHandle(popupRootHandle) : nullptr;
        if (!root) root = Root();
        if (root) {
            const auto deactivateError = BNM::TryInvoke([&] { setGameObjectActive[root](false); });
            if (deactivateError.IsValid()) {
                LOG_ERR("CustomComponents/Pick3Preview: deactivate FAILED reason=%s: %s",
                        reason, deactivateError.Message().c_str());
            }
            const auto destroyError = BNM::TryInvoke([&] { destroyObject(root); });
            if (destroyError.IsValid()) {
                LOG_ERR("CustomComponents/Pick3Preview: Destroy FAILED reason=%s: %s",
                        reason, destroyError.Message().c_str());
            }
            CC_DIAG("CustomComponents/Pick3Preview: canvas cleanup reason=%s root=%p deactivateOk=%d destroyOk=%d",
                     reason, root, deactivateError.IsValid() ? 0 : 1,
                     destroyError.IsValid() ? 0 : 1);
        } else {
            CC_DIAG("CustomComponents/Pick3Preview: canvas cleanup reason=%s root=absent", reason);
        }
        // Reward sprite/texture roots are released at match end, after the
        // popup has been deactivated; not on every picker close.
        if (!preserveHandle) {
            if (popupRootHandle && Core::SceneViewContext::freeGCHandle)
                Core::SceneViewContext::freeGCHandle(popupRootHandle);
            popupRootHandle = 0;
        }
    }

    inline void Close() {
        inlineTooltipShowing=false;
        inlineTooltipClosing=false;
        inlineTooltipTappedOption=-1;
        choiceTransitionActive=false;
        submitPending=false;
        exitAnimating=false;
        Pick3::Submission::SetPopupInputCapture(false);
        pvpTimeoutAttempted = false;
        ++asyncGeneration; // invalidate callbacks from the closed overlay
        ReleaseAsyncAssets();
        Pick3NativeAvatarLoad::EndSession();
        if (nativeCacheTest) Pick3SpriteCache::EndSession();
        // The dimmer must stop drawing/blocking touches before we report closed.
        DestroyPopupRoot("close", true);
        sceneFont = nullptr;
        cafeteriaFont = nullptr;
        presentationVisible = false;
        localizationResolved = false;
        isOpen = false;
        verifyClosedNextTick = true;
        CC_DIAG("CustomComponents/Pick3Preview: popup closed (canvas deactivated; verify next tick)");
    }

    inline Obj* CreateGO(const char* name, Obj* parent) {
        // Unity UI objects require RectTransform to be their ORIGINAL
        // transform component (not an AddComponent attempt to replace Transform).
        auto* components = typeClass.NewArray<Obj*>(1);
        if (!components) return nullptr;
        components->m_Items[0] = reinterpret_cast<Obj*>(rectClass.GetMonoType());
        Obj* result = gameObjectClass.CreateNewObjectParameters(
            BNM::CreateMonoString(name), components);
        if (!result) return nullptr;
        Obj* rect = getComponent[result](rectClass.GetMonoType());
        if (!rect) return nullptr;
        if (parent) {
            Obj* parentTransform = getTransform[parent]();
            if (!parentTransform) return nullptr;
            setParent[rect](parentTransform, false);
        }
        return result;
    }

    inline Obj* GetRect(Obj* go) {
        return go ? getComponent[go](rectClass.GetMonoType()) : nullptr;
    }

    inline void Position(Obj* go, float x, float y, float width, float height) {
        Obj* rect = GetRect(go);
        if (!rect) return;
        setAnchorMin[rect](Vec2{0.5f, 0.5f});
        setAnchorMax[rect](Vec2{0.5f, 0.5f});
        setPivot[rect](Vec2{0.5f, 0.5f});
        setSizeDelta[rect](Vec2{width, height});
        setAnchoredPosition[rect](Vec2{x, y});
    }

    inline Obj* Panel(Obj* parent, const char* name, float x, float y,
                      float w, float h, Color color, bool interceptInput = true) {
        Obj* go = CreateGO(name, parent);
        if (!go) return nullptr;
        Position(go, x, y, w, h);
        Obj* image = addComponent[go](imageClass.GetMonoType());
        if (!image) return nullptr;
        setGraphicColor[image](color);
        setRaycastTarget[image](interceptInput);
        return go;
    }

    inline bool Label(Obj* parent, const char* name, const char* label,
                      float x, float y, float w, float h,
                      int fontSize, Color color, bool bold = false,
                      bool shrinkToFit = true) {
        Obj* go = CreateGO(name, parent);
        if (!go) return false;
        Position(go, x, y, w, h);
        if (tmpReady && cafeteriaFont) {
            Obj* text = addComponent[go](tmpTextClass.GetMonoType());
            if (!text) return false;
            tmpSetFont[text](cafeteriaFont);
            if(cafeteriaMaterial && tmpSetSharedMaterial.IsValid())
                BNM::TryInvoke([&]{tmpSetSharedMaterial[text](cafeteriaMaterial);});
            tmpSetText[text](BNM::CreateMonoString(label));
            tmpSetFontSize[text](static_cast<float>(fontSize));
            tmpSetAlignment[text](5); // TMPro.TextAlignmentOptions.Center
            if (tmpSetFontStyle.IsValid()) tmpSetFontStyle[text](bold ? 1 : 0);
            // Stock description uses tighter tracking and leading. The pop-up's
            // UI scale differs, so use proportional TMP spacing instead of
            // copying the stock prefab's absolute units.
            if(std::strncmp(name,"Pick3_Desc_",11)==0){
                static BNM::Method<void> setCharacterSpacing, setLineSpacing;
                static bool resolved=false;
                if(!resolved){resolved=true;setCharacterSpacing=tmpTextClass.GetMethod("set_characterSpacing",1).cast<void>();
                    setLineSpacing=tmpTextClass.GetMethod("set_lineSpacing",1).cast<void>();}
                if(setCharacterSpacing.IsValid())BNM::TryInvoke([&]{setCharacterSpacing[text](-1.0f);});
                if(setLineSpacing.IsValid())BNM::TryInvoke([&]{setLineSpacing[text](-3.0f);});
            }
            if (tmpSetAutoSize.IsValid()) tmpSetAutoSize[text](shrinkToFit);
            if (tmpSetRichText.IsValid()) tmpSetRichText[text](true);
            if (tmpSetWordWrap.IsValid()) tmpSetWordWrap[text](shrinkToFit);
            if (tmpSetOverflowMode.IsValid()) tmpSetOverflowMode[text](3); // Truncate avoids missing-glyph markers on truncated TMP text
            if (tmpSetMinFontSize.IsValid()) tmpSetMinFontSize[text](shrinkToFit?std::max(9.f,fontSize*.78f):static_cast<float>(fontSize));
            if (tmpSetMaxFontSize.IsValid()) tmpSetMaxFontSize[text](static_cast<float>(fontSize));
            setGraphicColor[text](color);
            setRaycastTarget[text](std::strncmp(name,"Pick3_Desc_",11)==0);
            return true;
        }
        // Temporary fallback only if the localized Cafeteria asset is not yet
        // resident. This avoids a non-interactive blank popup on slow devices.
        Obj* text = addComponent[go](textClass.GetMonoType());
        if (!text) return false;
        if (sceneFont) setFont[text](sceneFont);
        setText[text](BNM::CreateMonoString(label));
        setFontSize[text](fontSize);
        setTextAnchor[text](4);
        if (setFontStyle.IsValid() && bold) setFontStyle[text](1);
        if (setBestFit.IsValid()) setBestFit[text](shrinkToFit);
        setGraphicColor[text](color);
        setRaycastTarget[text](false);
        return true;
    }

    // No bundled/sample portraits: missing or still-loading assets leave the
    // portrait area empty. The game's real sprite/animated prefab can fill it.

    // Diagnostic, read-only game asset lookup. This uses the same
    // CardFaceSpriteHolder.GetSprite(int) as the deck-editor visual pipeline,
    // but doesn't instantiate a cache or call asset-loading functions on the
    // gameplay thread. Missing sprites are expected until their bundles load.
    inline Obj* FindResidentCardFaceSprite(int cardId, int* inspected = nullptr) {
        if (inspected) *inspected = 0;
        if (!cardFaceLookupReady || cardId < 0) return nullptr;
        auto* objects = findLoadedUnityObjects(faceHolderClass.GetMonoType());
        if (!objects) return nullptr;
        const auto count = objects->GetCapacity();
        if (inspected) *inspected = static_cast<int>(count);
        for (BNM::IL2CPP::il2cpp_array_size_t n = 0; n < count; ++n) {
            Obj* holder = objects->m_Items[n];
            if (!holder) continue;
            Obj* sprite = getFaceSprite[holder](cardId);
            if (sprite) return sprite;
        }
        return nullptr;
    }

    // Real metadata is fetched separately from art. Missing artwork never
    // changes name, cost, stats or description back to mock data.
    inline void RelabelForGameSprite(int /*option*/, int /*cardId*/) {}

    // Cache the already-rendered candidate metadata for native animation.
    // Never reenter CardDataMapper from the avatar loader's UI tick.
    inline Pick3CardMetadata::CardInfo animationCardInfo[3]{};
    inline int animationCardInfoGuid[3]{-1,-1,-1};
    inline bool metadataLoaded[3]{};
    inline bool textLoaded[3]{};
    inline int metadataAttempts = 0;
    inline void UpdateCardMetadata() {
        if (!isOpen || !Root()) return;
        const int isolate=Pick3MetadataIsolation::mode;
        if (isolate==4) {
            Pick3MetadataIsolation::Trace("SKIP_ALL_METADATA",-1,0);
            return;
        }
        bool allReady = true;
        bool needsLocalization = false;
        for (int i = 0; i < activeChoiceCount; ++i) {
            allReady &= metadataLoaded[i] && textLoaded[i];
            needsLocalization |= !textLoaded[i];
        }
        if (allReady) return;
        Obj* loc = needsLocalization ? Pick3CardMetadata::FindLocalizationMapper() : nullptr;
        ++metadataAttempts;
        for (int i = 0; i < activeChoiceCount; ++i) {
            if (metadataLoaded[i] && textLoaded[i]) continue;
            Pick3MetadataIsolation::Trace("BEFORE_READ",i,previewCardIds[i]);
            const auto info = Pick3CardMetadata::Read(previewCardIds[i], loc);
            Pick3MetadataIsolation::Trace("AFTER_READ",i,previewCardIds[i]);
            if(info.hasCardData){
                animationCardInfo[i]=info;
                animationCardInfoGuid[i]=previewCardIds[i];
            }
            const char* prefixes[] = {"Pick3_Name_", "Pick3_Cost_", "Pick3_Stats_", "Pick3_Desc_"};
            const std::string* values[] = {&info.name, &info.cost, &info.stats, &info.description};
            for (int j = 0; j < 4; ++j) {
                char name[40]{};
                std::snprintf(name, sizeof(name), "%s%d", prefixes[j], i + 1);
                Obj* go = findByName(BNM::CreateMonoString(name));
                if (j==3) {
                    unformattedDescriptions[i]=*values[j];
                    const auto rich=ReadableInlineLinks(FormatAbilityText(*values[j]));
                    richDescriptionReady[i]=(rich!=*values[j] || values[j]->find('[')==std::string::npos);
                    SetLabelValue(go,rich.c_str());
                } else SetLabelValue(go, values[j]->c_str());
                if (j==3) ResizeDescriptionPlate(i);
                if (j == 0) {
                    char shadowName[40]{};
                    std::snprintf(shadowName, sizeof(shadowName), "Pick3_NameShadow_%d", i+1);
                    SetLabelValue(findByName(BNM::CreateMonoString(shadowName)), values[j]->c_str());
                }
            }
            if (info.hasCardData) {
                Pick3MetadataIsolation::Trace("BEFORE_BADGES",i,previewCardIds[i]);
                // Prefer verified game-owned, GC-rooted cost Sprites; fall back
                // to our embedded PNGs if the stock UI has not initialized or
                // if Unity has invalidated a retained native Sprite.
                // Faction comes from CardData, not the class-color flags.
                char badgeName[40]{};
                std::snprintf(badgeName,sizeof(badgeName),"Pick3_CostBadge_%d",i+1);
                Obj* badgeGO=findByName(BNM::CreateMonoString(badgeName));
                Obj* badgeImage=badgeGO?getComponent[badgeGO](imageClass.GetMonoType()):nullptr;
                if(badgeImage){
                    Obj* sprite=LoadRewardSprite(info.zombie?Pick3RewardArt::CostBrain:
                                                           Pick3RewardArt::CostSun);
                    if(sprite){
                        setImageSprite[badgeImage](sprite);
                        setGraphicColor[badgeImage](Color{1.f,1.f,1.f,1.f});
                    }else setGraphicColor[badgeImage](Color{1.f,1.f,1.f,0.f});
                }
                Pick3MetadataIsolation::Trace("AFTER_COST_BADGE",i,previewCardIds[i]);
                if (isolate!=1 && isolate!=3) {
                    Pick3MetadataIsolation::Trace("BEFORE_CLASS_RIM",i,previewCardIds[i]);
                    ApplyClassBackground(i,info.cardColor);
                    Pick3MetadataIsolation::Trace("AFTER_CLASS_RIM",i,previewCardIds[i]);
                } else Pick3MetadataIsolation::Trace("SKIP_CLASS_RIM",i,previewCardIds[i]);
                if (isolate!=2 && isolate!=3 && info.fighter && Pick3GameOwnedAssetRegistry::useInPicker) {
                    Pick3MetadataIsolation::Trace("BEFORE_STOCK_ICONS",i,previewCardIds[i]);
                    char statBadge[48]{};
                    const int icons[2]={info.attackIconType,info.healthIconType};
                    const char* prefix[2]={"Pick3_AttackBadge_","Pick3_HealthBadge_"};
                    for (int stat=0;stat<2;++stat) {
                        Obj* original=Pick3GameOwnedAssetRegistry::Icon(icons[stat]);
                        if (!original) continue; // Keep previously rendered basic icon.
                        std::snprintf(statBadge,sizeof(statBadge),"%s%d",prefix[stat],i+1);
                        Obj* badgeGO=findByName(BNM::CreateMonoString(statBadge));
                        Obj* badge=badgeGO?getComponent[badgeGO](imageClass.GetMonoType()):nullptr;
                        if (!badge) continue;
                        setImageSprite[badge](original);
                        if (icons[stat]!=stat)
                            setGraphicColor[badge](Color{1.f,1.f,1.f,1.f});
                    }
                    if (info.iconDecidersUsed)
                        LOG_INFO("CustomComponents/Pick3/STOCK_TRAITS: option=%d card=%d attackType=%d healthType=%d",
                                 i+1,previewCardIds[i],info.attackIconType,info.healthIconType);
                }
                Pick3MetadataIsolation::Trace("AFTER_STOCK_ICONS",i,previewCardIds[i]);
                char name[40]{};
                Pick3MetadataIsolation::Trace("BEFORE_STAT_LABELS",i,previewCardIds[i]);
                if (info.fighter) {
                    int attack = 0, health = 0;
                    if (std::sscanf(info.stats.c_str(), "%d / %d", &attack, &health) == 2) {
                        char number[24]{};
                        std::snprintf(name, sizeof(name), "Pick3_Attack_%d", i + 1);
                        Obj* attackGo = findByName(BNM::CreateMonoString(name));
                        std::snprintf(number, sizeof(number), "%d", attack);
                        SetLabelValue(attackGo, number);
                        std::snprintf(name, sizeof(name), "Pick3_Health_%d", i + 1);
                        Obj* healthGo = findByName(BNM::CreateMonoString(name));
                        std::snprintf(number, sizeof(number), "%d", health);
                        SetLabelValue(healthGo, number);
                    }
                } else {
                    for (const char* prefix : {"Pick3_AttackBadge_", "Pick3_HealthBadge_",
                                               "Pick3_Attack_", "Pick3_Health_"}) {
                        std::snprintf(name, sizeof(name), "%s%d", prefix, i + 1);
                        Obj* graphic = findByName(BNM::CreateMonoString(name));
                        if (graphic) setGameObjectActive[graphic](false);
                    }
                }
            }
            Pick3MetadataIsolation::Trace("AFTER_STAT_LABELS",i,previewCardIds[i]);
            metadataLoaded[i] = info.hasCardData;
            textLoaded[i] = info.localizedName && info.localizedDescription;
        }
    }

    // A closed scene invalidates the UI and releases loaded assets, but does
    // NOT release callback/receiver roots belonging to unresolved requests.
    // An outstanding managed Action may still be invoked by ResourceLoader.
    inline void ReleaseAsyncAssets() {
        for (int i = 0; i < 3; ++i) {
            if (assetGCHandles[i] && Core::SceneViewContext::freeGCHandle)
                Core::SceneViewContext::freeGCHandle(assetGCHandles[i]);
            assetGCHandles[i] = 0;
            asyncRequested[i] = asyncFinished[i] = asyncApplied[i] = false;
        }
    }

    inline AsyncInFlightRequest* FindAsyncRequest(int option, int generation) {
        for (auto& req : asyncInFlight)
            if (req.delegateRoot && req.option == option && req.generation == generation)
                return &req;
        return nullptr;
    }

    inline AsyncInFlightRequest* FreeAsyncRequestSlot() {
        for (auto& req : asyncInFlight)
            if (!req.delegateRoot && !req.receiverRoot) return &req;
        return nullptr;
    }

    // Called on the frontend tick, not from inside a managed delegate call:
    // the callback's receiver stays rooted until OnLoaded has fully returned.
    // Timeout is diagnostic ONLY: without a confirmed cancellation we cannot
    // safely dispose an Action that ResourceLoader might still invoke.
    inline void ServiceAsyncRequests() {
        for (auto& req : asyncInFlight) {
            if (!req.delegateRoot) continue;
            // Leave two additional UI ticks after the native callback handoff
            // before releasing its managed target/delegate GCHandles.
            if (req.callbackSeen && frameCounter - req.completedFrame >= 2) {
                CC_DIAG("CustomComponents/Pick3Preview/ASYNC: RELEASE option=%d gen=%d (callback completed)",
                         req.option + 1, req.generation);
                if (Core::SceneViewContext::freeGCHandle) {
                    Core::SceneViewContext::freeGCHandle(req.delegateRoot);
                    Core::SceneViewContext::freeGCHandle(req.receiverRoot);
                }
                req = {};
            } else if (!req.callbackSeen) {
                if (!req.pendingLogged &&
                    frameCounter - req.submittedFrame >= kPendingReportFrames) {
                    req.pendingLogged = true;
                    CC_DIAG("CustomComponents/Pick3Preview/ASYNC: PENDING option=%d gen=%d frames=%u; roots retained (timeout is not cancellation)",
                             req.option + 1, req.generation, frameCounter - req.submittedFrame);
                }
            }
        }
    }

    inline void OnGameAssetLoaded(int index, int generation, Obj* asset) {
        if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: CALLBACK_ENTER option=%d gen=%d asset=%d",index+1,generation,asset?1:0);
        AsyncInFlightRequest* request = FindAsyncRequest(index, generation);
        if (!request) {
            CC_DIAG("CustomComponents/Pick3Preview/ASYNC: callback UNTRACKED option=%d gen=%d asset=%p",
                     index + 1, generation, asset);
            return;
        }
        if (request->callbackSeen) {
            CC_DIAG("CustomComponents/Pick3Preview/ASYNC: callback DUPLICATE option=%d gen=%d",
                     index + 1, generation);
            return;
        }
        // Mark for cleanup by a later Tick; do not free our own receiver
        // while its managed OnLoaded method is currently executing.
        request->completedFrame = frameCounter;
        request->callbackSeen = true;
        const bool current = index >= 0 && index < 3 &&
                             generation == asyncGeneration && isOpen;
        if (!current) {
            CC_DIAG("CustomComponents/Pick3Preview/ASYNC: callback STALE option=%d gen=%d asset=%p; UI untouched",
                     index + 1, generation, asset);
            return;
        }
        if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: CALLBACK_CURRENT option=%d gen=%d",index+1,generation);
        asyncFinished[index] = true;
        const bool isHolder = asset && faceHolderClass.IsValid() &&
                              BNM::IsA(asset, faceHolderClass);
        if (isHolder && Core::SceneViewContext::newGCHandle)
            assetGCHandles[index] = Core::SceneViewContext::newGCHandle(asset, false);
        CC_DIAG("CustomComponents/Pick3Preview/ASYNC: callback option=%d cardId=%d asset=%p status=%s holder=%d rooted=%d",
                 index + 1, previewCardIds[index], asset,
                 !asset ? "NULL" : (isHolder ? "HOLDER" : "WRONG_TYPE"),
                 isHolder ? 1 : 0, assetGCHandles[index] ? 1 : 0);
    }

    inline bool NativeArtOnly() {
        return avatarExperimentStage>=3 && nativeAvatarRenderTestEnabled && Pick3NativeAvatarRender::ready;
    }
    inline void BeginGameArtRequests() {
        const bool nativeOnly = NativeArtOnly();
        if (artTraceModeForPopup >= 2 && asyncTickCount == 0)
            LOG_INFO("CustomComponents/Pick3/ART_TRACE: DISPATCH_CHECK nativeOnly=%d open=%d enabled=%d ready=%d choices=%d",
                     nativeOnly?1:0, isOpen?1:0, useAsyncGameArt?1:0, asyncMethodsReady?1:0, activeChoiceCount);
        if (nativeOnly || !isOpen || !useAsyncGameArt || !asyncMethodsReady) {
            if (artTraceModeForPopup >= 2 && asyncTickCount == 0)
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: DISPATCH_SKIP gate nativeOnly=%d open=%d enabled=%d ready=%d",
                         nativeOnly?1:0,isOpen?1:0,useAsyncGameArt?1:0,asyncMethodsReady?1:0);
            return;
        }
        Obj* soundManager = getSoundManager();
        Obj* wrapper = soundManager ? getGameResourceLoader[soundManager]() : nullptr;
        // Read-only wrapper RTTI. Never call a loader method on a mismatched type.
        if (artTraceModeForPopup >= 2 && wrapper &&
            !BNM::IsA(wrapper, socialWrapperClass) &&
            lastInspectedArtWrapper != wrapper) {
            lastInspectedArtWrapper = wrapper;
            auto actual = BNM::Class(wrapper);
            auto* actualRaw = actual.IsValid() ? actual.GetClass() : nullptr;
            auto* expectedRaw = socialWrapperClass.IsValid() ? socialWrapperClass.GetClass() : nullptr;
            LOG_INFO("CustomComponents/Pick3/ART_TRACE: WRAPPER_TYPE actual=%s.%s expected=%s.%s exact=%d",
                     actualRaw && actualRaw->namespaze ? actualRaw->namespaze : "?",
                     actualRaw && actualRaw->name ? actualRaw->name : "?",
                     expectedRaw && expectedRaw->namespaze ? expectedRaw->namespaze : "?",
                     expectedRaw && expectedRaw->name ? expectedRaw->name : "?",
                     actualRaw && actualRaw == expectedRaw ? 1 : 0);
            auto parent = actual.IsValid() ? actual.GetParent() : BNM::Class();
            for (int depth = 0; depth < 5 && parent.IsValid(); ++depth) {
                auto* raw = parent.GetClass();
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: WRAPPER_PARENT depth=%d type=%s.%s expected=%d",
                         depth + 1,
                         raw && raw->namespaze ? raw->namespaze : "?",
                         raw && raw->name ? raw->name : "?",
                         raw && raw == expectedRaw ? 1 : 0);
                parent = parent.GetParent();
            }
            if (actual.IsValid()) {
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: WRAPPER_METHODS LoaderField=%d get_ResourceLoader=%d LoadResourceAsync=%d",
                         actual.GetField("Loader").IsValid() ? 1 : 0,
                         actual.GetMethod("get_ResourceLoader", 0).IsValid() ? 1 : 0,
                         actual.GetMethod("LoadResourceAsync", 2).IsValid() ? 1 : 0);
            }
        }
        // SoundBankManager.get_ResourceLoader() returns the ResourceLoader
        // interface. On this game build its concrete object is usually
        // PvZCards.Common.IdInterpreterLoader, NOT AntFarm.ResourceLoader.
        // The generic LoadResourceAsync<CardFaceSpriteHolder> method below is
        // resolved against IdInterpreterLoader, so invoke it only on a proven
        // IdInterpreterLoader instance. Default OFF until ADB opt-in.
        int directLoaderMode = 0;
        if (FILE* f = std::fopen(
                "/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_direct_loader.txt", "rb")) {
            int value = 0;
            if (std::fscanf(f, "%d", &value) == 1 && value == 1) directLoaderMode = 1;
            std::fclose(f);
        }
        Obj* loader = nullptr;
        const bool isDirectCardLoader = wrapper && BNM::IsA(wrapper, cardAssetLoaderClass);
        if (artTraceModeForPopup >= 2 && asyncTickCount == 0)
            LOG_INFO("CustomComponents/Pick3/ART_TRACE: LOADER_ROUTE optIn=%d wrapper=%d directType=%d legacyType=%d",
                     directLoaderMode, wrapper ? 1 : 0, isDirectCardLoader ? 1 : 0,
                     wrapper && BNM::IsA(wrapper, socialWrapperClass) ? 1 : 0);
        if (directLoaderMode == 1 && isDirectCardLoader) {
            loader = wrapper;
            if (artTraceModeForPopup >= 2 && asyncTickCount == 0)
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: LOADER_ROUTE selected=direct-id-interpreter");
        } else {
            // Preserve original AntFarm -> HeroesSocialResourceLoader path
            // exactly when direct loading is not selected.
            if (!wrapper || !BNM::IsA(wrapper, socialWrapperClass)) {
                if (artTraceModeForPopup >= 2 && asyncTickCount == 0)
                    LOG_INFO("CustomComponents/Pick3/ART_TRACE: DISPATCH_SKIP wrapper manager=%d wrapper=%d typeOK=%d directOptIn=%d",
                             soundManager ? 1 : 0, wrapper ? 1 : 0,
                             wrapper && BNM::IsA(wrapper, socialWrapperClass) ? 1 : 0, directLoaderMode);
                return;
            }
            Obj* bridge = socialWrapperLoader[wrapper].Get();
            if (!bridge || !BNM::IsA(bridge, socialBridgeClass)) {
                if (artTraceModeForPopup >= 2)
                    LOG_INFO("CustomComponents/Pick3/ART_TRACE: DISPATCH_SKIP bridge present=%d typeOK=%d",
                             bridge ? 1 : 0, bridge && BNM::IsA(bridge, socialBridgeClass) ? 1 : 0);
                return;
            }
            loader = getPvzCardAssetLoader[bridge]();
            if (!loader || !BNM::IsA(loader, cardAssetLoaderClass)) {
                if (artTraceModeForPopup >= 2)
                    LOG_INFO("CustomComponents/Pick3/ART_TRACE: DISPATCH_SKIP cardLoader present=%d typeOK=%d",
                             loader ? 1 : 0, loader && BNM::IsA(loader, cardAssetLoaderClass) ? 1 : 0);
                return;
            }
        }
        if (artTraceModeForPopup >= 2 && asyncTickCount == 0)
            LOG_INFO("CustomComponents/Pick3/ART_TRACE: LOADER_ROUTE verified=1 loader=%p", loader);
        if (!Core::SceneViewContext::newGCHandle || !Core::SceneViewContext::getGCHandle ||
            !Core::SceneViewContext::freeGCHandle) {
            if (artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: DISPATCH_SKIP gcHandles unavailable");
            CC_DIAG("CustomComponents/Pick3Preview/ASYNC: GC handles unavailable; skipping requests");
            return;
        }
        Pick3AsyncAssetReceiver::deliver = &OnGameAssetLoaded;
        // One in-flight request at a time. Avoid three simultaneous managed
        // loader invocations from a game-effect popup. A completed callback
        // releases its roots in ServiceAsyncRequests on a later UI tick.
        for (const auto& pending : asyncInFlight) {
            if (pending.delegateRoot) {
                if(artTraceModeForPopup>=2 && asyncTickCount == 0)
                    LOG_INFO("CustomComponents/Pick3/ART_TRACE: DISPATCH_SKIP inFlight option=%d gen=%d callback=%d",pending.option+1,pending.generation,pending.callbackSeen?1:0);
                return;
            }
        }
        for (int i = 0; i < activeChoiceCount; ++i) {
            if (asyncRequested[i] || liveImageReady[i]) {
                if(artTraceModeForPopup>=2 && asyncTickCount == 0)
                    LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPTION_SKIP option=%d requested=%d live=%d",i+1,asyncRequested[i]?1:0,liveImageReady[i]?1:0);
                continue;
            }
            AsyncInFlightRequest* slot = FreeAsyncRequestSlot();
            if (!slot) {
                if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPTION_SKIP option=%d noFreeSlot",i+1);
                CC_DIAG("CustomComponents/Pick3Preview/ASYNC: CAP_REACHED option=%d cap=%d; pending roots preserved",
                         i + 1, kMaxAsyncInFlight);
                continue;
            }
            auto* path = getHolderPathForColor(previewColors[i]);
            if (!path) {
                if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPTION_SKIP option=%d pathMissing color=%d",i+1,previewColors[i]);
                CC_DIAG("CustomComponents/Pick3Preview/ASYNC: option=%d color=%d path=MISS",
                         i + 1, previewColors[i]);
                continue;
            }
            auto* receiver = reinterpret_cast<Pick3AsyncAssetReceiver::Receiver*>(
                callbackReceiverClass.CreateNewInstance());
            if (!receiver) {
                if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPTION_SKIP option=%d receiverFailed",i+1);
                CC_DIAG("CustomComponents/Pick3Preview/ASYNC: option=%d receiver=FAIL", i + 1);
                continue;
            }
            const std::uint32_t receiverRoot =
                Core::SceneViewContext::newGCHandle(reinterpret_cast<Obj*>(receiver), false);
            if (!receiverRoot) { if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPTION_SKIP option=%d receiverRootFailed",i+1); continue; }
            receiver->option = i;
            receiver->generation = asyncGeneration;
            auto callbackMethod = callbackReceiverClass.GetMethod("OnLoaded", 1);
            callbackMethod.SetInstance(receiver);
            auto* delegateShell = reinterpret_cast<BNM::DelegateBase*>(actionOfCardFaceHolder.CreateNewInstance());
            auto* callback = delegateShell && callbackMethod.IsValid()
                ? delegateShell->Create(callbackMethod) : nullptr;
            if (!callback) {
                Core::SceneViewContext::freeGCHandle(receiverRoot);
                if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPTION_SKIP option=%d delegateFailed",i+1);
                CC_DIAG("CustomComponents/Pick3Preview/ASYNC: option=%d delegate=FAIL", i + 1);
                continue;
            }
            const std::uint32_t delegateRoot =
                Core::SceneViewContext::newGCHandle(reinterpret_cast<Obj*>(callback), false);
            if (!delegateRoot) {
                Core::SceneViewContext::freeGCHandle(receiverRoot);
                if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPTION_SKIP option=%d delegateRootFailed",i+1);
                continue;
            }
            *slot = AsyncInFlightRequest{receiverRoot, delegateRoot, i,
                                         asyncGeneration, frameCounter, 0, false, false};
            asyncRequested[i] = true;
            const std::string pathUtf8 = path->str();
            if(artTraceModeForPopup>=2)
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: REQUEST option=%d guid=%d color=%d path='%s'",
                         i+1,previewCardIds[i],previewColors[i],pathUtf8.c_str());
            CC_DIAG("CustomComponents/Pick3Preview/ASYNC: REQUEST option=%d cardId=%d color=%d path='%s' loader=%p rooted=1",
                     i + 1, previewCardIds[i], previewColors[i], pathUtf8.c_str(), loader);
            // Do NOT free delegateRoot or receiverRoot here. The game may only
            // hold a weak/unrooted callback and complete on a later frame.
            Obj* loadRequest = loadGameResourceAsync[loader](path, reinterpret_cast<Obj*>(callback));
            if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: REQUEST_RETURN option=%d request=%d callbackSeen=%d",i+1,loadRequest?1:0,slot->callbackSeen?1:0);
            CC_DIAG("CustomComponents/Pick3Preview/ASYNC: CALL_RETURN option=%d gen=%d request=%p callbackSeen=%d",
                     i + 1, asyncGeneration, loadRequest, slot->callbackSeen ? 1 : 0);
            // The game owns the ResourceLoadRequest. A null request may mean
            // immediate completion or an error; the callback log is decisive.
            break; // only one new request per invocation
        }
    }

    inline void ApplyAsyncGameArt() {
        if (NativeArtOnly() || !isOpen || !Root() || !getFaceSprite.IsValid()) return;
        for (int i = 0; i < 3; ++i) {
            if (!asyncFinished[i] || asyncApplied[i]) continue;
            if (!assetGCHandles[i]) { // null asset or wrong returned type
                asyncApplied[i] = true;
                CC_DIAG("CustomComponents/Pick3Preview/ASYNC: option=%d cardId=%d sprite=UNAVAILABLE (callback completed)",
                         i + 1, previewCardIds[i]);
                continue;
            }
            Obj* holder = Core::SceneViewContext::getGCHandle(assetGCHandles[i]);
            if (holder && holderInspectForPopup) {
                auto actual = BNM::Class(holder);
                auto* raw = actual.IsValid() ? actual.GetClass() : nullptr;
                const bool expected = faceHolderClass.IsValid() && BNM::IsA(holder, faceHolderClass);
                LOG_INFO("CustomComponents/Pick3/HOLDER_INSPECT: option=%d guid=%d type=%s.%s expectedType=%d",
                         i+1, previewCardIds[i],
                         raw && raw->namespaze ? raw->namespaze : "?",
                         raw && raw->name ? raw->name : "?", expected ? 1 : 0);
                if (expected) {
                    auto spritesField = faceHolderClass.GetField("CardFaceSprites").cast<Obj*>();
                    Obj* sprites = spritesField.IsValid() ? spritesField[holder].Get() : nullptr;
                    LOG_INFO("CustomComponents/Pick3/HOLDER_INSPECT: option=%d field=%d list=%d",
                             i+1, spritesField.IsValid()?1:0, sprites?1:0);
                    if (sprites) {
                        auto collectionClass = BNM::Class(sprites);
                        auto getCount = collectionClass.IsValid() ? collectionClass.GetMethod("get_Count", 0).cast<int>() : BNM::Method<int>();
                        int count = -1;
                        bool countOK = false;
                        if (getCount.IsValid()) {
                            auto ex = BNM::TryInvoke([&]{ count = getCount[sprites](); });
                            countOK = !ex.IsValid();
                        }
                        LOG_INFO("CustomComponents/Pick3/HOLDER_INSPECT: option=%d countMethod=%d countOK=%d count=%d guidWithinBounds=%d",
                                 i+1, getCount.IsValid()?1:0, countOK?1:0, count,
                                 countOK && previewCardIds[i]>=0 && previewCardIds[i]<count ? 1 : 0);
                    }
                }
            }
            Obj* sprite = holder ? getFaceSprite[holder](previewCardIds[i]) : nullptr;
            if(artTraceModeForPopup>=2)
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: CALLBACK_RESULT option=%d guid=%d holder=%d sprite=%d",
                         i+1,previewCardIds[i],holder?1:0,sprite?1:0);
            asyncApplied[i] = true;
            if (!sprite) {
                CC_DIAG("CustomComponents/Pick3Preview/ASYNC: option=%d cardId=%d holder loaded, sprite=MISS",
                         i + 1, previewCardIds[i]);
                continue;
            }
            char name[] = "Pick3_LiveSprite_0";
            name[17] = static_cast<char>('1' + i);
            Obj* go = findByName(BNM::CreateMonoString(name));
            Obj* graphic = go ? getComponent[go](imageClass.GetMonoType()) : nullptr;
            if (!graphic) {
                if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: ASSIGN_SKIP option=%d missingImage",i+1);
                continue;
            }
            if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: ASSIGN_ENTER option=%d guid=%d",i+1,previewCardIds[i]);
            setImageSprite[graphic](sprite);
            setGraphicColor[graphic](Color{1.f,1.f,1.f,1.f});
            if (setImagePreserveAspect.IsValid()) setImagePreserveAspect[graphic](true);
            liveImageReady[i] = true;
            if(artTraceModeForPopup>=2) LOG_INFO("CustomComponents/Pick3/ART_TRACE: ASSIGN_SUCCESS option=%d guid=%d",i+1,previewCardIds[i]);
            RelabelForGameSprite(i, previewCardIds[i]);
            CC_DIAG("CustomComponents/Pick3Preview/ASYNC: option=%d cardId=%d sprite=LIVE",
                     i + 1, previewCardIds[i]);
        }
    }

    inline void ProbeLiveArt() {
        if (NativeArtOnly() || !isOpen || !Root() || !cardFaceLookupReady) return;
        int inspected = 0;
        int foundCount = 0;
        for (int i = 0; i < 3; ++i) {
            if (liveImageReady[i]) { ++foundCount; continue; }
            // Find by name rather than caching unsafe Unity pointers across
            // frames/matches. Each image is a scene-owned UGUI object.
            char name[] = "Pick3_LiveSprite_0";
            name[17] = static_cast<char>('1' + i);
            Obj* go = findByName(BNM::CreateMonoString(name));
            if (!go) continue;
            const int cardId = previewCardIds[i];
            Obj* sprite = FindResidentCardFaceSprite(cardId, &inspected);
            if(artTraceModeForPopup)
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: RESIDENT option=%d guid=%d holders=%d found=%d",
                         i+1,cardId,inspected,sprite?1:0);
            if (!sprite) {
                CC_DIAG("CustomComponents/Pick3Preview/DYNAMIC: option=%d cardId=%d sprite=MISS holders=%d",
                         i + 1, cardId, inspected);
                continue;
            }
            Obj* graphic = getComponent[go](imageClass.GetMonoType());
            if (!graphic) continue;
            setImageSprite[graphic](sprite);
            setGraphicColor[graphic](Color{1.f, 1.f, 1.f, 1.f});
            if (setImagePreserveAspect.IsValid()) setImagePreserveAspect[graphic](true);
            liveImageReady[i] = true;
            RelabelForGameSprite(i, cardId);
            ++foundCount;
            CC_DIAG("CustomComponents/Pick3Preview/DYNAMIC: option=%d cardId=%d sprite=LIVE holders=%d",
                     i + 1, cardId, inspected);
        }
        ++spriteProbeCount;
        CC_DIAG("CustomComponents/Pick3Preview/DYNAMIC: probe=%d/%d live=%d/3; screenshot portraits remain as fallback",
                 spriteProbeCount, kMaxSpriteProbes, foundCount);
    }

    inline bool IsInMatch() {
        if (!localHandClass.IsValid() || !findObjectOfType.IsValid()) return false;
        Obj* localHand = findObjectOfType(localHandClass.GetMonoType());
        return localHand != nullptr;
    }

    inline Obj* FindCafeteriaFont() {
        // This version of PvZH ships a TextMeshProFont SDF asset. UnityEngine.Font
        // (including Arial.ttf) is NOT the game's Cafeteria font.
        if (!tmpReady || !findLoadedUnityObjects.IsValid() ||
            !tmpFontClass.IsValid() || !getUnityObjectName.IsValid()) return nullptr;
        // Prefer a live game TMP text that uses the localized Cafeteria
        // asset. Never mutate or redistribute the game's font materials.
        auto* texts = findLoadedUnityObjects(tmpTextClass.GetMonoType());
        if (texts && texts->GetCapacity() < 8192) {
            for (std::size_t i = 0; i < texts->GetCapacity(); ++i) {
                Obj* text = texts->m_Items[i];
                if (!text || !BNM::IsA(text, tmpTextClass)) continue;
                Obj* font = tmpGetFont[text]();
                if (!font || !BNM::IsA(font, tmpFontClass)) continue;
                auto* name = getUnityObjectName[font]();
                if (name && name->str().find("Cafeteria Black") != std::string::npos) {
                    if (tmpGetSharedMaterial.IsValid()) {
                        Obj* material=nullptr;
                        const auto ex=BNM::TryInvoke([&]{material=tmpGetSharedMaterial[text]();});
                        if(!ex.IsValid()&&material)cafeteriaMaterial=material;
                    }
                    return font;
                }
            }
        }
        // As a fallback the font asset itself may be resident without a
        // TextMeshProUGUI donor (e.g. near the beginning of a scene).
        auto* fonts = findLoadedUnityObjects(tmpFontClass.GetMonoType());
        if (fonts && fonts->GetCapacity() < 4096) {
            for (std::size_t i = 0; i < fonts->GetCapacity(); ++i) {
                Obj* font = fonts->m_Items[i];
                if (!font || !BNM::IsA(font, tmpFontClass)) continue;
                auto* name = getUnityObjectName[font]();
                if (name && name->str().find("Cafeteria Black") != std::string::npos)
                    return font;
            }
        }
        return nullptr;
    }

    inline void SetLabelValue(Obj* gameObject, const char* value) {
        if (!gameObject) return;
        if (tmpReady) {
            Obj* tmp = getComponent[gameObject](tmpTextClass.GetMonoType());
            if (tmp) { tmpSetText[tmp](BNM::CreateMonoString(value)); return; }
        }
        Obj* oldText = getComponent[gameObject](textClass.GetMonoType());
        if (oldText) setText[oldText](BNM::CreateMonoString(value));
    }

    // IMPORTANT: UI.Image components borrow the *same* generated Sprite object.
    // The previous native-liveness probe invalidated new Sprite/Texture pairs
    // mere milliseconds after Sprite.Create(), then called Destroy() while the
    // popup still referenced them. This produced intermittent white rectangles
    // and tremendous in-popup cache churn. GC handles + DontUnloadUnusedAsset
    // provide the lifetime ownership; native probes are now diagnostics ONLY.
    inline bool OwnedArtAlive(Obj* obj) {
        if (!obj || !unityObjectAlive.IsValid()) return false;
        bool alive=false;
        const auto error=BNM::TryInvoke([&]{ alive=unityObjectAlive(obj); });
        return !error.IsValid() && alive;
    }

    inline Obj* ValidatedOwnedArt(int index) {
        if (!rewardSprites[index]) return nullptr;
        if (!rewardSpriteRoots[index]) {
            // UnityFS artwork, when enabled, is owned by the bundle loader.
            // Never Destroy/replace a borrowed Sprite on the popup path.
            return rewardSprites[index];
        }
        Obj* sprite=Pick3NativeAvatarLoad::Target(rewardSpriteRoots[index]);
        Obj* texture=Pick3NativeAvatarLoad::Target(rewardTextureRoots[index]);
        if (!sprite || !texture) {
            // A missing managed root is a genuine cache failure. Avoid
            // Destroy() here: existing Images may still reference the asset.
            // This branch should not occur with correctly rooted PNG assets.
            LOG_ERR("CustomComponents/Pick3/ART_CACHE: ROOT_MISSING kind=%d sprite=%d texture=%d",
                    index, sprite?1:0, texture?1:0);
            return nullptr;
        }
        if (!artCacheNativeCheckLogged[index]) {
            artCacheNativeCheckLogged[index]=true;
            // GC handles establish managed reachability. Unity native-object
            // probing on this path was unreliable and introduced extra IL2CPP
            // invocations whenever a picker opened.
            LOG_INFO("CustomComponents/Pick3/ART_CACHE: CHECK kind=%d rooted=1 nativeProbes=0 nonDestructive=1", index);
            // A false result here is NOT permission to destroy a Sprite used
            // by another Image. This diagnostic helps isolate the mismatched
            // BNM/Unity getter without risking the active picker.
        }
        rewardSprites[index]=sprite;
        rewardTextures[index]=texture;
        if (!artCacheReuseLogged[static_cast<std::size_t>(index)]) {
            artCacheReuseLogged[static_cast<std::size_t>(index)]=true;
            LOG_INFO("CustomComponents/Pick3/ART_CACHE: REUSE kind=%d rooted=1 pinned=1",index);
        }
        return sprite;
    }

    inline Obj* LoadRewardSpriteImpl(Pick3RewardArt::Kind kind, bool allowNative) {
        const int index = static_cast<int>(kind);
        if (index < 0 || index >= Pick3RewardArt::Count) return nullptr;
        if (Obj* cached=ValidatedOwnedArt(index)) return cached;
        // First choice: a real Unity Sprite from an explicitly packaged local
        // AssetBundle. No scene searches or PNG decoding for successful loads.
        int nativeIndex=-1;
        switch(kind){
          case Pick3RewardArt::InspectionGrass: nativeIndex=0;break;
          case Pick3RewardArt::InspectionReferenceRim: nativeIndex=1;break;
          case Pick3RewardArt::CostSun: nativeIndex=2;break;
          case Pick3RewardArt::CostBrain: nativeIndex=3;break;
          case Pick3RewardArt::AttackLeaf: nativeIndex=4;break;
          case Pick3RewardArt::HealthHeart: nativeIndex=5;break;
          default: break;
        }
        if(allowNative && nativeIndex>=0){
            Obj* native=Pick3LocalAssetBundles::Sprite(nativeIndex);
            if(native){
                // The asset manager owns the rooted sprite/bundle lifetime.
                rewardSprites[index]=native;
                return native;
            }
            // A bundled, owned PNG is safer than a missing icon if the
            // UnityFS registry is disabled. Use it for brain and attack too.
        }
        if(!pngArtReady)return nullptr;
        const auto data = Pick3RewardArt::Get(kind);
        if (!data.bytes || !data.size) return nullptr;
        Obj* texture = textureClass.CreateNewObjectParameters(2, 2);
        if (!texture) return nullptr;
        auto* raw = byteClass.NewArray<std::uint8_t>(data.size);
        if (!raw) return nullptr;
        std::memcpy(raw->m_Items, data.bytes, data.size);
        if (!loadPng(texture, raw)) {
            // Abandon the unshared texture; never cache a partially decoded asset.
            if (destroyObject.IsValid()) BNM::TryInvoke([&]{destroyObject(texture);});
            return nullptr;
        }
        Obj* sprite = spriteCreate(texture, Rect{0, 0, static_cast<float>(data.width),
                                                 static_cast<float>(data.height)}, Vec2{.5f, .5f});
        if (sprite) {
            // Do not call UnityEngine.Object.set_hideFlags from this path.
            // The crashing build faults in the native-to-managed invocation;
            // a managed GC root alone does not guarantee Unity native lifetime.
            // Cache is borrowed by picker UI, and no opportunistic destruction
            // is performed while an Image may still reference the sprite.
            rewardTextureRoots[index]=Pick3NativeAvatarLoad::Root(texture);
            rewardSpriteRoots[index]=Pick3NativeAvatarLoad::Root(sprite);
            if(rewardSpriteRoots[index] && rewardTextureRoots[index]){
                rewardTextures[index] = texture;
                rewardSprites[index] = sprite;
                LOG_INFO("CustomComponents/Pick3/ART_CACHE: CREATE kind=%d rooted=1 persistent=%d",
                         index,0);
            }else{
                // Do not keep pointers to GC-movable wrappers without roots.
                Pick3NativeAvatarLoad::Free(rewardTextureRoots[index]);
                Pick3NativeAvatarLoad::Free(rewardSpriteRoots[index]);
                BNM::TryInvoke([&]{destroyObject(sprite);});
                BNM::TryInvoke([&]{destroyObject(texture);});
                return nullptr;
            }
        }
        return sprite;
    }

    inline Obj* LoadRewardSprite(Pick3RewardArt::Kind kind) {
        // Prefer safely rooted stock sprites; do NOT memoize borrowed assets in
        // the process-lifetime fallback PNG cache. Unity can unload the owning
        // bundle on a later scene, so check liveness on every new use.
        switch (kind) {
            case Pick3RewardArt::CostSun:
                if (auto* p = Pick3GameOwnedAssetRegistry::Cost(false)) return p;
                break;
            case Pick3RewardArt::CostBrain:
                if (auto* p = Pick3GameOwnedAssetRegistry::Cost(true)) return p;
                break;
            case Pick3RewardArt::AttackLeaf:
                if (auto* p = Pick3GameOwnedAssetRegistry::Icon(0)) return p;
                break;
            case Pick3RewardArt::HealthHeart:
                if (auto* p = Pick3GameOwnedAssetRegistry::Icon(1)) return p;
                break;
            default: break;
        }
        return LoadRewardSpriteImpl(kind, true);
    }

    // A managed GC handle only keeps the wrapper reachable; it cannot keep
    // Unity's native Sprite/Texture alive across scene or match teardown.
    // Called only after the popup has been closed / the match is no longer
    // present. Never Destroy() cached native objects here: earlier native
    // probes and Destroy churn crashed on this Unity build.
    inline void ResetOwnedStaticArtForMatch(const char* reason) {
        int released=0;
        for (int i=0;i<Pick3RewardArt::Count;++i) {
            if (rewardSpriteRoots[i] || rewardTextureRoots[i]) ++released;
            Pick3NativeAvatarLoad::Free(rewardSpriteRoots[i]);
            Pick3NativeAvatarLoad::Free(rewardTextureRoots[i]);
            rewardSprites[i]=nullptr;
            rewardTextures[i]=nullptr;
            artCacheNativeCheckLogged[i]=false;
        }
        sharedStaticReady=false;
        sharedStaticReadyCount=0;
        artCacheReuseLogged.fill(false);
        LOG_INFO("CustomComponents/Pick3/ART_CACHE: MATCH_RESET reason=%s releasedPairs=%d preloadReady=0",reason,released);
    }

    inline void PreloadOwnedStaticArt() {
        if(sharedStaticReady || !pngArtReady)return;
        int ready=0;
        // A native crash does not produce a C++ exception. These markers let
        // the next log identify the exact kind being accessed if it recurs.
        for (auto kind : kSharedStaticKinds) {
            const int index=static_cast<int>(kind);
            LOG_INFO("CustomComponents/Pick3/PRELOAD_TRACE: ENTER kind=%d",index);
            Obj* sprite=LoadRewardSprite(kind);
            LOG_INFO("CustomComponents/Pick3/PRELOAD_TRACE: EXIT kind=%d present=%d",index,sprite?1:0);
            if(sprite)++ready;
        }
        sharedStaticReady=true;
        sharedStaticReadyCount=ready;
        (void)Pick3LocalAssetBundles::ColorScheme();
        LOG_INFO("CustomComponents/Pick3/STATIC_ASSETS: ready=%d/%d source=owned-unityfs+verified-fallback noResidentSpriteScan=1",
                 ready,static_cast<int>(std::size(kSharedStaticKinds)));
    }

    // Embedded fallback PNGs can be checked without invoking managed/Unity APIs.
    // Keep this in CPU-only menu phase: the PvE screen crash happened exactly
    // while calling AssetBundle.LoadAsset during menu-time prewarming.
    inline bool VerifyEmbeddedPng(Pick3RewardArt::Kind kind) {
        const auto art=Pick3RewardArt::Get(kind);
        if(!art.bytes || art.size<24 || art.size>4*1024*1024 ||
           art.width<=0 || art.height<=0 || art.width>8192 || art.height>8192)
            return false;
        static constexpr std::uint8_t signature[8]={137,80,78,71,13,10,26,10};
        if(std::memcmp(art.bytes,signature,sizeof(signature))!=0)return false;
        // PNG IHDR width and height are big endian, at offsets 16 and 20.
        auto be32=[](const std::uint8_t* ptr)->std::uint32_t{
            return (static_cast<std::uint32_t>(ptr[0])<<24) |
                   (static_cast<std::uint32_t>(ptr[1])<<16) |
                   (static_cast<std::uint32_t>(ptr[2])<<8) |
                   static_cast<std::uint32_t>(ptr[3]);
        };
        return be32(art.bytes+16)==static_cast<std::uint32_t>(art.width) &&
               be32(art.bytes+20)==static_cast<std::uint32_t>(art.height);
    }

    // Runs from the existing post-EventSystem.Update dispatcher.
    // MENU: CPU-only, no Unity managed method calls. MATCH: only verified
    // PNG Texture2D/Sprite construction, one item after 180 stable match frames.
    // NEVER perform AssetBundle.LoadFromFile or LoadAsset from this callback.
    inline void StartupPrewarmTick() {
        if(!startupPrewarmEnabled || startupPrewarmDone)return;
        ++startupPrewarmTick;
        if(startupPrewarmTick<static_cast<unsigned int>(startupPrewarmStartFrames))return;
        if(!startupPrewarmCpuDone){
            if(startupPrewarmTick-startupPrewarmLastStep<
                static_cast<unsigned int>(startupPrewarmIntervalFrames))return;
            startupPrewarmLastStep=startupPrewarmTick;
            if(startupPrewarmStage==0)
                LOG_INFO("CustomComponents/Pick3/PREWARM: CPU_BEGIN nativeEarly=0");
            if(startupPrewarmStage<static_cast<int>(std::size(kCpuVerifyKinds))){
                const auto kind=kCpuVerifyKinds[startupPrewarmStage++];
                const bool valid=VerifyEmbeddedPng(kind);
                startupPrewarmPngVerified[static_cast<int>(kind)]=valid;
                LOG_INFO("CustomComponents/Pick3/PREWARM: CPU_VERIFY kind=%d valid=%d",
                         static_cast<int>(kind),valid?1:0);
                return;
            }
            startupPrewarmCpuDone=true;
            LOG_INFO("CustomComponents/Pick3/PREWARM: CPU_COMPLETE nativeCalls=0 unityCalls=0");
            if(!startupPrewarmFallbackSprites){
                startupPrewarmDone=true;
                LOG_INFO("CustomComponents/Pick3/PREWARM: COMPLETE fallbackSprites=0 nativeCalls=0");
            }
            return;
        }
        if(sharedStaticReady){
            startupPrewarmDone=true;
            LOG_INFO("CustomComponents/Pick3/PREWARM: COMPLETE reason=picker-already-preloaded nativeEarly=0");
            return;
        }
        if(isOpen || requestActive || !pngArtReady){
            startupPrewarmMatchStable=0;
            return;
        }
        // The normal Tick() already discovers a match on its bounded
        // 30-frame idle path. Do not introduce additional Unity calls in
        // frontend menus just to discover a potential match.
        if(!matchWasPresent){
            startupPrewarmMatchStable=0;
            return;
        }
        // The expensive FindObjectOfType-based hand check is sampled every
        // ~12 frontend frames, NOT on every frame. A sustained sequence of
        // successful samples is required before Unity Sprite creation.
        constexpr unsigned int kMatchSampleFrames=12;
        if(startupPrewarmTick-startupPrewarmLastMatchCheck<kMatchSampleFrames)return;
        startupPrewarmLastMatchCheck=startupPrewarmTick;
        if(!IsInMatch()){
            startupPrewarmMatchStable=0;
            return;
        }
        if(startupPrewarmMatchStable<static_cast<unsigned int>(startupPrewarmMatchFrames)){
            startupPrewarmMatchStable=std::min(
                startupPrewarmMatchStable+kMatchSampleFrames,
                static_cast<unsigned int>(startupPrewarmMatchFrames));
            return;
        }
        if(startupPrewarmTick-startupPrewarmLastStep<
            static_cast<unsigned int>(startupPrewarmIntervalFrames))return;
        startupPrewarmLastStep=startupPrewarmTick;
        if(startupPrewarmFallbackStage==0)
            LOG_INFO("CustomComponents/Pick3/PREWARM: MATCH_BEGIN stableFrames=%u nativeEarly=0",
                     startupPrewarmMatchStable);
        const int stage=startupPrewarmFallbackStage++;
        if(stage<static_cast<int>(std::size(kMatchFallbackKinds))){
            const auto kind=kMatchFallbackKinds[stage];
            const std::uint64_t start=PerfClockMs();
            // This deliberately bypasses Pick3LocalAssetBundles::Sprite().
            Obj* resolved=startupPrewarmPngVerified[static_cast<int>(kind)]
                ? LoadRewardSpriteImpl(kind,false) : nullptr;
            LOG_INFO("CustomComponents/Pick3/PREWARM: MATCH_PNG stage=%d kind=%d present=%d elapsedMs=%llu nativeCalls=0",
                     stage,static_cast<int>(kind),resolved?1:0,
                     static_cast<unsigned long long>(PerfClockMs()-start));
        } else {
            startupPrewarmDone=true;
            LOG_INFO("CustomComponents/Pick3/PREWARM: COMPLETE fallbackSprites=%d nativeEarly=0",
                     static_cast<int>(std::size(kMatchFallbackKinds)));
        }
    }

    inline Obj* RewardImage(Obj* parent, const char* name, float x, float y,
                            float w, float h, Pick3RewardArt::Kind kind,
                            Color fallback, bool intercept = false) {
        Obj* go = Panel(parent, name, x, y, w, h, fallback, intercept);
        if (!go) return nullptr;
        Obj* sprite = LoadRewardSprite(kind);
        if (sprite) {
            const bool testGrass=(kind==Pick3RewardArt::InspectionGrass &&
                     Pick3LocalAssetBundles::directBundleLoadingEnabled &&
                     Pick3LocalAssetBundles::probeMode>=3);
            if(testGrass) LOG_INFO("CustomComponents/Pick3/UNITYFS: GRASS_APPLY_BEGIN object=%s native=%d",
                                   name, sprite==Pick3LocalAssetBundles::sprites[0].object?1:0);
            Obj* image = getComponent[go](imageClass.GetMonoType());
            if (image) {
                // The one existing Image per offer owns only the UI component;
                // the native Sprite and its Bundle remain GC-rooted in the loader.
                setImageSprite[image](sprite);
                setGraphicColor[image](Color{1,1,1,1});
                if(testGrass) LOG_INFO("CustomComponents/Pick3/UNITYFS: GRASS_APPLY_READY object=%s",
                                       name);
                if (setImagePreserveAspect.IsValid())
                    setImagePreserveAspect[image](kind!=Pick3RewardArt::DescriptionPlate &&
                                                     kind!=Pick3RewardArt::InlineTooltipPlate);
            }
        } else {
            // An Image without a Sprite draws its default solid UI rectangle.
            // Never let a missing circular rim or icon obscure a card portrait.
            if (kind == Pick3RewardArt::InspectionGrass ||
                kind == Pick3RewardArt::InspectionReferenceRim ||
                kind == Pick3RewardArt::CostSun ||
                kind == Pick3RewardArt::CostBrain ||
                kind == Pick3RewardArt::AttackLeaf ||
                kind == Pick3RewardArt::HealthHeart) {
                Obj* image=getComponent[go](imageClass.GetMonoType());
                if(image)setGraphicColor[image](Color{1.f,1.f,1.f,0.f});
            }
        }
        return go;
    }

    inline Obj* FindSceneFont() {
        Obj* text = findObjectOfType(textClass.GetMonoType());
        if (text && getFont.IsValid()) {
            Obj* font = getFont[text]();
            if (font) return font;
        }
        if (getBuiltinResource.IsValid())
            return getBuiltinResource(fontClass.GetMonoType(), BNM::CreateMonoString("Arial.ttf"));
        return nullptr;
    }

    // We have the genuine smallinspectionframe, but not the complete stock
    // CardInspectionColorColorizeImageView composition/color scheme. Until
    // that is verified, do NOT invent palette values or split/tint the rim.
    inline int CountClassBits(int bits) {
        const unsigned int v = static_cast<unsigned int>(bits);
        int count = 0;
        for(unsigned int mask=1; mask; mask<<=1) if(v & mask) ++count;
        return count;
    }
    inline void ApplyClassBackground(int index, int flags) {
        if(index<0 || index>=3) return;
        if(rimClassFlags[index] == flags) return;
        rimClassFlags[index] = flags;
        const int count=CountClassBits(flags);
        // PNG rim halves are created from OUR pinned art (not from UnityFS).
        // Do not instantiate prefabs or mutate any game-owned UI views.
        if (useStockClassRimTint && count>=1 && count<=2) {
            int first=0,second=0;
            for(int bit=0;bit<10;++bit){
                const int mask=1<<bit;
                if(flags & mask){if(!first)first=mask;else if(!second)second=mask;}
            }
            Color left=(count==1||count==2)?CardClassColor(first):Color{1,1,1,1};
            Color right=count==2?CardClassColor(second):left;
            const char* prefixes[]={"Pick3_ClassRimLeft_","Pick3_ClassRimRight_"};
            const Color choices[]={left,right};
            for(int side=0;side<2;++side){
                char name[48]{};std::snprintf(name,sizeof(name),"%s%d",prefixes[side],index+1);
                Obj* go=findByName(BNM::CreateMonoString(name));
                Obj* image=go?getComponent[go](imageClass.GetMonoType()):nullptr;
                if(image){
                    setGraphicColor[image](choices[side]);
                }
            }
            LOG_INFO("CustomComponents/Pick3/CLASS_SHELL: option=%d flags=%d classes=%d source=owned-split-rim left=%d right=%d",
                     index+1,flags,count,first,second);
        }else{
            LOG_INFO("CustomComponents/Pick3/CLASS_SHELL: option=%d flags=%d classes=%d source=native-inspection-rim tint=none",
                     index+1, flags, count);
        }
    }
    inline Obj* InspectionCostSprite(Obj* holderGO) {
        if(!holderGO)return nullptr;
        Obj* graphic=getComponent[holderGO](imageClass.GetMonoType());
        if(!graphic && getComponentInChildren.IsValid())
            graphic=getComponentInChildren[holderGO](imageClass.GetMonoType());
        if(graphic && imageGetSprite.IsValid()){
            Obj* sprite=nullptr;
            if(!BNM::TryInvoke([&]{sprite=imageGetSprite[graphic]();}).IsValid() && sprite)return sprite;
        }
        Obj* renderer=getComponent[holderGO](spriteRendererClass.GetMonoType());
        if(!renderer&&getComponentInChildren.IsValid())
            renderer=getComponentInChildren[holderGO](spriteRendererClass.GetMonoType());
        if(renderer&&spriteRendererGetSprite.IsValid()){
            Obj* sprite=nullptr;
            if(!BNM::TryInvoke([&]{sprite=spriteRendererGetSprite[renderer]();}).IsValid())return sprite;
        }
        return nullptr;
    }
    inline void CollectInspectionArt() {
        // Deliberately no-op: cost/stats are embedded and self-owned.
    }

    inline void ResizeDescriptionPlate(int option){
        if(!descriptionLayout.autoSize||option<0||option>=3||!findByName.IsValid())return;
        const float cardW=descriptionCardW[option],cardH=descriptionCardH[option];
        if(cardW<=0||cardH<=0)return;
        char name[64]{};std::snprintf(name,sizeof(name),"Pick3_Desc_%d",option+1);
        Obj* textGO=findByName(BNM::CreateMonoString(name));
        Obj* tmp=textGO&&tmpReady?getComponent[textGO](tmpTextClass.GetMonoType()):nullptr;
        if(!tmp||!tmpGetPreferredHeight.IsValid()||!tmpGetPreferredWidth.IsValid())return;
        const float maxW=cardW*descriptionLayout.maxWidth;
        const float minW=cardW*descriptionLayout.minWidth;
        const float maxH=cardH*descriptionLayout.maxHeight;
        const float minH=cardH*descriptionLayout.minHeight;
        const float pad=std::max(2.f,cardW*descriptionLayout.padding);
        float preferredWidth=maxW;
        BNM::TryInvoke([&]{preferredWidth=tmpGetPreferredWidth[tmp]();});
        if(!std::isfinite(preferredWidth)||preferredWidth<1.f)preferredWidth=maxW;
        float plateW=std::clamp(preferredWidth+2.f*pad,minW,maxW);
        // Constrain the label BEFORE evaluating TMP's wrapped preferredHeight.
        Position(textGO,cardW*layout.plateWidth*statText.descriptionX,
                 descriptionPlateY[option]+cardH*layout.plateHeight*statText.descriptionY,
                 std::max(12.f,plateW-2.f*pad),maxH);
        float preferredHeight=minH;
        BNM::TryInvoke([&]{preferredHeight=tmpGetPreferredHeight[tmp]();});
        if(!std::isfinite(preferredHeight)||preferredHeight<1.f)preferredHeight=minH;
        float plateH=std::clamp(preferredHeight+2.f*pad,minH,maxH);
        if(tmpSetAutoSize.IsValid())tmpSetAutoSize[tmp](descriptionLayout.autoFit && preferredHeight+2.f*pad>maxH);
        Position(textGO,cardW*layout.plateWidth*statText.descriptionX,
                 descriptionPlateY[option]+cardH*layout.plateHeight*statText.descriptionY,
                 std::max(12.f,plateW-2.f*pad),std::max(12.f,plateH-2.f*pad));
        std::snprintf(name,sizeof(name),"Pick3_DescriptionPlate_%d",option+1);
        Obj* plate=findByName(BNM::CreateMonoString(name));
        if(plate)Position(plate,0.f,descriptionPlateY[option],plateW,plateH);
        // Preserve the normal gap to the bottom edge, move the entire Choose
        // button (and hit box) if the description grows downwards.
        const float delta=(plateH-descriptionBasePlateHeight[option])*.5f;
        const float buttonY=descriptionButtonY[option]-delta;
        const float h=std::max(34.f,64.f*cardUnit)*layout.buttonHeight;
        const float bw=cardW*layout.buttonWidth;
        std::snprintf(name,sizeof(name),"Pick3_ChooseButton_%d",option);
        Obj* button=findByName(BNM::CreateMonoString(name));
        if(button)Position(button,0.f,buttonY,bw,h);
        const float textY=buttonY+std::max(34.f,64.f*cardUnit)*layout.buttonTextOffset;
        std::snprintf(name,sizeof(name),"Pick3_ChooseText_%d",option);
        Obj* label=findByName(BNM::CreateMonoString(name));
        if(label)Position(label,0.f,textY,bw*.95f,h*.95f);
        std::snprintf(name,sizeof(name),"Pick3_ChooseShadow_%d",option+1);
        Obj* shadow=findByName(BNM::CreateMonoString(name));
        if(shadow)Position(shadow,1.5f*cardUnit,textY-1.5f*cardUnit,bw*.95f,h*.95f);
        choiceRects[option].y-=delta;
        descriptionButtonY[option]=buttonY;
        descriptionBasePlateHeight[option]=plateH;
    }

    // Retained as a compatibility entry point for older callers.
    // Static Pick3 UI must never scan or borrow game-owned sprites.
    inline void CollectStockRewardSprites() {}

    inline void SetStockSprite(const char* prefix, int index, Obj* sprite, Color tint) {
        if (!sprite || !findByName.IsValid()) return;
        char name[48]{}; std::snprintf(name,sizeof(name),"%s%d",prefix,index+1);
        Obj* go=findByName(BNM::CreateMonoString(name));
        Obj* image=go ? getComponent[go](imageClass.GetMonoType()) : nullptr;
        if (!image) return;
        setImageSprite[image](sprite);
        setGraphicColor[image](tint);
        if (std::strcmp(prefix,"Pick3_CostBadge_")==0 && getUnityObjectName.IsValid()) {
            static Obj* previousSun=nullptr;
            static Obj* previousBrain=nullptr;
            Obj*& previous=(sprite==gameBrainIcon ? previousBrain : previousSun);
            if (previous!=sprite) {
                previous=sprite;
                auto* name=getUnityObjectName[sprite]();
                LOG_INFO("CustomComponents/Pick3/UI: REAL_COST_SPRITE_READY resource=%s",
                         name ? name->str().c_str() : "(unknown)");
            }
        }
    }

    inline std::string FormatAbilityText(const std::string& source) {
        if (source.find('[')==std::string::npos || !taggedMediatorClass.IsValid() ||
            !mediatorConverter.IsValid() || !convertTaggedText.IsValid() ||
            !findLoadedUnityObjects.IsValid()) return source;
        auto* mediators=findLoadedUnityObjects(taggedMediatorClass.GetMonoType());
        if (!mediators || mediators->GetCapacity()>2048) return source;
        for (std::size_t i=0;i<mediators->GetCapacity();++i) {
            Obj* mediator=mediators->m_Items[i];
            if (!mediator || !BNM::IsA(mediator,taggedMediatorClass)) continue;
            Obj* converter=mediatorConverter[mediator].Get();
            if (!converter || !BNM::IsA(converter,inlineConverterClass)) continue;
            BNM::Structures::Mono::String* converted=nullptr;
            const auto ex=BNM::TryInvoke([&]{ converted=convertTaggedText[converter](BNM::CreateMonoString(source)); });
            if (!ex.IsValid() && converted && converted->str().size()<4096) return converted->str();
        }
        return source; // Keep source accurately if the stock mapping has not loaded.
    }

    inline std::string ReadableInlineLinks(std::string source){
        // Preserve TMP <link=...> IDs and native keyword provider behavior.
        // Only replace bright keyword foreground colors on cream description
        // plates. Do not alter the original localized gameplay strings.
        std::size_t p=0;
        while((p=source.find("<color=",p))!=std::string::npos){
            const auto end=source.find('>',p+7);
            if(end==std::string::npos||end-p>48)break;
            // Only replace light cyan/blue link colors; leave red/green
            // damage, health and icon tags exactly as the converter wrote them.
            const std::string token=source.substr(p+7,end-p-7);
            bool cyan=(token=="cyan"||token=="aqua");
            if(token.size()>=7 && token[0]=='#'){
                unsigned r=0,g=0,b=0;
                if(std::sscanf(token.c_str(),"#%02x%02x%02x",&r,&g,&b)==3)
                    cyan=(r<=120 && g>=115 && b>=145 && b>r+55);
            }
            if(cyan){source.replace(p,end-p+1,"<color=#007F91>");p+=15;}
            else p=end+1;
        }
        return source;
    }

    inline void RefreshDeferredGameArt() {
        // DO NOT perform a Resources.FindObjectsOfTypeAll scan from Tick.
        // Rich-text conversion is performed when metadata is first applied.
        // Deliberately leave unresolved icons as bundled sun/brain sprites;
        // this path must not read borrowed Unity Object fields mid-animation.
    }

    // One-shot official ResourceLoader request for the card-inspection UI
    // prefab. OFF by default until device verification: there is no guarantee
    // that the downloaded inspection asset bundle exists on every installation.
    // No global FindObjectsOfTypeAll and no fabricated "native" result.
    inline void OnInspectionPrefabLoaded(Obj* prefab){
        if(inspectionPrefabCompleted)return;
        inspectionPrefabCompleted=true;
        inspectionPrefabCompletedFrame=frameCounter;
        if(prefab && inspectorGameObjectClass.IsValid() && BNM::IsA(prefab,inspectorGameObjectClass)) {
            inspectionPrefabRoot=Pick3NativeAvatarLoad::Root(prefab);
            inspectionLoadResult=inspectionPrefabRoot?1:0;
        } else inspectionLoadResult=0;
    }
    inline void RequestInspectionPrefab(){
        if(!enableInspectionPrefabRequest || inspectionPrefabRequested || !isOpen ||
           !prefabLoadAsync.IsValid() || !inspectionPrefabReceiverClass.IsValid() ||
           !inspectorActionClass.IsValid())return;
        Obj* manager=nullptr;
        Obj* wrapper=nullptr;
        Obj* bridge=nullptr;
        Obj* loader=nullptr;
        if(getSoundManager.IsValid())BNM::TryInvoke([&]{manager=getSoundManager();});
        if(manager && getGameResourceLoader.IsValid())
            BNM::TryInvoke([&]{wrapper=getGameResourceLoader[manager]();});
        if(wrapper && BNM::IsA(wrapper,socialWrapperClass) && socialWrapperLoader.IsValid())
            bridge=socialWrapperLoader[wrapper].Get();
        if(bridge && BNM::IsA(bridge,socialBridgeClass) && getPvzCardAssetLoader.IsValid())
            BNM::TryInvoke([&]{loader=getPvzCardAssetLoader[bridge]();});
        if(!loader || !BNM::IsA(loader,cardAssetLoaderClass))return;
        auto* receiver=reinterpret_cast<Pick3InspectionPrefabReceiver::Receiver*>(
                           inspectionPrefabReceiverClass.CreateNewInstance());
        if(!receiver)return;
        inspectionPrefabReceiverRoot=Pick3NativeAvatarLoad::Root(reinterpret_cast<Obj*>(receiver));
        auto method=inspectionPrefabReceiverClass.GetMethod("OnLoaded",1);
        method.SetInstance(reinterpret_cast<Obj*>(receiver));
        auto* shell=reinterpret_cast<BNM::DelegateBase*>(inspectorActionClass.CreateNewInstance());
        Obj* callback=shell?reinterpret_cast<Obj*>(shell->Create(method)):nullptr;
        inspectionPrefabActionRoot=Pick3NativeAvatarLoad::Root(callback);
        if(!inspectionPrefabReceiverRoot || !inspectionPrefabActionRoot){
            Pick3NativeAvatarLoad::Free(inspectionPrefabReceiverRoot);
            Pick3NativeAvatarLoad::Free(inspectionPrefabActionRoot);
            return;
        }
        inspectionPrefabRequested=true;
        Pick3InspectionPrefabReceiver::deliver=&OnInspectionPrefabLoaded;
        // The manifest identifies this exact prefab resource path. If the
        // downloaded bundle is absent, the callback can return null.
        LOG_INFO("CustomComponents/Pick3/INSPECTION_PREFAB: request path=Assets/Ship/Prefabs/UI/CardInspectionPanel mode=one-shot");
        auto error=BNM::TryInvoke([&]{
            prefabLoadAsync[loader](BNM::CreateMonoString("Assets/Ship/Prefabs/UI/CardInspectionPanel"),callback);
        });
        if(error.IsValid()) {
            // Keep delegate handles unless we have evidence no callback can
            // arrive. Leaking one pair is preferable to use-after-free.
            LOG_ERR("CustomComponents/Pick3/INSPECTION_PREFAB: loader threw; no automatic retry");
        }
    }
    inline void ServiceInspectionPrefab(){
        if(!inspectionPrefabRequested || !inspectionPrefabCompleted)return;
        if(!inspectionPrefabHandled){
            inspectionPrefabHandled=true;
            if(!isOpen){
                LOG_INFO("CustomComponents/Pick3/INSPECTION_PREFAB: callback after popup close; no UI mutation");
            }

            int count=0;
            Obj* prefab=isOpen?Pick3NativeAvatarLoad::Target(inspectionPrefabRoot):nullptr;
            if(prefab && gameGetComponentsInChildren.IsValid() && inspectionCircleClass.IsValid()) {
                BNM::Structures::Mono::Array<Obj*>* views=nullptr;
                auto ex=BNM::TryInvoke([&]{
                    views=reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(
                        gameGetComponentsInChildren[prefab](inspectionCircleClass.GetMonoType(),true));
                });
                if(!ex.IsValid() && views && views->GetCapacity()<128) {
                    const char* fieldNames[]={"KabloomSprite","MegaGroSprite","GuardianSprite",
                        "SmartySprite","SolarSprite","BrainySprite","HeartySprite","SneakySprite",
                        "HungrySprite","MadcapSprite"};
                    for(std::size_t v=0;v<views->GetCapacity();++v){
                        Obj* view=views->m_Items[v];
                        if(!view || !BNM::IsA(view,inspectionCircleClass))continue;
                        for(int bit=0;bit<10;++bit){
                            if(nativeClassSprites[bit])continue;
                            auto f=inspectionCircleClass.GetField(fieldNames[bit]).cast<Obj*>();
                            if(f.IsValid()){
                                Obj* sprite=f[view].Get();
                                if(sprite && BNM::IsA(sprite,spriteClass)){
                                    nativeClassSprites[bit]=sprite;
                                    ++count;
                                }
                            }
                        }
                    }
                }
            }
            LOG_INFO("CustomComponents/Pick3/INSPECTION_PREFAB: callback result=%d classSprites=%d/10; fallback retained when unavailable",
                     inspectionLoadResult,count);
            // Only a verified, square class sprite can go straight on our
            // square circle. Tall card-inspection assets must be cropped
            // separately after the source sprite rect has been validated.
            for(int i=0;isOpen && i<activeChoiceCount;++i){
                if(!metadataLoaded[i])continue;
                const int flags=rimClassFlags[i];
                if(CountClassBits(flags)!=1)continue;
                int bit=-1;for(int b=0;b<10;++b)if(flags==(1<<b)){bit=b;break;}
                if(bit<0 || !nativeClassSprites[bit])continue;
                Obj* asset=nativeClassSprites[bit];
                Rect r{};
                auto err=BNM::TryInvoke([&]{r=nativeSpriteTextureRect[asset]();});
                if(err.IsValid() || r.width<=0.f || r.height<=0.f ||
                   !std::isfinite(r.width) || !std::isfinite(r.height))continue;
                // A tall panel image cannot simply be stretched over the
                // circle: it would reproduce the cropped-out inspection pane.
                const float ratio=r.width/r.height;
                if(ratio<.85f||ratio>1.18f){
                    LOG_INFO("CustomComponents/Pick3/INSPECTION_PREFAB: option=%d native sprite is panel-shaped (%.2f); keep fallback",i+1,ratio);
                    continue;
                }
                char name[48]{};
                std::snprintf(name,sizeof(name),"Pick3_NativeInspectionRim_%d",i+1);
                SetStockSprite("Pick3_NativeInspectionRim_",i,asset);
                LOG_INFO("CustomComponents/Pick3/INSPECTION_PREFAB: option=%d native square class sprite applied",i+1);
            }
        }
        if(frameCounter-inspectionPrefabCompletedFrame>=2){
            Pick3NativeAvatarLoad::Free(inspectionPrefabReceiverRoot);
            Pick3NativeAvatarLoad::Free(inspectionPrefabActionRoot);
        }
    }

    // Two-stage dismissal: first the unchosen card groups AND the chosen
    // button/labels slide away; only then does the chosen card fade out.
    // Keep the full sequence within the existing press-feedback window so
    // gameplay submission timing remains unchanged.
    inline void BeginChoiceTransition() {
        choiceTransitionActive=true;
        choiceTransitionStarted=PresentationClock();
        LOG_INFO("CustomComponents/Pick3/UI: CHOICE_EXIT_PREVIEW selected=%d total=%d",
                 picked.Count(),activeChoiceCount);
    }
    inline void AnimateChoiceTransition() {
        if(!choiceTransitionActive || !findByName.IsValid()) return;
        const float elapsed=PresentationClock()-choiceTransitionStarted;
        const float w=static_cast<float>(screenWidth());
        const float h=static_cast<float>(screenHeight());
        const float phase1=otherChoiceDismissSeconds;
        // The selected card finishes travelling BEFORE its fade begins.
        const float fadeStart=chosenGlideSeconds+chosenCenterHoldSeconds;
        const float phase2=chosenFadeSeconds;
        for(int i=0;i<activeChoiceCount;++i) {
            char name[48]{};
            std::snprintf(name,sizeof(name),"Pick3_RewardCard_%d",i+1);
            Obj* go=findByName(BNM::CreateMonoString(name));
            if(!go)continue;
            const bool chosen=picked.Has(i);
            const float t=std::clamp((elapsed-dismissTimingOffset[i])/phase1,0.f,1.f);
            const float eased=t*t*(3.f-2.f*t);
            Obj* rect=GetRect(go);
            Obj* group=canvasGroupClass.IsValid()?getComponent[go](canvasGroupClass.GetMonoType()):nullptr;
            if(!chosen) {
                if(rect && setAnchoredPosition.IsValid()) {
                    const float sign=i==2?1.f:-1.f;
                    setAnchoredPosition[rect](Vec2{cardOriginX[i]+sign*w*.16f*eased,
                                                    cardOriginY[i]+12.f*cardUnit*eased});
                }
                if(group && setCanvasGroupAlpha.IsValid())setCanvasGroupAlpha[group](1.f-eased);
                if(setLocalScale.IsValid()) {
                    Obj* tr=getTransform[go]();
                    if(tr)setLocalScale[tr](Vec3{1.f-.06f*eased,1.f-.06f*eased,1.f});
                }
                continue;
            }
            // A real multi-frame exponential glide, independent of the other
            // cards' dismissal. Lower options describe a shallow lifting arc;
            // the top option glides down. Normalization arrives at exactly 1.
            const float arrival=std::clamp(elapsed/chosenGlideSeconds,0.f,1.f);
            const float exponentialDenominator=1.f-std::exp2(-chosenGlideExponent);
            const float glide=arrival>=1.f?1.f:
                (1.f-std::exp2(-chosenGlideExponent*arrival))/exponentialDenominator;
            const float arc=cardOriginY[i]<0.f?
                std::sin(3.14159265f*arrival)*h*chosenArcHeight:0.f;
            if(rect && setAnchoredPosition.IsValid())
                setAnchoredPosition[rect](Vec2{cardOriginX[i]*(1.f-glide),
                                              cardOriginY[i]*(1.f-glide)+arc});
            const float buttonDrop=h*.19f*eased;
            const float labelY=descriptionButtonY[i]+
                std::max(34.f,64.f*cardUnit)*layout.buttonTextOffset;
            std::snprintf(name,sizeof(name),"Pick3_ChooseButton_%d",i);
            Obj* button=findByName(BNM::CreateMonoString(name));
            if(button) {
                Obj* buttonRect=GetRect(button);
                if(buttonRect && setAnchoredPosition.IsValid())
                    setAnchoredPosition[buttonRect](Vec2{0.f,descriptionButtonY[i]-buttonDrop});
                Obj* image=getComponent[button](imageClass.GetMonoType());
                if(image && setGraphicColor.IsValid())
                    setGraphicColor[image](Color{1.f,1.f,1.f,1.f-eased});
            }
            std::snprintf(name,sizeof(name),"Pick3_ChooseText_%d",i);
            Obj* label=findByName(BNM::CreateMonoString(name));
            if(label) {
                Obj* labelRect=GetRect(label);
                if(labelRect && setAnchoredPosition.IsValid())
                    setAnchoredPosition[labelRect](Vec2{0.f,labelY-buttonDrop});
                Obj* graphic=getComponent[label](textClass.GetMonoType());
                if(graphic && setGraphicColor.IsValid())
                    setGraphicColor[graphic](Color{1.f,1.f,1.f,1.f-eased});
            }
            std::snprintf(name,sizeof(name),"Pick3_ChooseShadow_%d",i+1);
            Obj* shadow=findByName(BNM::CreateMonoString(name));
            if(shadow) {
                Obj* shadowRect=GetRect(shadow);
                if(shadowRect && setAnchoredPosition.IsValid())
                    setAnchoredPosition[shadowRect](Vec2{1.5f*cardUnit,labelY-1.5f*cardUnit-buttonDrop});
                Obj* graphic=getComponent[shadow](textClass.GetMonoType());
                if(graphic && setGraphicColor.IsValid())
                    setGraphicColor[graphic](Color{.02f,.12f,.03f,1.f-eased});
            }
            // Only after the two nonselected cards and selected button
            // have departed may the selected card itself begin fading.
            const float fadeT=std::clamp((elapsed-fadeStart)/phase2,0.f,1.f);
            const float fade=fadeT*fadeT*(3.f-2.f*fadeT);
            // Restore the original soft "puff": chosen card grows slightly
            // while fading, after its own button and other cards slide off.
            if(setLocalScale.IsValid()) {
                Obj* tr=getTransform[go]();
                if(tr) {
                    const float puff=1.f+chosenPuffScale*(1.f-std::pow(1.f-fade,3.f));
                    setLocalScale[tr](Vec3{puff,puff,1.f});
                }
            }
            if(group && setCanvasGroupAlpha.IsValid())setCanvasGroupAlpha[group](1.f-fade);
        }
    }
    inline void BeginExitAnimation() {
        submitPending=false;
        choiceTransitionActive=false;
        exitAnimating=true;
        exitStarted=PresentationClock();
        Pick3::Submission::SetPopupInputCapture(true); // block stray stock touches during dismiss
    }
    inline bool AnimateExit() {
        if (!exitAnimating) return false;
        const float t=std::clamp((PresentationClock()-exitStarted)/std::max(.01f,exitSeconds),0.f,1.f);
        const float eased=t*t*(3.f-2.f*t);
        Obj* go=findByName.IsValid()?findByName(BNM::CreateMonoString("Pick3_Content")):nullptr;
        if (go && entranceApiReady) {
            Obj* group=getComponent[go](canvasGroupClass.GetMonoType());
            if (group) setCanvasGroupAlpha[group](1.f-eased);
            Obj* trans=getTransform[go]();
            if (trans && setLocalScale.IsValid()) setLocalScale[trans](Vec3{1.f-.07f*eased,1.f-.07f*eased,1.f});
        }
        Obj* veil=findByName.IsValid()?findByName(BNM::CreateMonoString("Pick3_DimmedBattlefield")):nullptr;
        if(veil) {
            Obj* image=getComponent[veil](imageClass.GetMonoType());
            if(image) setGraphicColor[image](Color{.025f,.035f,.035f,battlefieldDimAlpha*(1.f-eased)});
        }
        if(t>=1.f) Close();
        return true;
    }

    inline void UpdatePickStatus() {
        if (!gameplaySubmissionEnabled || !isOpen || !findByName.IsValid()) return;
        const bool ready = Pick3::Submission::Ready();
        char progress[100]{};
        if (submissionServiceError)
            std::snprintf(progress, sizeof(progress), "Submission failed - check Sprig log");
        else if (picked.Sealed() && submitPending)
            progress[0] = '\0'; // Button provides immediate visual acknowledgement.
        else if (picked.Sealed())
            std::snprintf(progress, sizeof(progress), "Confirming your choice...");
        else if (!ready || picked.Count() == 0)
            progress[0] = '\0';  // Never expose the asynchronous initialization stage.
        else
            std::snprintf(progress, sizeof(progress), "Selected %d of %d", picked.Count(), picked.Required());
        Obj* statusGo = findByName(BNM::CreateMonoString("Pick3_Status"));
        SetLabelValue(statusGo, progress);
        for (int i = 0; i < activeChoiceCount; ++i) {
            char textName[40]{}, panelName[40]{};
            std::snprintf(textName, sizeof(textName), "Pick3_ChooseText_%d", i);
            std::snprintf(panelName, sizeof(panelName), "Pick3_ChooseButton_%d", i);
            Obj* textGo = findByName(BNM::CreateMonoString(textName));
            SetLabelValue(textGo, picked.Has(i) ? "Selected" : "Choose");
            char buttonShadow[40]{};
            std::snprintf(buttonShadow, sizeof(buttonShadow), "Pick3_ChooseShadow_%d", i+1);
            SetLabelValue(findByName(BNM::CreateMonoString(buttonShadow)),
                          picked.Has(i) ? "Selected" : "Choose");
            Obj* panelGo = findByName(BNM::CreateMonoString(panelName));
            if (panelGo && setLocalScale.IsValid()) {
                Obj* tr=getTransform[panelGo]();
                if (tr) setLocalScale[tr](picked.Has(i)?Vec3{.95f,.95f,1.f}:Vec3{1.f,1.f,1.f});
            }
            Obj* panel = panelGo ? getComponent[panelGo](imageClass.GetMonoType()) : nullptr;
            if (panel) {
                const bool stockButton=NativeFullUIEnabled() &&
                    Pick3LocalAssetBundles::sprites[6].object;
                // Keep the stock sprite's intended appearance before input.
                // Selection feedback comes from the shared card exit motion.
                (void)stockButton;
                setGraphicColor[panel](Color{1.f,1.f,1.f,1.f});
            }
        }
    }

    inline void UpdateTargetingLocalization() {
        if (!isOpen || localizationResolved || !requestActive || activeRequest.sourceCardGuid <= 0)
            return;
        Obj* mapper = Pick3CardMetadata::FindLocalizationMapper();
        if (!mapper) return;
        std::string localized, resolvedKey;
        if (!Pick3CardMetadata::TryPickFromThreeTargeting(
                activeRequest.sourceCardGuid, mapper, localized, &resolvedKey)) return;
        Obj* go = findByName(BNM::CreateMonoString("Pick3_Subtitle"));
        if (!go) return;
        SetLabelValue(go, localized.c_str());
        if(Obj* shadow=findByName(BNM::CreateMonoString("Pick3_SubtitleShadow")))
            SetLabelValue(shadow, localized.c_str());
        localizationResolved = true;
        LOG_INFO("CustomComponents/Pick3/UI: TARGETING_LOC sourceGuid=%d key='%s'",
                activeRequest.sourceCardGuid, resolvedKey.c_str());
    }

    inline bool Open() {
        // Read once per popup; no mode switches while Unity objects are live.
        const int diagnosticMode=Pick3DiagnosticMode::ReadMode();
        Pick3MetadataIsolation::ReadMode();
        const bool rawAsyncSelector = ReadArtSelector(kAsyncArtSelector, configuredAsyncGameArt);
        const bool overrideAsyncSuppression = ReadArtSelector(kAsyncArtOverrideSelector, false);
        const bool blockedByAssetTrace = traceCardAssetsTask;
        const bool blockedByOwnerProbe = probeLiveOwners;
        useAsyncGameArt = rawAsyncSelector &&
            (overrideAsyncSuppression || (!blockedByAssetTrace && !blockedByOwnerProbe));
        residentArtEnabledForPopup = ReadArtSelector(kResidentArtSelector, true);
        lastInspectedArtWrapper = nullptr;
        LOG_INFO("CustomComponents/Pick3/ART_GATE: requested=%d config=%d override=%d "
                 "blockedAssetTrace=%d blockedOwnerProbe=%d effective=%d asyncReady=%d externalOnly=%d",
                 rawAsyncSelector ? 1 : 0, configuredAsyncGameArt ? 1 : 0,
                 overrideAsyncSuppression ? 1 : 0, blockedByAssetTrace ? 1 : 0,
                 blockedByOwnerProbe ? 1 : 0, useAsyncGameArt ? 1 : 0,
                 asyncMethodsReady ? 1 : 0, externalOnly ? 1 : 0);
        artTraceModeForPopup = ReadArtTraceMode();
        // Deliberately hard-disable the crash-associated holder inspection
        // regardless of any stale ADB selector on the device.
        holderInspectForPopup = false;
        factoryProbeModeForPopup = 0;
        if (FILE* f = std::fopen(kFactoryProbeSelector, "rb")) {
            int requested = 0;
            if (std::fscanf(f, "%d", &requested) == 1)
                factoryProbeModeForPopup = std::clamp(requested, 0, 7);
            std::fclose(f);
        }
        if (factoryProbeModeForPopup) {
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: mode=%d holderInspectorHardDisabled=1 createsCard=%d", factoryProbeModeForPopup, factoryProbeModeForPopup >= 5);
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: API factory=%d config=%d create4=%d avatar=%d enter=%d idle=%d getAvatar=%d",
                nativeCardViewFactoryClass.IsValid(), nativeCardViewConfigClass.IsValid(),
                nativeFactoryCreateAsync.IsValid(), nativeAvatarViewClass.IsValid(),
                nativeAvatarEnter.IsValid(), nativeAvatarIdle.IsValid(), nativeCardGetAvatar.IsValid());
            nativeAvatarProbeAttempts = 0;
            nativeAvatarProbeResolved = false;
            nativeAvatarProbeReported = false;
        }
        if(artTraceModeForPopup)
            LOG_INFO("CustomComponents/Pick3/ART_TRACE: mode=%d async=%d resident=%d lookupReady=%d asyncReady=%d nativeCache=%d",
                     artTraceModeForPopup, useAsyncGameArt?1:0,
                     residentArtEnabledForPopup?1:0, cardFaceLookupReady?1:0,
                     asyncMethodsReady?1:0, nativeCacheTest?1:0);
        LOG_INFO("CustomComponents/Pick3/ART_SWITCHES: async=%d resident=%d configAsync=%d",
                 useAsyncGameArt ? 1 : 0, residentArtEnabledForPopup ? 1 : 0,
                 configuredAsyncGameArt ? 1 : 0);
        // The successfully exercised three-card native pipeline is the normal
        // release path when enabled in config. A factory diagnostic selector
        // overrides it, so modes 0-4 cannot accidentally allocate CardViews.
        avatarExperimentStage=(factoryProbeModeForPopup==0 && nativeAvatarLoadTestEnabled) ?
            configuredAvatarExperimentStage : diagnosticMode;
        Pick3NativeAvatarLoad::releaseCreationAllowed=
            factoryProbeModeForPopup==0 && nativeAvatarLoadTestEnabled &&
            avatarExperimentStage>=2;
        // Factory selector overrides only the experimental native-avatar stage.
        // Preserve all normal PickFromThree gameplay and visual behavior.
        if (factoryProbeModeForPopup >= 5) {
            avatarExperimentStage = factoryProbeModeForPopup == 5 ? 2 :
                                    factoryProbeModeForPopup == 6 ? 3 : 4;
            avatarExperimentChoice = 0;
            // A completed prior request can advance on the next Unity tick.
            // An unresolved request is never overlapped with another start.
            avatarExperimentSpacingFrames = std::max(1,avatarExperimentSpacingFrames);

            nativeAvatarLoadTestEnabled = true;
            nativeAvatarRenderTestEnabled = factoryProbeModeForPopup >= 6;
            if (nativeAvatarRenderTestEnabled)
                Pick3NativeAvatarRender::Initialize(true);
            Pick3NativeAvatarLoad::Initialize(true);
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: RISK_OPT_IN mode=%d stage=%d loadReady=%d renderReady=%d", 
                     factoryProbeModeForPopup, avatarExperimentStage,
                     Pick3NativeAvatarLoad::initialized, Pick3NativeAvatarRender::ready);
        }
        if (factoryProbeModeForPopup == 3) {
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: MODE3_SIGNATURE factory=%d create4=%d cardView=%d config=%d action=%d receiver=%d", 
                     nativeCardViewFactoryClass.IsValid(), nativeFactoryCreateAsync.IsValid(),
                     Pick3NativeAvatarLoad::cardViewClass.IsValid(), nativeCardViewConfigClass.IsValid(),
                     Pick3NativeAvatarLoad::actionClass.IsValid(), Pick3NativeAvatarLoad::receiverClass.IsValid());
        }
        if (factoryProbeModeForPopup == 4) {
            LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: MODE4_LOADER enabled=%d initialized=%d create=%d callback=%d renderer=%d", 
                     Pick3NativeAvatarLoad::enabled, Pick3NativeAvatarLoad::initialized,
                     Pick3NativeAvatarLoad::createCard.IsValid(),
                     Pick3NativeAvatarLoad::actionClass.IsValid(), Pick3NativeAvatarRender::ready);
        }
        LOG_INFO("CustomComponents/Pick3/DIAG: POPUP_START mode=%d",diagnosticMode);
        const std::uint64_t perfStart=PerfClockMs();
        inlineTooltipShowing=false;
        inlineTooltipClosing=false;
        inlineTooltipTappedOption=-1;
        submitPending=false;
        exitAnimating=false;
        choiceTransitionActive=false;
        gameSunIcon=gameBrainIcon=gameAttackIcon=gameHealthIcon=gameFighterBackground=gameColorScheme=nullptr;
        stockArtRefreshCount=0;
        std::fill(std::begin(rimClassFlags),std::end(rimClassFlags),-999);
        cafeteriaMaterial=nullptr;
        std::fill(std::begin(nativeClassSprites),std::end(nativeClassSprites),nullptr);
        std::fill(std::begin(inspectedCircleCache),std::end(inspectedCircleCache),nullptr);
        std::fill(std::begin(richDescriptionReady),std::end(richDescriptionReady),false);
        for(auto& raw:unformattedDescriptions) raw.clear();
        submissionServiceError = false;
        avatarExperimentPreflightLogged=false;
        avatarExperimentLastStartFrame=0;
        avatarExperimentNextIndex=0;
        avatarExperimentTimeoutLogged=false;
        nativeAvatarProbeResolved=false;
        nativeAvatarProbeReported=false;
        nativeAvatarProbeAttempts=0;
        std::fill(std::begin(nativeAvatarLoadStartAttempts),std::end(nativeAvatarLoadStartAttempts),0);
        std::fill(std::begin(nativeAvatarLoadStarted),std::end(nativeAvatarLoadStarted),false);
        std::fill(std::begin(nativeAvatarRenderBound),std::end(nativeAvatarRenderBound),false);
        // If the game reused the same Unity scene and an old popup survived,
        // never silently report a successful Open() with isOpen still false.
        if (Root() || popupRootHandle) {
            LOG_ERR("CustomComponents/Pick3Preview: stale popup detected before Open; removing it");
            DestroyPopupRoot("open-stale");
        }
        const int sw = screenWidth();
        const int sh = screenHeight();
        if (sw <= 0 || sh <= 0 || !IsInMatch()) return false;
        const float w = static_cast<float>(sw), h = static_cast<float>(sh);
        const float unit = std::min(w / static_cast<float>(referenceWidth),
                                    h / static_cast<float>(referenceHeight));
        const float designW=static_cast<float>(referenceWidth)*unit;
        const float designH=static_cast<float>(referenceHeight)*unit;
        // Reward cards use a round image, white name and cream information
        // plate, unlike the older tall rectangular debugging panels.
        const float cardW = std::min(designW * .34f, 245.f * unit);
        const float cardH = std::min(designH * .27f, 346.f * unit);
        const float xGap = designW*layout.sideGap;
        const float topY = layout.topY*designH;
        const float lowerY = layout.lowerY*designH;
        const float buttonH = std::max(34.f, 64.f * unit);
        const int cardText = std::max(12, static_cast<int>(21.f * unit));
        const int buttonText = std::max(16, static_cast<int>(32.f * unit*layout.buttonTextScale));

        // Process-lifetime reward sprites and textures are already GC-rooted.
        // Clearing these pointers on each Open() caused PNG decode and
        // Sprite.Create to run again and leaked the previous root handles.
        // Keep cached art across matches; never cache scene-owned Images.
        const std::uint64_t perfBeforeFont=PerfClockMs();
        cafeteriaFont = FindCafeteriaFont();
        if (!cafeteriaFont && !cafeteriaNotFoundLogged) {
            cafeteriaNotFoundLogged = true;
            LOG_ERR("CustomComponents/Pick3/UI: Cafeteria Black SDF font not resident; temporary legacy fallback");
        }
        sceneFont = cafeteriaFont ? nullptr : FindSceneFont();
        CC_DIAG("CustomComponents/Pick3/UI: reward font=Cafeteria:%d size=%dx%d", cafeteriaFont ? 1 : 0, sw, sh);
        const std::uint64_t perfAfterFont=PerfClockMs();
        Pick3DiagnosticMode::Mark("static-art-preload",0,activeRequest.sourceCardGuid);
        PreloadOwnedStaticArt();
        Pick3DiagnosticMode::Complete("static-art-preload",0,activeRequest.sourceCardGuid);
        const std::uint64_t perfAfterPreload=PerfClockMs();
        LOG_INFO("CustomComponents/Pick3/PERF: PRELOAD elapsedMs=%llu fontMs=%llu nativeProbe=%d",
                 static_cast<unsigned long long>(perfAfterPreload-perfAfterFont),
                 static_cast<unsigned long long>(perfAfterFont-perfBeforeFont),
                 Pick3LocalAssetBundles::probeMode);
        Obj* root = CreateGO(kRootName, nullptr);
        if (!root) { LOG_ERR("CustomComponents/Pick3Preview: failed to allocate root"); return false; }
        if (Core::SceneViewContext::newGCHandle)
            popupRootHandle = Core::SceneViewContext::newGCHandle(root, false);
        CC_DIAG("CustomComponents/Pick3Preview: canvas created root=%p gcRoot=%u",
                 root, popupRootHandle);
        Obj* canvas = addComponent[root](canvasClass.GetMonoType());
        if (!canvas) { Close(); return false; }
        setCanvasRenderMode[canvas](0); // ScreenSpaceOverlay
        if (setCanvasOverrideSorting.IsValid()) setCanvasOverrideSorting[canvas](true);
        setCanvasSortingOrder[canvas](32000); // modal overlay above stock targeting UGUI
        if (!addComponent[root](raycasterClass.GetMonoType())) { Close(); return false; }

        // Fullscreen UGUI shield consumes EventSystem pointer raycasts.
        // World-space input routes not using EventSystem may require separate
        // verified command guards; do NOT globally hook tiny TouchManager.Update().
        // The three "Choose" regions below use pointer polling; stock target
        // command input is guarded while this Canvas owns gameplay selection.
        Obj* veil = Panel(root, "Pick3_DimmedBattlefield", 0, 0, w, h,
                          Color{.025f, .035f, .035f, battlefieldDimAlpha});
        if (!veil) { Close(); return false; }
        Obj* content = CreateGO("Pick3_Content", root);
        if (!content) { Close(); return false; }
        Position(content, 0, 0, w, h);
        if (entranceApiReady) {
            Obj* group = addComponent[content](canvasGroupClass.GetMonoType());
            if (!group) { Close(); return false; }
            setCanvasGroupAlpha[group](0.f);
            Obj* contentTransform = getTransform[content]();
            if (contentTransform && setLocalScale.IsValid())
                setLocalScale[contentTransform](Vec3{.90f,.90f,1.f});
        }
        char subtitle[80]{};
        if (gameplaySubmissionEnabled) {
            std::snprintf(subtitle, sizeof(subtitle), "Choose %d card%s to get.",
                          activeRequest.requiredTargets, activeRequest.requiredTargets == 1 ? "" : "s");
        } else {
            std::snprintf(subtitle, sizeof(subtitle), "%s",
                          externalOnly ? "Preview selection" : "Card preview");
        }
        auto createTargetingPrompt=[&](){
            const float tx=targetingX*unit,ty=targetingY*unit;
            const int size=std::max(18,static_cast<int>(30.f*unit*targetingFontScale));
            Label(content,"Pick3_SubtitleShadow",subtitle,tx+2.f*unit,ty-3.f*unit,
                  designW*.94f,76.f*unit,size,Color{.04f,.13f,.03f,1.f},true);
            Label(content,"Pick3_Subtitle",subtitle,tx,ty,
                  designW*.94f,76.f*unit,size,Color{.49f,1.f,.31f,1.f},true);
        };
        if(targetingBehindCards)createTargetingPrompt();
        if (gameplaySubmissionEnabled)
            Label(content, "Pick3_Status", "", 0, .315f * designH,
                  w*.95f, 50.f*unit, std::max(12, static_cast<int>(21.f*unit)),
                  Color{.98f, .93f, .72f, 1.f}, true);

        const std::uint64_t perfBeforeCards=PerfClockMs();
        // Never display unrelated sample-card data while definitions load.
        const char* descriptions[3] = {"Text unavailable", "Text unavailable", "Text unavailable"};
        const char* costs[3] = {"?", "?", "?"};
        const char* stats[3] = {"?", "?", "?"};
        const float xs[3] = {0.f, -xGap, xGap};
        const float ys[3] = {topY, lowerY, lowerY};
        cardUnit=unit;
        for (int i = 0; i < activeChoiceCount; ++i) {
            cardOriginX[i]=xs[i];cardOriginY[i]=ys[i];
            char cardGroupName[40]{};
            std::snprintf(cardGroupName,sizeof(cardGroupName),"Pick3_RewardCard_%d",i+1);
            Obj* cardParent=CreateGO(cardGroupName,content);
            if(!cardParent){Close();return false;}
            Position(cardParent,xs[i],ys[i],cardW*1.50f,cardH*2.5f);
            if(canvasGroupClass.IsValid() && setCanvasGroupAlpha.IsValid()) {
                Obj* g=addComponent[cardParent](canvasGroupClass.GetMonoType());
                if(g)setCanvasGroupAlpha[g](entranceApiReady?0.f:1.f);
            }
            const float cx = 0.f, cy = 0.f;
            // Native inspection grass and native inspection rim, without an
            // invented class-colored overlay. White name and ability plate.
            const float artY = cy + cardH * layout.portraitY;
            portraitOriginY[i]=artY;
            const float artSize = cardW * layout.portraitSize;
            char avatarParentName[48]{};
            std::snprintf(avatarParentName,sizeof(avatarParentName),"Pick3_AnimatedPortrait_%d",i+1);
            Obj* avatarParent=CreateGO(avatarParentName,cardParent);
            if(!avatarParent){Close();return false;}
            Position(avatarParent,0,artY,artSize*1.25f,artSize*1.25f);
            if(entranceApiReady && setLocalScale.IsValid()){
                Obj* g=addComponent[avatarParent](canvasGroupClass.GetMonoType());
                if(g)setCanvasGroupAlpha[g](0.f);
            }
            // Genuine sprite frame_circle_grass from user-recovered UnityFS,
            // embedded and GC-rooted; no runtime game-cache dependency. It has its own circular
            // transparency and cannot mask any live fighter/FX animation.
            char grassName[48]{};
            std::snprintf(grassName,sizeof(grassName),"Pick3_InspectionGrass_%d",i+1);
            Obj* grass=RewardImage(avatarParent,grassName,cx,0.f,
                artSize*inspectionGrassSize,artSize*inspectionGrassSize,
                Pick3RewardArt::InspectionGrass,Color{1,1,1,0},false);
            // Keep the shell visible from the first frame, even without card art.
            (void)grass;
            // Only draw actual card artwork here, never a demonstration image.
            char liveName[] = "Pick3_LiveSprite_0";
            liveName[17] = static_cast<char>('1' + i);
            Obj* liveLayer = Panel(avatarParent, liveName, cx, 0.f,
                                   cardW*.70f, cardW*.70f,
                                   Color{1.f,1.f,1.f,0.f}, false);
            if (liveLayer && setImagePreserveAspect.IsValid()) {
                Obj* graphic = getComponent[liveLayer](imageClass.GetMonoType());
                if (graphic) setImagePreserveAspect[graphic](true);
            }
            // Original inspection ring, loaded from our own embedded PNG.
            // It deliberately remains its natural colors (white UI tint).
            // This is not the game's per-class colorization yet.
            char referenceName[48]{};
            std::snprintf(referenceName,sizeof(referenceName),"Pick3_ReferenceRim_%d",i+1);
            RewardImage(avatarParent,referenceName,cx,0.f,artSize,artSize,
                        Pick3RewardArt::InspectionReferenceRim,Color{1,1,1,1},false);
            // Half-frame colorization using two UGUI Image.Filled layers of the
            // authentic native inspection rim. No prefab instantiation, shader
            // guessing, mask component, or unmanaged scene-wide asset search.
            // Preserve the original untinted full rim below as a fallback.
            if (useStockClassRimTint) {
                // Two verified PNG halves derived offline from our own
                // shipped inspection rim. NO UI.Image.Filled native methods,
                // no duplicate UnityFS asset loads or scene-owned references.
                const Pick3RewardArt::Kind kinds[]={Pick3RewardArt::InspectionRimLeft,
                                                    Pick3RewardArt::InspectionRimRight};
                const char* prefix[]={"Pick3_ClassRimLeft_","Pick3_ClassRimRight_"};
                for (int side=0;side<2;++side) {
                    char layerName[48]{};
                    std::snprintf(layerName,sizeof(layerName),"%s%d",prefix[side],i+1);
                    Obj* layer=RewardImage(avatarParent,layerName,cx,0.f,
                                             artSize,artSize,kinds[side],
                                             Color{1.f,1.f,1.f,0.f},false);
                    Obj* graphic=layer?getComponent[layer](imageClass.GetMonoType()):nullptr;
                    if (graphic) setGraphicColor[graphic](Color{1.f,1.f,1.f,0.f});
                }
            }
            nativeArtworkOriginY[i]=artY;
            nativeArtworkCardWidth[i]=cardW;
            if (avatarExperimentStage>=3 && nativeAvatarRenderTestEnabled && Pick3NativeAvatarRender::ready) {
                char rawName[48]{};
                std::snprintf(rawName,sizeof(rawName),"Pick3_NativeAvatarRaw_%d",i+1);
                // Render the full CardView outside the decorative class circle.
                // RawImage is non-raycast, so buttons and cards remain interactive.
                Obj* rawGO=CreateGO(rawName,avatarParent);
                if(rawGO){
                    Position(rawGO,0.f,0.f,cardW*nativeViewScale,
                             cardW*nativeViewScale);
                    Pick3NativeAvatarRender::AttachRaw(i,rawGO);
                }
            }
            char costBadge[40]{}, attackBadge[40]{}, healthBadge[40]{};
            std::snprintf(costBadge, sizeof(costBadge), "Pick3_CostBadge_%d", i+1);
            std::snprintf(attackBadge, sizeof(attackBadge), "Pick3_AttackBadge_%d", i+1);
            std::snprintf(healthBadge, sizeof(healthBadge), "Pick3_HealthBadge_%d", i+1);
            const float badgeSize = cardW*layout.badgeScale;
            const float attackX = cx - cardW*layout.statSpacing;
            const float healthX = cx + cardW*layout.statSpacing;
            const float statsY = artY - artSize*layout.statsBelowPortrait;
            // Use the packaged static cost badge; independent of scene state.
            RewardImage(cardParent, costBadge, cx+cardW*.41f, artY+artSize*.34f,
                  badgeSize, badgeSize,
                  Pick3RewardArt::CostSun, Color{1,1,1,0}, false);
            // RewardImage applies its sprite's default tint; explicitly keep
            // the badge hidden until the real faction has been read.
            if (Obj* costGO=findByName(BNM::CreateMonoString(costBadge))) {
                if (Obj* costImage=getComponent[costGO](imageClass.GetMonoType()))
                    setGraphicColor[costImage](Color{1.f,1.f,1.f,0.f});
            }
            RewardImage(cardParent, attackBadge, attackX, statsY,
                        badgeSize, badgeSize, Pick3RewardArt::AttackLeaf,
                        Color{.36f,.74f,.16f,1.f});
            RewardImage(cardParent, healthBadge, healthX, statsY,
                        badgeSize, badgeSize, Pick3RewardArt::HealthHeart,
                        Color{.85f,.25f,.19f,1.f});
            char costLabel[] = "Pick3_Cost_0";
            char nameLabel[] = "Pick3_Name_0";
            char attackLabel[40]{}, healthLabel[40]{};
            char descLabel[] = "Pick3_Desc_0";
            costLabel[11] = nameLabel[11] = descLabel[11] = static_cast<char>('1' + i);
            std::snprintf(attackLabel, sizeof(attackLabel), "Pick3_Attack_%d", i+1);
            std::snprintf(healthLabel, sizeof(healthLabel), "Pick3_Health_%d", i+1);
            // Text is constrained to the badge's central area, with TMP
            // autosizing enabled for multi-digit values (not a fixed 21px font).
            const float baseBadgeFont = std::min(cardText*layout.statScale,badgeSize*.45f);
            const auto badgeFont=[&](float multiplier) {
                return std::max(9,static_cast<int>(std::lround(baseBadgeFont*multiplier)));
            };
            Label(cardParent, costLabel, costs[i],
                  cx+cardW*.41f+badgeSize*statText.costX,
                  artY+artSize*.34f+badgeSize*statText.costY,
                  badgeSize*statText.costWidth, badgeSize*.85f,
                  badgeFont(statText.costSize), Color{.13f,.08f,.04f,1.f}, false, false);
            Label(cardParent, attackLabel, "?",
                  attackX+badgeSize*statText.attackX,statsY+badgeSize*statText.attackY,
                  badgeSize*statText.attackWidth,badgeSize*.85f,
                  badgeFont(statText.attackSize), Color{1,1,.90f,1},false,false);
            Label(cardParent, healthLabel, "?",
                  healthX+badgeSize*statText.healthX,statsY+badgeSize*statText.healthY,
                  badgeSize*statText.healthWidth,badgeSize*.85f,
                  badgeFont(statText.healthSize),Color{1,1,.90f,1},false,false);
            char placeholderName[48]{};
            std::snprintf(placeholderName, sizeof(placeholderName), "Card #%d", previewCardIds[i]);
            const float nameY = statsY-badgeSize*layout.nameBelowStats;
            char nameShadow[40]{};
            std::snprintf(nameShadow, sizeof(nameShadow), "Pick3_NameShadow_%d", i+1);
            Label(cardParent, nameShadow, placeholderName, cx+1.3f*unit, nameY-2.0f*unit,
                  cardW*1.23f, cardH*.145f,
                  std::max(13,static_cast<int>(cardText*layout.nameScale)),
                  Color{.015f,.035f,.013f,1.f}, true);
            Label(cardParent, nameLabel, placeholderName, cx, nameY,
                  cardW*1.23f, cardH*.145f,
                  std::max(13,static_cast<int>(cardText*layout.nameScale)),
                  Color{1,1,1,1}, true);
            const float plateY = nameY-cardH*layout.plateBelowName;
            descriptionPlateY[i]=plateY;descriptionCardW[i]=cardW;
            descriptionCardH[i]=cardH;
            descriptionBasePlateHeight[i]=cardH*layout.plateHeight;
            char plateName[48]{};
            std::snprintf(plateName,sizeof(plateName),"Pick3_DescriptionPlate_%d",i+1);
            RewardImage(cardParent, plateName, cx, plateY,
                        cardW*layout.plateWidth, cardH*layout.plateHeight,
                        Pick3RewardArt::DescriptionPlate, Color{.98f,.96f,.84f,1.f});
            Label(cardParent, descLabel, descriptions[i],
                  cx+cardW*layout.plateWidth*statText.descriptionX,
                  plateY+cardH*layout.plateHeight*statText.descriptionY,
                  cardW*layout.plateWidth*statText.descriptionWidth,
                  cardH*layout.plateHeight*statText.descriptionHeight,
                  std::max(9,static_cast<int>(std::lround(19.f*unit*statText.descriptionSize))),
                  Color{.15f,.13f,.10f,1.f});
            const float buttonY = plateY-cardH*layout.buttonBelowPlate;
            descriptionButtonY[i]=buttonY;
            char buttonName[40]{}, textName[40]{};
            std::snprintf(buttonName, sizeof(buttonName), "Pick3_ChooseButton_%d", i);
            std::snprintf(textName, sizeof(textName), "Pick3_ChooseText_%d", i);
            // The game's original reward-choice button_tint Sprite, if its
            // UnityFS bundle resolves. Sprite geometry/rounded edges are not
            // recreated from a PNG. A simple neutral control remains if the
            // native subasset is unavailable on this Unity build.
            Obj* buttonGO=Panel(cardParent, buttonName, cx, buttonY,
                  cardW*layout.buttonWidth,buttonH*layout.buttonHeight,
                  Color{1.f,1.f,1.f,1.f}, true);
            if(buttonGO){
                Obj* buttonSprite=LoadRewardSprite(Pick3RewardArt::ChooseButton);
                if(!buttonSprite)buttonSprite=Pick3LocalAssetBundles::Sprite(6);
                Obj* buttonImage=getComponent[buttonGO](imageClass.GetMonoType());
                if(buttonImage && buttonSprite){
                    setImageSprite[buttonImage](buttonSprite);
                    // The exact HeroesButton interactive tint and transition
                    // are NOT asserted by this static bundle loader.
                    setGraphicColor[buttonImage](Color{1.f,1.f,1.f,1.f});
                    if(setImagePreserveAspect.IsValid())setImagePreserveAspect[buttonImage](false);
                    // Sprite border is preserved by UI.Image's 9-sliced mode;
                    // avoid distorting the stock rounded border when resized.
                    struct SliceBorders {float left,bottom,right,top;};
                    auto getBorder=spriteClass.GetMethod("get_border",0).cast<SliceBorders>();
                    auto setType=imageClass.GetMethod("set_type",1).cast<void>();
                    if(false && Pick3LocalAssetBundles::enableButtonSlicing &&
                       getBorder.IsValid() && setType.IsValid()){
                        SliceBorders borders{};
                        if(!BNM::TryInvoke([&]{borders=getBorder[buttonSprite]();}).IsValid() &&
                           (borders.left>0||borders.bottom>0||borders.right>0||borders.top>0))
                            BNM::TryInvoke([&]{setType[buttonImage](1);}); // Image.Type.Sliced
                    }
                }
            }
            // Each button stays in its option's card hierarchy, above its
            // background and description. Preserve natural option order;
            // no cross-card sibling reorder (could cover other controls).
            char chooseShadow[40]{};
            std::snprintf(chooseShadow, sizeof(chooseShadow), "Pick3_ChooseShadow_%d", i+1);
            const float textY=buttonY+buttonH*layout.buttonTextOffset;
            Label(cardParent, chooseShadow, "Choose", cx+1.5f*unit, textY-1.5f*unit,
                  cardW*layout.buttonWidth*.95f,buttonH*layout.buttonHeight*.95f,
                  buttonText, Color{.02f,.12f,.03f,1.f}, true, false);
            Label(cardParent, textName, "Choose", cx,textY,
                  cardW*layout.buttonWidth*.95f,buttonH*layout.buttonHeight*.95f,
                  buttonText, Color{1.f,1.f,1.f,1.f}, true, false);
            choiceRects[i] = HitRect{w*.5f+xs[i], h*.5f+ys[i]+buttonY,
                                     cardW*layout.buttonWidth*.5f,
                                     buttonH*layout.buttonHeight*.5f};
            ResizeDescriptionPlate(i);
        }
        const std::uint64_t perfAfterCards=PerfClockMs();
        LOG_INFO("CustomComponents/Pick3/PERF: CARD_UI elapsedMs=%llu count=%d",
                 static_cast<unsigned long long>(perfAfterCards-perfBeforeCards),activeChoiceCount);
        // Optional foreground subtitle is intentionally created after the
        // cards, but uses the same positioning and localization key.
        if(!targetingBehindCards)createTargetingPrompt();
        // Keep the original dismiss button and hitbox code available for
        // future optional/cancelable selections; mandatory choice is default.
        closeRect = HitRect{};
        if (showCloseButton) {
            const float closeX = designW * .42f, closeY = designH * .44f;
            Panel(content, "Pick3_CloseButton", closeX, closeY, 70.f*unit, 70.f*unit,
                  Color{.19f,.22f,.28f,1.f});
            Label(content, "Pick3_CloseText", "X", closeX, closeY, 70.f*unit,70.f*unit,
                  buttonText,Color{1.f,1.f,1.f,1.f},true);
            closeRect = HitRect{w*.5f+closeX,h*.5f+closeY,35.f*unit,35.f*unit};
        }
        // Build the tooltip while the resident game's Cafeteria font/material
        // is borrowed for this popup. Keeping it active at zero alpha avoids
        // GameObject.Find missing it on later click callbacks.
        if(inlineLinkApiReady && canvasGroupClass.IsValid()) {
            Obj* tip=RewardImage(root,"Pick3_InlineTooltip",0,0,100.f*unit,54.f*unit,
                           Pick3RewardArt::InlineTooltipPlate,Color{1,1,1,0},false);
            if(tip){
                Obj* group=addComponent[tip](canvasGroupClass.GetMonoType());
                if(group && setCanvasGroupAlpha.IsValid())setCanvasGroupAlpha[group](0.f);
                Label(tip,"Pick3_InlineTooltipText","",0,0,90.f*unit,48.f*unit,
                      std::max(12,static_cast<int>(17.f*unit*tooltipFontScale)),
                      Color{1.f,.98f,.86f,1.f},false,true);
            }
        }
        sceneFont = nullptr;
        cafeteriaFont = nullptr;
        isOpen = true;
        presentationStartFrame = frameCounter;
        presentationStart = PresentationClock();
        ResetChoiceTiming();
        if(artTraceModeForPopup)
            for(int i=0;i<activeChoiceCount;++i)
                LOG_INFO("CustomComponents/Pick3/ART_TRACE: CANDIDATE option=%d guid=%d color=%d resident=%d async=%d",
                         i+1,previewCardIds[i],previewColors[i],
                         residentArtEnabledForPopup?1:0,useAsyncGameArt?1:0);
        presentationVisible = !entranceApiReady;
        localizationResolved = false;
        if (externalOnly && requestActive)
            LOG_INFO("CustomComponents/Pick3/UI: PREPARE sourceGuid=%d preloadMs=%d enterMs=%d targetIcons=stockOwned",
                     activeRequest.sourceCardGuid,
                     static_cast<int>(preloadSeconds * 1000.f),
                     static_cast<int>(enterSeconds * 1000.f));
        if (gameplaySubmissionEnabled) {
            if (!Pick3::Submission::PopupCanOpen()) {
                LOG_ERR("CustomComponents/Pick3/INPUT: SUBMISSION_UNAVAILABLE; popup cannot open");
                DestroyPopupRoot("missing-input-lock");
                isOpen = false;
                return false;
            }
            Pick3::Submission::SetPopupInputCapture(true);
            CC_DIAG("CustomComponents/Pick3/INPUT: UGUI_OVERLAY_ACTIVE stockCommands=guarded touchScript=unchanged (NOT FULLY MODAL)");
        }
        // NativeAvatarLoad::Start() rejects requests unless BeginSession()
        // has armed popupOpen. This must happen only after the screen has
        // successfully been created and gameplay input preflight has passed.
        // No artificial wait: Open() still presents immediately.
        if (nativeAvatarLoadTestEnabled && avatarExperimentStage>=2) Pick3NativeAvatarLoad::BeginSession();
        std::fill(std::begin(nativeAvatarLoadStartAttempts),std::end(nativeAvatarLoadStartAttempts),0);
        std::fill(std::begin(nativeAvatarLoadStarted),std::end(nativeAvatarLoadStarted),false);
        std::fill(std::begin(nativeAvatarRenderBound),std::end(nativeAvatarRenderBound),false);
        UpdatePickStatus();
        if (nativeCacheTest && activeChoiceCount == 3) Pick3SpriteCache::BeginSession(previewCardIds);
        ++asyncGeneration;
        ReleaseAsyncAssets();
        asyncTickCount = 0;
        std::fill(std::begin(liveImageReady), std::end(liveImageReady), false);
        spriteProbeCount = 0;
        std::fill(std::begin(metadataLoaded), std::end(metadataLoaded), false);
        std::fill(std::begin(textLoaded), std::end(textLoaded), false);
        metadataAttempts = 0;
        const std::uint64_t perfBeforeMetadata=PerfClockMs();
        UpdateCardMetadata();
        UpdateTargetingLocalization();
        const std::uint64_t perfAfterMetadata=PerfClockMs();
        LOG_INFO("CustomComponents/Pick3/PERF: METADATA elapsedMs=%llu",
                 static_cast<unsigned long long>(perfAfterMetadata-perfBeforeMetadata));
        if(artTraceModeForPopup>=2)
            LOG_INFO("CustomComponents/Pick3/ART_TRACE: OPEN_DISPATCH enabled=%d ready=%d",useAsyncGameArt?1:0,asyncMethodsReady?1:0);
        // Non-loading path: check already resident CardFaceSpriteHolders first.
        // Async requests remain an explicit experimental opt-in.
        // Do not scan the scene synchronously during popup creation.
        BeginGameArtRequests();
        RequestInspectionPrefab();
        const std::uint64_t perfFinish=PerfClockMs();
        LOG_INFO("CustomComponents/Pick3/PERF: OPEN totalMs=%llu priorMs=%llu uiAndInputMs=%llu",
                 static_cast<unsigned long long>(perfFinish-perfStart),
                 static_cast<unsigned long long>(perfBeforeFont-perfStart),
                 static_cast<unsigned long long>(perfFinish-perfAfterPreload));
        CC_DIAG("CustomComponents/Pick3Preview: SHOWN actual card art or empty portrait; %s",
                 showCloseButton ? "choose a button or X to close" : "choose required NumTargets (1..3); X hidden");
        return true;
    }

    inline void HideDescriptionTooltip(){
        if(!inlineTooltipShowing||inlineTooltipClosing)return;
        inlineTooltipClosing=true;
        inlineTooltipCloseStart=PresentationClock();
        inlineTooltipCooldownUntil=inlineTooltipCloseStart+tooltipExitSeconds+.12f;
        inlineTooltipTappedPulse=false;
    }
    inline void AnimateInlineTooltip(){
        if(!isOpen || !findByName.IsValid() || !setLocalScale.IsValid())return;
        if(inlineTooltipShowing){
            Obj* go=findByName(BNM::CreateMonoString("Pick3_InlineTooltip"));
            if(go){
                const float age=PresentationClock()-(inlineTooltipClosing?inlineTooltipCloseStart:inlineTooltipStart);
                const float duration=inlineTooltipClosing?tooltipExitSeconds:tooltipEnterSeconds;
                float t=std::clamp(age/std::max(.001f,duration),0.f,1.f);
                // Lightweight back-easing on open, smooth fade on close.
                float alpha=inlineTooltipClosing?1.f-t:t*t*(3.f-2.f*t);
                float scale=inlineTooltipClosing?1.f-.06f*t:
                    .91f+.09f*(1.f-std::pow(1.f-t,3.f));
                Obj* group=canvasGroupClass.IsValid()?getComponent[go](canvasGroupClass.GetMonoType()):nullptr;
                if(group && setCanvasGroupAlpha.IsValid())setCanvasGroupAlpha[group](alpha);
                Obj* tr=getTransform[go]();if(tr)setLocalScale[tr](Vec3{scale,scale,1.f});
                if(inlineTooltipClosing && t>=1.f){
                    inlineTooltipShowing=false;inlineTooltipClosing=false;
                    inlineTooltipTappedOption=-1;
                    if(group && setCanvasGroupAlpha.IsValid())setCanvasGroupAlpha[group](0.f);
                }
            }
        }
        if(inlineTooltipTappedOption>=0 && inlineTooltipTappedOption<activeChoiceCount){
            const int i=inlineTooltipTappedOption;
            char name[40]{};std::snprintf(name,sizeof(name),"Pick3_Desc_%d",i+1);
            Obj* go=findByName(BNM::CreateMonoString(name));
            if(go){
                const float t=std::clamp((PresentationClock()-inlineTooltipStart)/.14f,0.f,1.f);
                const float pulse=1.f-.042f*std::sin(t*3.14159265f);
                Obj* tr=getTransform[go]();if(tr)setLocalScale[tr](Vec3{pulse,pulse,1.f});
                if(t>=1.f)inlineTooltipTappedOption=-1;
            }else inlineTooltipTappedOption=-1;
        }
    }

    inline void HandleDescriptionTap(Vec3 pos, float sw, float sh) {
        if (!inlineLinkApiReady || !tmpReady || !isOpen || inlineTooltipClosing ||
            PresentationClock()<inlineTooltipCooldownUntil) return;
        // TMP's own rich-text parser determines which glyphs belong to a link;
        // geometry is not guessed from the text/plate rectangles.
        for(int i=0;i<activeChoiceCount;++i) {
            char name[40]{};std::snprintf(name,sizeof(name),"Pick3_Desc_%d",i+1);
            Obj* go=findByName(BNM::CreateMonoString(name));
            Obj* label=go?getComponent[go](tmpTextClass.GetMonoType()):nullptr;
            if(!label)continue;
            int index=-1;
            auto hit=BNM::TryInvoke([&]{ index=tmpFindLink(label,pos,static_cast<Obj*>(nullptr)); });
            if(hit.IsValid()||index<0)continue;
            Obj* info=nullptr;
            auto get=BNM::TryInvoke([&]{info=tmpGetTextInfo[label]();});
            if(get.IsValid()||!info)continue;
            auto* links=tmpLinks[info].Get();
            if(!links||index>=links->GetSize()||index>=links->GetCapacity())continue;
            const TmpLinkInfo& link=links->GetData()[index];
            if(!link.link)continue;
            std::string key=link.link->str();
            if(key.empty()||key.size()>128)continue;
            int param=0;
            if(link.param){
                const std::string argument=link.param->str();
                if(!argument.empty()) {
                    char* end=nullptr;
                    const long value=std::strtol(argument.c_str(),&end,10);
                    if(end!=argument.c_str()&&*end=='\0'&&value>=-999&&value<=999)param=static_cast<int>(value);
                }
            }
            auto meta=Pick3CardMetadata::Read(previewCardIds[i],nullptr);
            if(!meta.hasCardData)continue;
            std::string description;
            Obj* provider=Pick3NativeAvatarLoad::ResolveInjected(keywordInfoProviderClass, "keyword-info-provider");
            if(provider && keywordDescription.IsValid()) {
                BNM::Structures::Mono::String* result=nullptr;
                auto ex=BNM::TryInvoke([&]{result=keywordDescription[provider](
                       BNM::CreateMonoString(key),meta.zombie?1:0,param);});
                if(!ex.IsValid()&&result)description=result->str();
            }
            // If the DI provider is unavailable, use the very same localized
            // key namespace used internally by KeywordAbilityInfoProvider.
            if(description.empty()) {
                Obj* mapper=Pick3CardMetadata::FindLocalizationMapper();
                std::string lower=key;
                std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                Pick3CardMetadata::TryRawLocalizationKey(mapper,
                    std::string(meta.zombie?"ability_desc_zombies_":"ability_desc_plants_")+lower,description);
            }
            if(description.empty()) {
                LOG_ERR("CustomComponents/Pick3/TOOLTIP: no description key='%s' faction=%d",key.c_str(),meta.zombie?1:0);
                return;
            }
            // Avoid scanning resident TaggedTextView mediators on a tap. The
            // localized keyword explanation can display directly in TMP.
            // Small rounded popup, independently layered above native avatars.
            // Its text is localized by the existing provider, not a new lookup.
            Obj* popup=findByName(BNM::CreateMonoString("Pick3_InlineTooltip"));
            if(!popup)return;
            const float unit=std::min(sw/480.f,sh/900.f);
            const float width=std::min(sw*tooltipMaxWidthScale,296.f*unit);
            const float textSize=std::max(12.f,17.f*unit*tooltipFontScale);
            const float estimatedLines=std::max(1.f,std::ceil(
                 static_cast<float>(description.size())/std::max(10.f,width/(textSize*.53f))));
            const float height=std::min(sh*tooltipMaxHeightScale,
                   std::clamp((estimatedLines*textSize*1.35f+25.f*unit),48.f*unit,112.f*unit));
            const float centerX=std::clamp(pos.x-sw*.5f,-(sw-width)*.5f,(sw-width)*.5f);
            const float aboveY=pos.y-sh*.5f+height*.70f;
            const float centerY=std::clamp(aboveY,-(sh-height)*.5f,(sh-height)*.5f);
            Position(popup,centerX,centerY,width,height);
            Obj* textGo=findByName(BNM::CreateMonoString("Pick3_InlineTooltipText"));
            if(textGo){Position(textGo,0.f,0.f,width*.87f,height*.72f);
                SetLabelValue(textGo,description.c_str());}
            inlineTooltipStart=PresentationClock();
            inlineTooltipClosing=false;
            inlineTooltipShowing=true;
            inlineTooltipTappedOption=i;
            Obj* tr=getTransform[popup]();if(tr&&setLocalScale.IsValid())
                setLocalScale[tr](Vec3{.91f,.91f,1.f});
            if(canvasGroupClass.IsValid() && setCanvasGroupAlpha.IsValid()) {
                Obj* group=getComponent[popup](canvasGroupClass.GetMonoType());
                if(group)setCanvasGroupAlpha[group](0.f);
            }
            LOG_INFO("CustomComponents/Pick3/TOOLTIP: show option=%d keyword='%s' param=%d safeSession=1",i+1,key.c_str(),param);
            return;
        }
    }

    inline void UpdateInspectionCircleVisibility(int) {
        // Deliberately a no-op. Grass and the verified inspection ring are
        // structural UI layers: absent/late portrait assets never hide them.
    }

    inline void ApplyVisibleNativeAvatars() {
        if(!isOpen || avatarExperimentStage<3 || !nativeAvatarRenderTestEnabled || !Pick3NativeAvatarRender::ready)return;
        for(int i=0;i<activeChoiceCount && i<3;++i){
            if(!Pick3NativeAvatarRender::Texture(i))continue;
            char name[48]{};
            std::snprintf(name,sizeof(name),"Pick3_NativeAvatarRaw_%d",i+1);
            Obj* raw=findByName(BNM::CreateMonoString(name));
            if(!raw)continue;
            if(!nativeAvatarRenderBound[i]){
                // Center all cards over the disc, but give the taller
                // environment artwork an independent upward adjustment.
                const bool environment=Pick3NativeAvatarRender::IsEnvironment(i);
                const float scale=environment?environmentViewScale:nativeViewScale;
                // RawImage is parented to AnimatedPortrait, not RewardCard.
                // Keep local coordinates stable while that parent animates.
                const float y=environment ? environmentViewOffsetY*nativeArtworkCardWidth[i] : 0.f;
                Position(raw,0.f,y,nativeArtworkCardWidth[i]*scale,
                                    nativeArtworkCardWidth[i]*scale);
                nativeAvatarRenderBound[i]=Pick3NativeAvatarRender::Bind(i,raw);
                if(nativeAvatarRenderBound[i])
                    LOG_INFO("CustomComponents/Pick3/AVATAR_RENDER: UI_BOUND option=%d env=%d viewScale=%.2f y=%.1f masked=0",
                             i+1,environment?1:0,scale,y);
            }
            UpdateInspectionCircleVisibility(i);
            if(nativeAvatarRenderBound[i]){
                std::snprintf(name,sizeof(name),"Pick3_LiveSprite_%d",i+1);
                Obj* old=findByName(BNM::CreateMonoString(name));
                Obj* graphic=old?getComponent[old](imageClass.GetMonoType()):nullptr;
                if(graphic)setGraphicColor[graphic](Color{1.f,1.f,1.f,0.f});
            }
        }
    }

    inline void ApplyNativeCacheSpriteArt() {
        if (NativeArtOnly() || !nativeCacheTest || !isOpen || !Root() ||
            !Pick3SpriteCache::Completed()) return;
        for (int i = 0; i < 3; ++i) {
            if (liveImageReady[i]) continue;
            Obj* sprite = Pick3SpriteCache::GetSprite(i, previewCardIds[i]);
            if (!sprite) continue;
            char name[] = "Pick3_LiveSprite_0";
            name[17] = static_cast<char>('1' + i);
            Obj* go = findByName(BNM::CreateMonoString(name));
            Obj* graphic = go ? getComponent[go](imageClass.GetMonoType()) : nullptr;
            if (!graphic) continue;
            setImageSprite[graphic](sprite);
            setGraphicColor[graphic](Color{1.f, 1.f, 1.f, 1.f});
            if (setImagePreserveAspect.IsValid()) setImagePreserveAspect[graphic](true);
            liveImageReady[i] = true;
            RelabelForGameSprite(i, previewCardIds[i]);
            CC_DIAG("CustomComponents/Pick3/CACHE: APPLIED option=%d cardId=%d", i + 1, previewCardIds[i]);
        }
    }

    // Stage 1A: strictly passive tick/lifecycle diagnostic. Stage 1's old
    // synchronous CardMetadata::Read + BNM::Class lookups preceded its first
    // log, so the last experiment could crash without identifying the call.
    // Don't touch IL2CPP metadata, Unity objects or card model here.
    inline void AvatarExperimentPreflight(){
        if(avatarExperimentStage==0 || avatarExperimentPreflightLogged ||
           !isOpen || !requestActive) return;
        avatarExperimentPreflightLogged=true;
        const int index=avatarExperimentChoice;
        LOG_INFO("CustomComponents/Pick3/ANIMATION_EXPERIMENT: PREFLIGHT_1A ENTER option=%d count=%zu nativeCalls=0 metadataCalls=0 classLookups=0",index+1,activeRequest.options.size());
        if(index>=0 && index<static_cast<int>(activeRequest.options.size())){
            const int guid=activeRequest.options[index].cardGuid;
            LOG_INFO("CustomComponents/Pick3/ANIMATION_EXPERIMENT: PREFLIGHT_1A DONE option=%d guid=%d nativeCalls=0",index+1,guid);
        } else {
            LOG_INFO("CustomComponents/Pick3/ANIMATION_EXPERIMENT: PREFLIGHT_1A NO_OPTION index=%d",index+1);
        }
    }

    // Registered into the EXISTING EventSystem.Update dispatcher. Never hooks it twice.
    static void Tick() {
        if (!enabled) return;
        ++frameCounter;
        // CPU-only checks in menus; optional PNG-only work after a sustained
        // match. Never LoadAsset() from this startup callback.
        StartupPrewarmTick();
        // Diagnostic bypass of work that continues *after* Open() succeeds.
        // Keep base dispatch and the popup Unity objects intact; this is not a fix.
        const int postOpenIsolation=Pick3PostOpenIsolation::ReadMode();
        if(!(isOpen && postOpenIsolation>=2)) {
            ServiceAsyncRequests();
            ServiceInspectionPrefab();
        }
        if (avatarExperimentStage>=2) Pick3NativeAvatarLoad::Tick(PresentationClock());
        // Destroy() is deferred in Unity. Check after it has had a frame to
        // run; if a live orphan remains, deactivate it again before UI input.
        if (verifyClosedNextTick) {
            verifyClosedNextTick = false;
            Obj* owned = popupRootHandle && Core::SceneViewContext::getGCHandle
                ? Core::SceneViewContext::getGCHandle(popupRootHandle) : nullptr;
            bool alive = false;
            bool reliable = false;
            if (owned && unityObjectAlive.IsValid()) {
                const auto ex = BNM::TryInvoke([&] { alive = unityObjectAlive(owned); });
                reliable = !ex.IsValid();
            }
            // GameObject.Find ignores inactive GameObjects: explicitly inspect
            // the GC-rooted object, not merely Find(rootName).
            if (reliable && alive) {
                LOG_ERR("CustomComponents/Pick3Preview: CLOSE_PENDING native Unity canvas still alive; retrying destroy");
                DestroyPopupRoot("close-watchdog");
            } else if (owned && !reliable) {
                CC_DIAG("CustomComponents/Pick3Preview: CLOSE_VERIFY_FALLBACK Unity alive check unavailable");
                DestroyPopupRoot("close-fallback");
            } else {
                CC_DIAG("CustomComponents/Pick3Preview: CLOSE_VERIFIED native canvas destroyed");
                if (popupRootHandle && Core::SceneViewContext::freeGCHandle)
                    Core::SceneViewContext::freeGCHandle(popupRootHandle);
                popupRootHandle = 0;
            }
        }
        if (nativeCacheTest) Pick3SpriteCache::Tick(isOpen, previewCardIds);
        if (isOpen && postOpenIsolation>=1) {
            // Mode 1/2 suppress UI Tick and all its managed calls. With mode 1,
            // async asset servicing above still runs; mode 2 suppresses both.
            return;
        }
        if (isOpen) {
            AvatarExperimentPreflight();
            AnimateInlineTooltip();
            // Never postpone the popup to look up optional native avatar metadata.
            // Mode 1 only logs metadata. Mode 2 opts into the existing
            // managed injection lookup; no card is created in either mode.
            if (factoryProbeModeForPopup >= 2) {
                if (nativeAvatarProbeAttempts == 0)
                    LOG_INFO("CustomComponents/Pick3/FACTORY_TEST: MODE2_TICK isOpen=%d requestActive=%d choices=%d configProbe=%d",
                        isOpen, requestActive, activeChoiceCount, nativeAvatarProbeEnabled);
                ProbeNativeAvatarFactory();
            }
            // Bounded native factory experiment. Only one new request is
            // attempted per interval; level 2/3 select ONE card. Level 4
            // advances to the next option only after the previous finishes
            // (or becomes unavailable), never all three in one frame.
            // All of this is behind the *explicit* factory-unsafe opt-in.
            if (nativeAvatarLoadTestEnabled && avatarExperimentStage>=2 &&
                requestActive &&
                (factoryProbeModeForPopup == 0 || factoryProbeModeForPopup >= 5)) {
                const int limit=avatarExperimentStage==4?activeChoiceCount:1;
                if(avatarExperimentNextIndex<limit) {
                    const int i=avatarExperimentStage==4?avatarExperimentNextIndex:avatarExperimentChoice;
                    if(i<3 && i<static_cast<int>(activeRequest.options.size())){
                        if (avatarExperimentStage==4 && i>0 &&
                            !Pick3NativeAvatarLoad::OptionFinished(i-1)) {
                            // No parallel factory calls: the previous CardView
                            // must finish its callback/initialization first.
                            if(!avatarExperimentTimeoutLogged &&
                               frameCounter-avatarExperimentLastStartFrame>360){
                                avatarExperimentTimeoutLogged=true;
                                LOG_ERR("CustomComponents/Pick3/ANIMATION_EXPERIMENT: STALLED previousOption=%d elapsedFrames=%u; refusing concurrent factory request",i,frameCounter-avatarExperimentLastStartFrame);
                            }
                        } else if(avatarExperimentStage>=3 && !Pick3NativeAvatarRender::ready){
                            if(!avatarExperimentTimeoutLogged){
                                avatarExperimentTimeoutLogged=true;
                                LOG_ERR("CustomComponents/Pick3/ANIMATION_EXPERIMENT: NO_RENDERER ready=0; static portrait preserved, no native factory call");
                            }
                            avatarExperimentNextIndex=limit;
                        } else if (avatarExperimentLastStartFrame==0 || avatarExperimentSpacingFrames==0 ||
                            frameCounter-avatarExperimentLastStartFrame>=static_cast<unsigned>(avatarExperimentSpacingFrames)){
                            avatarExperimentLastStartFrame=frameCounter;
                            const int guid=activeRequest.options[i].cardGuid;
                            ++nativeAvatarLoadStartAttempts[i];
                            if(animationCardInfoGuid[i]!=guid || !animationCardInfo[i].hasCardData){
                                LOG_INFO("CustomComponents/Pick3/ANIMATION_EXPERIMENT: SKIP option=%d guid=%d reason=%s fallback=static",i+1,guid,
                                    animationCardInfoGuid[i]!=guid || !animationCardInfo[i].hasCardData ? "metadata-pending" : "metadata-pending");
                                ++avatarExperimentNextIndex;
                            } else if(Pick3NativeAvatarLoad::Start(guid,i,animationCardInfo[i])){
                                nativeAvatarLoadStarted[i]=true;
                                ++avatarExperimentNextIndex;
                            } else {
                                // No repeated factory invocation on failure:
                                // the stock image stays and this option is skipped.
                                LOG_ERR("CustomComponents/Pick3/ANIMATION_EXPERIMENT: SKIP option=%d guid=%d nativeStartFailed=1 fallback=static",i+1,guid);
                                ++avatarExperimentNextIndex;
                            }
                        }
                    } else ++avatarExperimentNextIndex;
                }
            }
            // The service has already completed; only the visual dismissal
            // stays alive for a short, uninterruptible fade/scale-out.
            if (AnimateExit()) return;
            if (choiceTransitionActive) AnimateChoiceTransition();
            if (submitPending && PresentationClock() >= submitAt) {
                // Never dispatch a postponed game command into an exited match.
                if (!IsInMatch() || !Root()) {Close();return;}
                submitPending=false;
                Pick3::Submission::SetPopupInputCapture(false);
                if (Pick3::Submission::Submit(activeRequest,picked.Indices(),false)) {
                    requestActive=false;
                    BeginExitAnimation();
                } else if (Pick3::Submission::CanRetryLastSubmission()) {
                    picked.Begin(activeChoiceCount,activeRequest.requiredTargets);
                    choiceTransitionActive=false;
                    for(int j=0;j<activeChoiceCount;++j) {
                        char name[40]{};
                        std::snprintf(name,sizeof(name),"Pick3_RewardCard_%d",j+1);
                        Obj* go=findByName(BNM::CreateMonoString(name));
                        if(!go)continue;
                        Obj* rect=GetRect(go);
                        if(rect && setAnchoredPosition.IsValid())
                            setAnchoredPosition[rect](Vec2{cardOriginX[j],cardOriginY[j]});
                        Obj* group=canvasGroupClass.IsValid()?getComponent[go](canvasGroupClass.GetMonoType()):nullptr;
                        if(group && setCanvasGroupAlpha.IsValid())setCanvasGroupAlpha[group](1.f);
                        Obj* tr=getTransform[go]();
                        if(tr && setLocalScale.IsValid())setLocalScale[tr](Vec3{1.f,1.f,1.f});
                        // Restore independently animated selected-button pieces
                        // if the game's submission preflight rejects this pick.
                        const float textY=descriptionButtonY[j]+
                            std::max(34.f,64.f*cardUnit)*layout.buttonTextOffset;
                        for(int part=0;part<3;++part) {
                            char partName[48]{};
                            if(part==0)std::snprintf(partName,sizeof(partName),"Pick3_ChooseButton_%d",j);
                            else if(part==1)std::snprintf(partName,sizeof(partName),"Pick3_ChooseText_%d",j);
                            else std::snprintf(partName,sizeof(partName),"Pick3_ChooseShadow_%d",j+1);
                            Obj* child=findByName(BNM::CreateMonoString(partName));
                            if(!child)continue;
                            Obj* childRect=GetRect(child);
                            if(childRect && setAnchoredPosition.IsValid())
                                setAnchoredPosition[childRect](Vec2{part==2?1.5f*cardUnit:0.f,
                                    part==0?descriptionButtonY[j]:(textY-(part==2?1.5f*cardUnit:0.f))});
                            Obj* graphic=getComponent[child]((part==0?imageClass:textClass).GetMonoType());
                            if(graphic && setGraphicColor.IsValid())
                                setGraphicColor[graphic](part==2?Color{.02f,.12f,.03f,1.f}:
                                    Color{1.f,1.f,1.f,1.f});
                        }
                    }
                    UpdatePickStatus();
                    Pick3::Submission::SetPopupInputCapture(true);
                    LOG_ERR("CustomComponents/Pick3/SUBMIT: delayed preflight failed; UI rearmed");
                } else {
                    submissionServiceError=true;
                    UpdatePickStatus();
                    Pick3::Submission::SetPopupInputCapture(true);
                    LOG_ERR("CustomComponents/Pick3/SUBMIT: delayed service dispatch uncertain; no retry");
                }
                return;
            }
            // No recurring scene-wide asset discovery. Per-card native assets
            // use their single asynchronous request and stable session ticket.
            // Stock CardAssetsTask can publish its provider after the popup opens.
            // Retry metadata at most every 30 frames; localization may load later.
            if (metadataAttempts == 0 || frameCounter % 30 == 0) {
                UpdateCardMetadata();
                UpdateTargetingLocalization();
            }
            ApplyNativeCacheSpriteArt();
            ApplyAsyncGameArt();
            // Bounded late discovery: game-owned holders may be loaded by the
            // scene after this popup opens. Keep this read-only and infrequent.
            if (residentArtEnabledForPopup && cardFaceLookupReady && spriteProbeCount < kMaxSpriteProbes &&
                (frameCounter == presentationStartFrame + 1 ||
                 (frameCounter > presentationStartFrame &&
                  (frameCounter - presentationStartFrame) % 90 == 0))) {
                ProbeLiveArt();
            }
            ApplyVisibleNativeAvatars();
            if(!NativeArtOnly())for(int i=0;i<activeChoiceCount;++i)
                UpdateInspectionCircleVisibility(i);
            // One bounded retry for a loader that wasn't initialized on the
            // first frame. Never enqueue another request for the same option.
            if (useAsyncGameArt && asyncMethodsReady && ++asyncTickCount % 15 == 0)
                BeginGameArtRequests();
            // Re-try a few times as the game may load CardFaceSpriteHolders
            // after the local hand first appears. Do not loop forever.
            // Frontend transitions do not necessarily unload the Unity scene.
            // Our standalone ScreenSpaceOverlay Canvas can survive those views,
            // so actively close it when the local match hand disappears.
            if (frameCounter % 30 == 0) {
                const bool matchStillPresent = IsInMatch();
                if (!matchStillPresent || !Root()) {
                    CC_DIAG("CustomComponents/Pick3Preview: scene transition while open match=%d rootVisible=%d",
                             matchStillPresent ? 1 : 0, Root() ? 1 : 0);
                    Close();
                    if (!matchStillPresent) {
                        if (externalOnly) { Pick3::TargetBridge::Clear(); requestActive = false; }
                        Pick3::Submission::ResetMatch();
                    }
                    if (!matchStillPresent) {
                        if (nativeCacheTest) Pick3SpriteCache::ResetForMatch();
                        ResetOwnedStaticArtForMatch("open-scene-transition");
                        shownThisMatch = false;
                        matchWasPresent = false;
                    }
                    return;
                }
            }
            // Obtain the injected SelectTargetService without requiring a
            // stock target click to wake up the dependency capture.
            if (externalOnly && requestActive && gameplaySubmissionEnabled)
                Pick3::Submission::PumpDependencies();
            // Show readiness while the game injects service/model dependencies.
            if (externalOnly && requestActive && gameplaySubmissionEnabled) {
                const bool readyNow = Pick3::Submission::Ready();
                if (readyNow != wasReady) {
                    wasReady = readyNow;
                    UpdatePickStatus();
                    CC_DIAG("CustomComponents/Pick3/SUBMIT: UI_READY=%d effect=%d required=%d",
                             readyNow ? 1 : 0, activeRequest.effectEntityId, picked.Required());
                }
            }
            // PvP timeout: add the remaining unique options, then ONE request.
            if (externalOnly && requestActive && gameplaySubmissionEnabled && !picked.Sealed() &&
                !pvpTimeoutAttempted && Pick3::Submission::ShouldAutoPick()) {
                pvpTimeoutAttempted = true;
                if (Pick3::Submission::Ready()) {
                    std::vector<int> unpicked;
                    for (int i = 0; i < activeChoiceCount; ++i)
                        if (!picked.Has(i)) unpicked.push_back(i);
                    while (!picked.Complete() && !unpicked.empty()) {
                        const int pos = Pick3::Submission::RandomIndex(static_cast<int>(unpicked.size()));
                        picked.Add(unpicked[static_cast<std::size_t>(pos)]);
                        unpicked.erase(unpicked.begin() + pos);
                    }
                }
                if (Pick3::Submission::Ready() && picked.Complete() && picked.Seal()) {
                    CC_DIAG("CustomComponents/Pick3/SUBMIT: TIMER_PICK count=%d", picked.Count());
                    UpdatePickStatus();
                    Pick3::Submission::SetPopupInputCapture(false);
                    if (Pick3::Submission::Submit(activeRequest, picked.Indices(), true)) {
                        requestActive = false;
                        BeginExitAnimation();
                    } else {
                        Pick3::Submission::SetPopupInputCapture(true);
                    }
                } else {
                    LOG_ERR("CustomComponents/Pick3/SUBMIT: TIMER_NOT_READY_OR_UNFILLABLE");
                }
                return;
            }
            // Target icon presentation belongs entirely to the stock game.
            // Always stage/fetch assets before the fixed reveal; never allow
            // touches while cards are invisible or still entering.
            AnimateEntrance();
            if (!EntranceComplete()) return;
            if (isOpen && mouseDown(0)) {
                const Vec3 pos = mousePosition();
                if(inlineTooltipShowing || inlineTooltipClosing){
                    HideDescriptionTooltip();
                    return; // consume every tap until exit animation completes
                }
                if(inlineLinkApiReady){
                    HandleDescriptionTap(pos,static_cast<float>(screenWidth()),
                                              static_cast<float>(screenHeight()));
                    if(inlineTooltipShowing)return;
                }
                if (showCloseButton && Contains(closeRect, pos.x, pos.y)) { Close(); return; }
                for (int i = 0; i < activeChoiceCount; ++i) {
                    if (!Contains(choiceRects[i], pos.x, pos.y)) continue;
                    if (externalOnly && requestActive && gameplaySubmissionEnabled) {
                        if (picked.Sealed()) return; // No repeat or reentrant Process calls.
                        if (!Pick3::Submission::Ready()) {
                            if (!notReadyLogged) {
                                CC_DIAG("CustomComponents/Pick3/SUBMIT: CLICK_WAITING_FOR_SERVICE effect=%d", activeRequest.effectEntityId);
                                notReadyLogged = true;
                            }
                            return;
                        }
                        if (!picked.Add(i)) {
                            CC_DIAG("CustomComponents/Pick3/SUBMIT: IGNORED_ALREADY_SELECTED option=%d", i+1);
                            return;
                        }
                        CC_DIAG("CustomComponents/Pick3/SUBMIT: PICK_ACCUMULATED effect=%d option=%d count=%d required=%d",
                                 activeRequest.effectEntityId, i+1, picked.Count(), picked.Required());
                        UpdatePickStatus();
                        if (!picked.Complete()) return;
                        // Block all further clicks BEFORE entering the game's Process().
                        if (!picked.Seal()) return;
                        UpdatePickStatus();
                        // Keep the selected button visibly pressed for 500 ms.
                        // The game service still receives exactly one normal
                        // submission after this visual acknowledgement.
                        submitPending=true;
                        submitAt=PresentationClock()+pressFeedbackSeconds;
                        BeginChoiceTransition();
                        UpdatePickStatus();
                        return;
                    }
                    if (externalOnly && requestActive && static_cast<std::size_t>(i) < activeRequest.options.size()) {
                        const auto choice = activeRequest.options[static_cast<std::size_t>(i)];
                        CC_DIAG("CustomComponents/Pick3/STEP2: PROBE_PICK option=%d entity=%d guid=%d effect=%d (NOT SENT TO GAME)",
                                 i+1, choice.entityId, choice.cardGuid, activeRequest.effectEntityId);
                        requestActive = false;
                    } else {
                        CC_DIAG("CustomComponents/Pick3Preview: CHOSE mock option %d (UI-only)", i+1);
                    }
                    Close();
                    return;
                }
            }
            return;
        }
        // Recover any active orphan left by a scene or input transition.
        // Keep this on the bounded 30-frame path, never on every Update.
        if (frameCounter % 30 != 0 &&
            !(externalOnly && Pick3::TargetBridge::HasQueuedChoice())) return;
        if (Root() || popupRootHandle) {
            LOG_ERR("CustomComponents/Pick3Preview: inactive-state orphan; cleaning before match check");
            DestroyPopupRoot("idle-watchdog");
        }
        const bool match = IsInMatch();
        if (!match) {
            if (externalOnly) { Pick3::TargetBridge::Clear(); requestActive = false; }
            if (matchWasPresent) {
                Pick3::Submission::ResetMatch();
                Pick3GameOwnedAssetRegistry::Reset("match-ended");
                ResetOwnedStaticArtForMatch("idle-match-ended");
                if (nativeCacheTest) Pick3SpriteCache::ResetForMatch();
                shownThisMatch = false;
                CC_DIAG("CustomComponents/Pick3Preview: match ended; rearmed");
            }
            matchWasPresent = false;
            return;
        }
        matchWasPresent = true;
        if (externalOnly) {
            Pick3::PreviewRequest request;
            if (!Pick3::TargetBridge::Consume(request)) return;
            if (request.options.empty()) return;
            activeRequest = std::move(request);
            requestActive = true;
            pvpTimeoutAttempted = false;
            activeChoiceCount = static_cast<int>(activeRequest.options.size());
            wasReady = false;
            notReadyLogged = false;
            if (!picked.Begin(activeChoiceCount, activeRequest.requiredTargets)) {
                LOG_ERR("CustomComponents/Pick3/SUBMIT: INVALID_PICK_COUNT required=%d offered=%d",
                        activeRequest.requiredTargets, activeChoiceCount);
                requestActive = false;
                return;
            }
            for (int i = 0; i < 3; ++i) {
                previewCardIds[i] = i < activeChoiceCount ? activeRequest.options[static_cast<std::size_t>(i)].cardGuid : -1;
                animationCardInfo[i]=Pick3CardMetadata::CardInfo{};
                animationCardInfoGuid[i]=-1;
            }
            CC_DIAG("CustomComponents/Pick3/STEP2: PRESENT options=%d required=%d ids=%d,%d,%d", activeChoiceCount, activeRequest.requiredTargets,
                     previewCardIds[0], previewCardIds[1], previewCardIds[2]);
            if (!Open()) {
                LOG_ERR("CustomComponents/Pick3/STEP2: dynamic popup open failed");
                requestActive = false;
            }
            return;
        }
        if (shownThisMatch) return;
        // Mark before opening so missing resources cannot cause an endless
        // crash/retry loop on every frontend frame.
        shownThisMatch = true;
        if (!Open()) LOG_ERR("CustomComponents/Pick3Preview: unable to open popup");
    }

    inline bool Initialize(bool requested, bool allowClose = false,
                           int firstCardId = 114, int secondCardId = 628,
                           int thirdCardId = 684, bool asyncArt = true,
                           int color1 = 4, int color2 = 32, int color3 = 128,
                           bool probeOwners = true, bool /* legacyResolveInstances */ = false,
                           bool traceAssetTask = true, bool cacheTest = false,
                           bool requestCacheArt = false,
                           bool externalRequestsOnly = false,
                           bool submitGameplay = false,
                           int preloadMs = 700, int entranceMs = 300,
                           bool suppressStockVisuals = true,
                           int pressDelayMs = 500, int dismissalMs = 180,
                           bool enableNativeAvatarFactoryProbe = true,
                           bool enableNativeAvatarLoadTest = false,
                           bool enableNativeAvatarRenderTest = false) {
        if (!requested) return true;
        nativeAvatarProbeEnabled=enableNativeAvatarFactoryProbe;
        nativeAvatarLoadTestEnabled=enableNativeAvatarLoadTest;
        nativeAvatarRenderTestEnabled=enableNativeAvatarLoadTest && enableNativeAvatarRenderTest;
        Pick3NativeAvatarRender::Initialize(nativeAvatarRenderTestEnabled);
        Pick3NativeAvatarLoad::Initialize(enableNativeAvatarLoadTest);
        showCloseButton = allowClose;
        // Always 0. The historical preload setting is ignored for this test.
        preloadSeconds = 0.f;
        enterSeconds = static_cast<float>(std::clamp(entranceMs, 100, 1500)) / 1000.f;
        battlefieldDimAlpha = suppressStockVisuals ? .55f : .42f; // legacy dim setting; never hides target icons
        pressFeedbackSeconds=static_cast<float>(std::clamp(pressDelayMs,100,900))/1000.f;
        exitSeconds=static_cast<float>(std::clamp(dismissalMs,80,500))/1000.f;
        externalOnly = externalRequestsOnly;
        gameplaySubmissionEnabled = externalOnly && submitGameplay;
        if (gameplaySubmissionEnabled) showCloseButton = false;
        previewCardIds[0] = firstCardId;
        previewCardIds[1] = secondCardId;
        previewCardIds[2] = thirdCardId;
        nativeCacheTest = cacheTest;
        traceCardAssetsTask = traceAssetTask || cacheTest;
        if (nativeCacheTest) {
            nativeCacheTest = Pick3SpriteCache::Initialize(true, requestCacheArt);
            Pick3CardAssetsCapture::onConcreteDependencies = nativeCacheTest
                ? &Pick3SpriteCache::Capture : nullptr;
        }
        // Use one diagnostic at a time. The previous live-owner probe found no
        // inspection mediators during a match, so avoid redundant scene scans.
        probeLiveOwners = false; // retained parameter for compatibility
        if (traceCardAssetsTask) {
            const bool installed = Pick3CardAssetsCapture::Initialize();
            CC_DIAG("CustomComponents/Pick3/ASSETTASK: mode=StartCapture installed=%d liveOwnerScan=0", installed ? 1 : 0);
        }
        // In probe mode the old experimental loader is forcibly disabled.
        // It previously failed at runtime and must not pollute this diagnostic.
        // External gameplay picker may opt in to the independently rooted
        // CardFaceSpriteHolder receiver. Disabled by default in config.ini.
        // Never couple this to the experimental sprite-cache or metadata hooks.
        configuredAsyncGameArt = asyncArt && !probeLiveOwners && !traceCardAssetsTask;
        useAsyncGameArt = configuredAsyncGameArt;
        previewColors[0] = color1;
        previewColors[1] = color2;
        previewColors[2] = color3;
        auto gameObject = BNM::Class("UnityEngine", "GameObject");
        auto transform = BNM::Class("UnityEngine", "Transform");
        auto unityObject = BNM::Class("UnityEngine", "Object");
        auto resources = BNM::Class("UnityEngine", "Resources");
        auto clockClass = BNM::Class("UnityEngine", "Time");
        stockFrameClass=BNM::Class("PvZCards.UI","CollectionCardFrameView");
        liveFrameClass=BNM::Class("PvZCards.Game","CardFrameViewComponent");
        stockIconsClass=BNM::Class("PvZCards.Game","CardFrameIconSprites");
        stockBgClass=BNM::Class("PvZCards.Game","CardFrameViewFrameSprites");
        stockColorSchemeClass=BNM::Class("PvZCards.Game","ScriptableObjectColorScheme");
        if(stockColorSchemeClass.IsValid()) gameGetColorForClass=stockColorSchemeClass.GetMethod("GetVisualColorForCardColor",1).cast<Color>();
        taggedMediatorClass=BNM::Class("PvZCards.UI","TaggedTextViewMediator");
        inlineConverterClass=BNM::Class("PvZCards.Utilities","InlineTextTagConverter");
        if(liveFrameClass.IsValid()) {
            liveFrameSprites=liveFrameClass.GetField("FrameSprites").cast<Obj*>();
            liveFrameIcons=liveFrameClass.GetField("IconSprites").cast<Obj*>();
            liveFrameScheme=liveFrameClass.GetField("ColorScheme").cast<Obj*>();
        }
        if(stockFrameClass.IsValid()) {
            stockPlantCost=stockFrameClass.GetField("PlantCostSprite").cast<Obj*>();
            stockCostImage=stockFrameClass.GetField("SunCostImage").cast<Obj*>();
            stockZombieCost=stockFrameClass.GetField("ZombieCostSprite").cast<Obj*>();
            stockIcons=stockFrameClass.GetField("IconSprites").cast<Obj*>();
            stockFrames=stockFrameClass.GetField("FrameSprites").cast<Obj*>();
        }
        if(stockIconsClass.IsValid()) {
            stockAttackSprite=stockIconsClass.GetField("BasicAttackSprite").cast<Obj*>();
            stockHealthSprite=stockIconsClass.GetField("BasicHealthSprite").cast<Obj*>();
        }
        if(stockBgClass.IsValid()) stockFighterBackground=stockBgClass.GetField("FighterBackgroundSprite").cast<Obj*>();
        if(taggedMediatorClass.IsValid()) mediatorConverter=taggedMediatorClass.GetField("<InlineTextTagConverter>k__BackingField").cast<Obj*>();
        if(inlineConverterClass.IsValid()) convertTaggedText=inlineConverterClass.GetMethod("Convert",1).cast<BNM::Structures::Mono::String*>();
        nativeCardViewFactoryClass=BNM::Class("PvZCards.Game","CardViewFactory");
        nativeCardViewConfigClass=BNM::Class("PvZCards.Game","CardViewConfiguration");
        nativeAvatarViewClass=BNM::Class("PvZCards.Game","CardAvatarViewComponent");
        nativeCardViewClass=BNM::Class("PvZCards.Game","CardViewComponent");
        if(nativeCardViewFactoryClass.IsValid())
            nativeFactoryCreateAsync=nativeCardViewFactoryClass.GetMethod("CreateCardAsync",4).cast<Obj*>();
        if(nativeAvatarViewClass.IsValid()) {
            nativeAvatarEnter=nativeAvatarViewClass.GetMethod("HandlePlayedToBoard",0).cast<void>();
            nativeAvatarIdle=nativeAvatarViewClass.GetMethod("PlayIdle",0).cast<void>();
        }
        if(nativeCardViewClass.IsValid())
            nativeCardGetAvatar=nativeCardViewClass.GetMethod("GetAvatar",0).cast<Obj*>();
        LOG_INFO("CustomComponents/Pick3/NATIVE_AVATAR: API factory=%d config=%d avatar=%d enter=%d idle=%d getAvatar=%d",
            nativeCardViewFactoryClass.IsValid(),nativeCardViewConfigClass.IsValid(),
            nativeAvatarViewClass.IsValid(),nativeAvatarEnter.IsValid(),nativeAvatarIdle.IsValid(),
            nativeCardGetAvatar.IsValid());
        canvasGroupClass = BNM::Class("UnityEngine", "CanvasGroup");
        if (canvasGroupClass.IsValid())
            setCanvasGroupAlpha = canvasGroupClass.GetMethod("set_alpha", 1).cast<void>();
        setLocalScale = transform.GetMethod("set_localScale", 1).cast<void>();
        if (clockClass.IsValid())
            realtimeSinceStartup = clockClass.GetMethod("get_realtimeSinceStartup", 0).cast<float>();
        entranceApiReady = canvasGroupClass.IsValid() && setCanvasGroupAlpha.IsValid() &&
            realtimeSinceStartup.IsValid();
        CC_DIAG("CustomComponents/Pick3/UI: entrance ready=%d fixedPreload=%.2fs fade=%.2fs",
                entranceApiReady ? 1 : 0, preloadSeconds, enterSeconds);
        auto input = BNM::Class("UnityEngine", "Input");
        auto screen = BNM::Class("UnityEngine", "Screen");
        gameObjectClass = gameObject;
        rectClass = BNM::Class("UnityEngine", "RectTransform");
        imageClass = BNM::Class("UnityEngine.UI", "Image");
        if (imageClass.IsValid()) imageGetSprite=imageClass.GetMethod("get_sprite",0).cast<Obj*>();
        textClass = BNM::Class("UnityEngine.UI", "Text");
        canvasClass = BNM::Class("UnityEngine", "Canvas");
        raycasterClass = BNM::Class("UnityEngine.UI", "GraphicRaycaster");
        fontClass = BNM::Class("UnityEngine", "Font");
        localHandClass = BNM::Class("PvZCards.Game", "LocalHandViewComponent");
        typeClass = BNM::Class("System", "Type");
        auto graphic = BNM::Class("UnityEngine.UI", "Graphic");
        tmpTextClass = BNM::Class("TMPro", "TextMeshProUGUI");
        tmpFontClass = BNM::Class("TMPro", "TextMeshProFont");
        if (!gameObject.IsValid() || !transform.IsValid() || !unityObject.IsValid() ||
            !rectClass.IsValid() || !imageClass.IsValid() || !textClass.IsValid() ||
            !canvasClass.IsValid() || !raycasterClass.IsValid() || !fontClass.IsValid() ||
            !localHandClass.IsValid() || !typeClass.IsValid() || !graphic.IsValid() || !resources.IsValid() ||
            !input.IsValid() || !screen.IsValid()) {
            LOG_ERR("CustomComponents/Pick3Preview: Unity UI classes missing; disabled");
            return false;
        }
        findByName = gameObject.GetMethod("Find", 1).cast<Obj*>();
        addComponent = gameObject.GetMethod("AddComponent", 1).cast<Obj*>();
        getComponent = gameObject.GetMethod("GetComponent", 1).cast<Obj*>();
        getTransform = gameObject.GetMethod("get_transform", 0).cast<Obj*>();
        setGameObjectActive = gameObject.GetMethod("SetActive", 1).cast<void>();
        setParent = transform.GetMethod("SetParent", 2).cast<void>();
        destroyObject = unityObject.GetMethod("Destroy", {unityObject.GetCompileTimeClass()}).cast<void>();
        unityObjectAlive = unityObject.GetMethod("op_Implicit", 1).cast<bool>();
        setOwnedArtHideFlags = unityObject.GetMethod("set_hideFlags",1).cast<void>();
        findObjectOfType = unityObject.GetMethod("FindObjectOfType", 1).cast<Obj*>();
        getBuiltinResource = resources.GetMethod("GetBuiltinResource", 2).cast<Obj*>();
        getUnityObjectName = unityObject.GetMethod("get_name", 0)
                                    .cast<BNM::Structures::Mono::String*>();
        getComponentInChildren=gameObject.GetMethod("GetComponentInChildren",1).cast<Obj*>();
        spriteRendererClass=BNM::Class("UnityEngine","SpriteRenderer");
        if(spriteClass.IsValid()){
            nativeSpriteTextureRect=spriteClass.GetMethod("get_textureRect",0).cast<Rect>();
            nativeSpriteTexture=spriteClass.GetMethod("get_texture",0).cast<Obj*>();
        }
        if(spriteRendererClass.IsValid())
            spriteRendererGetSprite=spriteRendererClass.GetMethod("get_sprite",0).cast<Obj*>();
        inspectionCircleClass=BNM::Class("PvZCards.CardInspection","CardInspectionColorSpriteView");
        inspectionCostClass=BNM::Class("PvZCards.CardInspection","CardInspectionSunCostView");
        if(inspectionCircleClass.IsValid()){
            const char* names[]={"BrainySprite","GuardianSprite","HeartySprite","HungrySprite",
                "KabloomSprite","MadcapSprite","MegaGroSprite","SmartySprite","SneakySprite","SolarSprite"};
            for(int c=0;c<10;++c)inspectionClassSprites[c]=inspectionCircleClass.GetField(names[c]).cast<Obj*>();
        }
        if(inspectionCostClass.IsValid()){
            inspectionSunGO=inspectionCostClass.GetField("SunIcon").cast<Obj*>();
            inspectionBrainGO=inspectionCostClass.GetField("BrainIcon").cast<Obj*>();
        }
        if (tmpTextClass.IsValid() && tmpFontClass.IsValid()) {
            tmpGetFont = tmpTextClass.GetMethod("get_font", 0).cast<Obj*>();
            tmpGetSharedMaterial=tmpTextClass.GetMethod("get_fontSharedMaterial",0).cast<Obj*>();
            tmpSetSharedMaterial=tmpTextClass.GetMethod("set_fontSharedMaterial",1).cast<void>();
            tmpGetPreferredWidth=tmpTextClass.GetMethod("get_preferredWidth",0).cast<float>();
            tmpGetPreferredHeight=tmpTextClass.GetMethod("get_preferredHeight",0).cast<float>();
            tmpSetFont = tmpTextClass.GetMethod("set_font", 1).cast<void>();
            tmpSetText = tmpTextClass.GetMethod("set_text", 1).cast<void>();
            tmpSetWordWrap=tmpTextClass.GetMethod("set_enableWordWrapping",1).cast<void>();
            tmpSetOverflowMode=tmpTextClass.GetMethod("set_OverflowMode",1).cast<void>();
            tmpSetMinFontSize=tmpTextClass.GetMethod("set_fontSizeMin",1).cast<void>();
            tmpSetMaxFontSize=tmpTextClass.GetMethod("set_fontSizeMax",1).cast<void>();
            tmpSetRichText=tmpTextClass.GetMethod("set_richText",1).cast<void>();
            tmpSetFontSize = tmpTextClass.GetMethod("set_fontSize", 1).cast<void>();
            tmpSetAlignment = tmpTextClass.GetMethod("set_alignment", 1).cast<void>();
            tmpSetFontStyle = tmpTextClass.GetMethod("set_fontStyle", 1).cast<void>();
            tmpSetAutoSize = tmpTextClass.GetMethod("set_enableAutoSizing", 1).cast<void>();
        }
        tmpReady = tmpTextClass.IsValid() && tmpFontClass.IsValid() &&
                   getUnityObjectName.IsValid() && tmpGetFont.IsValid() &&
                   tmpSetFont.IsValid() && tmpSetText.IsValid() &&
                   tmpSetFontSize.IsValid() && tmpSetAlignment.IsValid();
        LOG_INFO("CustomComponents/Pick3/UI: CAFETERIA_SDF_API ready=%d", tmpReady?1:0);
        tmpTextInfoClass=BNM::Class("TMPro","TMP_TextInfo");
        keywordInfoProviderClass=BNM::Class("PvZCards.CardInspection","KeywordAbilityInfoProvider");
        if(tmpTextClass.IsValid())tmpGetTextInfo=tmpTextClass.GetMethod("get_textInfo",0).cast<Obj*>();
        if(tmpTextInfoClass.IsValid())tmpLinks=tmpTextInfoClass.GetField("linkInfo")
                          .cast<BNM::Structures::Mono::List<TmpLinkInfo>*>();
        auto utilities=BNM::Class("TMPro","TMP_TextUtilities");
        if(utilities.IsValid()&&tmpTextClass.IsValid()){
            auto v3=BNM::Class("UnityEngine","Vector3");
            auto cam=BNM::Class("UnityEngine","Camera");
            if(v3.IsValid()&&cam.IsValid())
                tmpFindLink=utilities.GetMethod("FindIntersectingLink",{
                    tmpTextClass.GetCompileTimeClass(),v3.GetCompileTimeClass(),cam.GetCompileTimeClass()}).cast<int>();
        }
        if(keywordInfoProviderClass.IsValid()) keywordDescription=keywordInfoProviderClass
            .GetMethod("GetKeywordDescription",3).cast<BNM::Structures::Mono::String*>();
        inlineLinkApiReady=tmpReady && tmpGetTextInfo.IsValid() && tmpLinks.IsValid() &&
                                 tmpFindLink.IsValid();
        LOG_INFO("CustomComponents/Pick3/TOOLTIP: links=%d provider=%d",inlineLinkApiReady?1:0,
                            keywordDescription.IsValid()?1:0);
        getFont = textClass.GetMethod("get_font", 0).cast<Obj*>();
        setFont = textClass.GetMethod("set_font", 1).cast<void>();
        setText = textClass.GetMethod("set_text", 1).cast<void>();
        setFontSize = textClass.GetMethod("set_fontSize", 1).cast<void>();
        setTextAnchor = textClass.GetMethod("set_alignment", 1).cast<void>();
        setFontStyle = textClass.GetMethod("set_fontStyle", 1).cast<void>();
        setBestFit = textClass.GetMethod("set_resizeTextForBestFit", 1).cast<void>();
        setGraphicColor = graphic.GetMethod("set_color", 1).cast<void>();
        setRaycastTarget = graphic.GetMethod("set_raycastTarget", 1).cast<void>();
        setAnchorMin = rectClass.GetMethod("set_anchorMin", 1).cast<void>();
        setAnchorMax = rectClass.GetMethod("set_anchorMax", 1).cast<void>();
        setPivot = rectClass.GetMethod("set_pivot", 1).cast<void>();
        setSizeDelta = rectClass.GetMethod("set_sizeDelta", 1).cast<void>();
        setAnchoredPosition = rectClass.GetMethod("set_anchoredPosition", 1).cast<void>();
        setCanvasRenderMode = canvasClass.GetMethod("set_renderMode", 1).cast<void>();
        setCanvasOverrideSorting = canvasClass.GetMethod("set_overrideSorting", 1).cast<void>();
        setCanvasSortingOrder = canvasClass.GetMethod("set_sortingOrder", 1).cast<void>();
        screenWidth = screen.GetMethod("get_width", 0).cast<int>();
        screenHeight = screen.GetMethod("get_height", 0).cast<int>();
        mouseDown = input.GetMethod("GetMouseButtonDown", 1).cast<bool>();
        mousePosition = input.GetMethod("get_mousePosition", 0).cast<Vec3>();
        textureClass = BNM::Class("UnityEngine", "Texture2D");
        spriteClass = BNM::Class("UnityEngine", "Sprite");
        byteClass = BNM::Class("System", "Byte");
        auto imageConversion = BNM::Class("UnityEngine", "ImageConversion");
        if (imageConversion.IsValid())
            loadPng = imageConversion.GetMethod("LoadImage", 2).cast<bool>();
        if (spriteClass.IsValid())
            spriteCreate = spriteClass.GetMethod("Create", 3).cast<Obj*>();
        setImageSprite = imageClass.GetMethod("set_sprite", 1).cast<void>();
        setImagePreserveAspect = imageClass.GetMethod("set_preserveAspect", 1).cast<void>();
        faceHolderClass = BNM::Class("PvZCards.CardFaceSprites", "CardFaceSpriteHolder");
        findLoadedUnityObjects = resources.GetMethod("FindObjectsOfTypeAll", 1)
                                     .cast<BNM::Structures::Mono::Array<Obj*>*>();
        if (faceHolderClass.IsValid())
            getFaceSprite = faceHolderClass.GetMethod("GetSprite", 1).cast<Obj*>();
        cardFaceLookupReady = faceHolderClass.IsValid() &&
                              findLoadedUnityObjects.IsValid() && getFaceSprite.IsValid();
        CC_DIAG("CustomComponents/Pick3Preview/DYNAMIC: loaded-holder sprite probe ready=%d ids=%d,%d,%d",
                 cardFaceLookupReady ? 1 : 0,
                 previewCardIds[0], previewCardIds[1], previewCardIds[2]);
        soundManagerClass = BNM::Class("", "SoundBankManager");
        callbackReceiverClass = BNM::Class("Sprig.Pick3", "AsyncCardFaceReceiver");
        actionOfCardFaceHolder = BNM::Class("System", "Action`1").GetGeneric({faceHolderClass.GetCompileTimeClass()});
        socialWrapperClass = BNM::Class("AntFarm", "ResourceLoader");
        socialBridgeClass = BNM::Class("PvZCards.Social", "HeroesSocialResourceLoader");
        cardAssetLoaderClass = BNM::Class("PvZCards.Common", "IdInterpreterLoader");
        auto cardViewConstants = BNM::Class("PvZCards.Game", "CardViewConstants");
        if (soundManagerClass.IsValid()) {
            getSoundManager = soundManagerClass.GetMethod("GetInstance", 0).cast<Obj*>();
            getGameResourceLoader = soundManagerClass.GetMethod("get_ResourceLoader", 0).cast<Obj*>();
        }
        if (socialWrapperClass.IsValid())
            socialWrapperLoader = socialWrapperClass.GetField("Loader");
        if (socialBridgeClass.IsValid())
            getPvzCardAssetLoader = socialBridgeClass.GetMethod("get_ResourceLoader", 0).cast<Obj*>();
        if (cardAssetLoaderClass.IsValid() && faceHolderClass.IsValid()) {
            auto generic = cardAssetLoaderClass.GetMethod("LoadResourceAsync", 2);
            if (generic.IsValid())
                loadGameResourceAsync = generic.GetGeneric({faceHolderClass.GetCompileTimeClass()}).cast<Obj*>();
        }
        if (cardViewConstants.IsValid())
            getHolderPathForColor = cardViewConstants.GetMethod("GetCardSpriteHolderAssetPathForColor", 1)
                                        .cast<BNM::Structures::Mono::String*>();
        asyncMethodsReady = getSoundManager.IsValid() && getGameResourceLoader.IsValid() &&
                            socialWrapperClass.IsValid() && socialWrapperLoader.IsValid() &&
                            socialBridgeClass.IsValid() && getPvzCardAssetLoader.IsValid() &&
                            cardAssetLoaderClass.IsValid() && loadGameResourceAsync.IsValid() &&
                            getHolderPathForColor.IsValid() && callbackReceiverClass.IsValid() &&
                            callbackReceiverClass.GetMethod("OnLoaded", 1).IsValid() &&
                            actionOfCardFaceHolder.IsValid() && getFaceSprite.IsValid();
        CC_DIAG("CustomComponents/Pick3Preview/ASYNC: ready=%d enabled=%d manager=%d loader=%d callback=%d action=%d path=%d colors=%d,%d,%d",
                 asyncMethodsReady ? 1 : 0, useAsyncGameArt ? 1 : 0,
                 getSoundManager.IsValid() ? 1 : 0, loadGameResourceAsync.IsValid() ? 1 : 0,
                 callbackReceiverClass.IsValid() ? 1 : 0, actionOfCardFaceHolder.IsValid() ? 1 : 0,
                 getHolderPathForColor.IsValid() ? 1 : 0,
                 previewColors[0], previewColors[1], previewColors[2]);
        inspectorGameObjectClass=BNM::Class("UnityEngine","GameObject");
        inspectionPrefabReceiverClass=BNM::Class("Sprig.Pick3","InspectionPrefabReceiver");
        if(inspectorGameObjectClass.IsValid()) {
            // Resolve the exact (System.Type, bool) overload, not a generic
            // variant with the same argument count.
            gameGetComponentsInChildren=inspectorGameObjectClass.GetMethod(
                "GetComponentsInChildren",{"type","includeInactive"}).cast<Obj*>();
            inspectorActionClass=BNM::Class("System","Action`1").GetGeneric({inspectorGameObjectClass.GetCompileTimeClass()});
            if(cardAssetLoaderClass.IsValid()) {
                auto generic=cardAssetLoaderClass.GetMethod("LoadResourceAsync",2);
                if(generic.IsValid())
                    prefabLoadAsync=generic.GetGeneric({inspectorGameObjectClass.GetCompileTimeClass()}).cast<Obj*>();
            }
        }
        LOG_INFO("CustomComponents/Pick3/INSPECTION_PREFAB: API loader=%d callback=%d view=%d; config opt-in only",
                 prefabLoadAsync.IsValid(),inspectionPrefabReceiverClass.IsValid(),
                 gameGetComponentsInChildren.IsValid());
        Pick3CardMetadata::Initialize();
        // ADB metadata modes (read at module startup; restart game to change).
        // 0=off; 1=task snapshots; 2=game-driven mapper SetCardData;
        // 3=both. Neither mode calls GetCardData or loads bundles manually.
        Pick3CardAssetsCapture::onCardDataSnapshot = nullptr;
        {
            int mode=0;
            const char* path="/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_metadata_source.txt";
            if(FILE* f=std::fopen(path,"rb")) {
                int value=0;
                if(std::fscanf(f,"%d",&value)==1 && value>=0 && value<=3) mode=value;
                std::fclose(f);
            } else {
                // Preserve previous snapshot selector for existing installs.
                const char* old="/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_metadata_snapshot.txt";
                if(FILE* f=std::fopen(old,"rb")) {
                    int value=0;
                    if(std::fscanf(f,"%d",&value)==1 && value==1) mode=1;
                    std::fclose(f);
                }
            }
            if(mode&1) {
                Pick3CardAssetsCapture::onCardDataSnapshot=&Pick3CardMetadata::CaptureSnapshot;
                LOG_INFO("CustomComponents/Pick3/META_SOURCE: taskHook=%d",Pick3CardAssetsCapture::Initialize()?1:0);
            }
            Pick3CardMetadata::mapperCaptureActive = (mode&2)!=0;
            if(mode&2)
                LOG_INFO("CustomComponents/Pick3/META_SOURCE: mapperHook=%d source=game-SetCardData",Pick3CardMetadata::InstallMapperCapture()?1:0);
            LOG_INFO("CustomComponents/Pick3/META_SOURCE: mode=%d task=%d mapper=%d",mode,(mode&1)?1:0,(mode&2)?1:0);
        }
        pngArtReady = textureClass.IsValid() && textureClass.GetMethod(".ctor", 2).IsValid() &&
                      byteClass.IsValid() && spriteClass.IsValid() &&
                      loadPng.IsValid() && spriteCreate.IsValid() && setImageSprite.IsValid();
        CC_DIAG("CustomComponents/Pick3Preview/ASYNC: types wrapper=%d bridge=%d pvzLoader=%d generic=%d socialField=%d getter=%d",
                 socialWrapperClass.IsValid() ? 1 : 0, socialBridgeClass.IsValid() ? 1 : 0,
                 cardAssetLoaderClass.IsValid() ? 1 : 0, loadGameResourceAsync.IsValid() ? 1 : 0,
                 socialWrapperLoader.IsValid() ? 1 : 0, getPvzCardAssetLoader.IsValid() ? 1 : 0);
        CC_DIAG("CustomComponents/Pick3Preview: screenshot portrait loader ready=%d", pngArtReady ? 1:0);
        if (!HasMethods() || !gameObject.GetMethod(".ctor", 2).IsValid() ||
            !CustomComponents::Core::FrontendTickDispatcher::RegisterOptionalPreviewTick(&Tick)) {
            LOG_ERR("CustomComponents/Pick3Preview: required Unity methods or dispatcher unavailable");
            return false;
        }
        enabled = true;
        CC_DIAG("CustomComponents/Pick3Preview: enabled mode=%s", externalOnly ? "external request probe" : "once-per-match mock");
        return true;
    }
} // namespace CustomComponents::Presentation::PickFromThreePreviewSystem
