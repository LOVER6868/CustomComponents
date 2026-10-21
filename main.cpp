#include <bloom/bloom.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include "log.h"
#include "CustomComponents.hpp"
#include "DrawCardFromSubsetInDeck.hpp"
#include "DiscardPileZoneEffects.hpp"

namespace {
    // ----------------------------------------------------------------------------
    // Module startup
    // ----------------------------------------------------------------------------

    // Startup isolation reads a decimal cutoff on EVERY process launch.
    // File is game-external so testing another cutoff does not require an APK rebuild.
    // -1 / missing file means the complete original startup sequence.
    static int ReadStartupCutoff() {
        constexpr const char* paths[] = {
            "/sdcard/Android/data/com.ea.gp.pvzheroes/files/customcomponents_startup_stage.txt",
            "/data/user/0/com.ea.gp.pvzheroes/files/customcomponents_startup_stage.txt"
        };
        for (const char* path : paths) {
            FILE* file = std::fopen(path, "r");
            if (!file) continue;
            char buf[32] = {};
            const bool read = std::fgets(buf, sizeof(buf), file) != nullptr;
            std::fclose(file);
            if (!read) continue;
            char* end = nullptr;
            long val = std::strtol(buf, &end, 10);
            if (end == buf || val < -1 || val > 9) {
                LOG_ERR("CustomComponents/STARTUP_DIAG: invalid cutoff in %s (valid: -1..9)", path);
                continue;
            }
            LOG_INFO("CustomComponents/STARTUP_DIAG: using %s cutoff=%ld", path, val);
            return static_cast<int>(val);
        }
        return -1;
    }

    // Read once before hooks are installed. Modes: 0 neither, 1 Charge,
    // 2 Thrash, 3 both. Missing file retains original behavior (both).
    static int ReadCombatHookMode() {
        constexpr const char* paths[] = {
            "/sdcard/Android/data/com.ea.gp.pvzheroes/files/customcomponents_combat_hooks.txt",
            "/data/user/0/com.ea.gp.pvzheroes/files/customcomponents_combat_hooks.txt"
        };
        for (const char* path : paths) {
            FILE* file = std::fopen(path, "r");
            if (!file) continue;
            char buf[32] = {};
            const bool ok = std::fgets(buf, sizeof(buf), file) != nullptr;
            std::fclose(file);
            if (!ok) continue;
            char* end = nullptr;
            const long val = std::strtol(buf, &end, 10);
            if (end == buf || val < 0 || val > 3) {
                LOG_ERR("CustomComponents/COMBAT_DIAG: invalid value in %s (valid 0..3)", path);
                continue;
            }
            LOG_INFO("CustomComponents/COMBAT_DIAG: read %s mode=%ld", path, val);
            return static_cast<int>(val);
        }
        LOG_INFO("CustomComponents/COMBAT_DIAG: selector missing, default mode=3");
        return 3;
    }

    // Pick3 stage-8 hook selector. -1/missing = existing config.ini behavior.
    // 0 = neither; 1 = candidate only; 2 = submission requested without
    // candidate (unsupported: fail closed); 3 = both.
    // Read once per process launch, prior to installing Stage-8 hooks.
    static int ReadPick3HookMode() {
        constexpr const char* paths[] = {
            "/sdcard/Android/data/com.ea.gp.pvzheroes/files/customcomponents_pick3_hooks.txt",
            "/data/user/0/com.ea.gp.pvzheroes/files/customcomponents_pick3_hooks.txt"
        };
        for (const char* path : paths) {
            FILE* file = std::fopen(path, "r");
            if (!file) continue;
            char buf[32] = {};
            const bool ok = std::fgets(buf, sizeof(buf), file) != nullptr;
            std::fclose(file);
            if (!ok) continue;
            char* end = nullptr;
            const long val = std::strtol(buf, &end, 10);
            if (end == buf || val < -1 || val > 3) {
                LOG_ERR("CustomComponents/Pick3/HOOK_MODE: invalid value in %s (valid -1..3)", path);
                continue;
            }
            LOG_INFO("CustomComponents/Pick3/HOOK_MODE: read %s mode=%ld", path, val);
            return static_cast<int>(val);
        }
        LOG_INFO("CustomComponents/Pick3/HOOK_MODE: no selector; falling back to config.ini");
        return -1;
    }

    static void Init() {
        if (!BLOOM_CONFIG_BOOL("General", "Enabled", true)) {
            LOG_INFO("CustomComponents: disabled");
            return;
        }

        const int startupCutoff = ReadStartupCutoff();
        LOG_INFO("CustomComponents/STARTUP_DIAG: BEGIN cutoff=%d (-1 means full startup)", startupCutoff);
        // The cutoff stops the *entire* remainder of initialization in original order.
        // This avoids installing later consumers without their earlier dependencies.
#define STARTUP_BOUNDARY(index, name) \
        do { \
            LOG_INFO("CustomComponents/STARTUP_DIAG: ENTER stage=%d name=%s", index, name); \
            if (startupCutoff >= 0 && startupCutoff < index) { \
                LOG_INFO("CustomComponents/STARTUP_DIAG: STOP before stage=%d name=%s", index, name); \
                return; \
            } \
        } while (false)
        STARTUP_BOUNDARY(1, "numeric-and-grantable-core");
        LOG_INFO("CustomComponents: initializing");
        if (!CustomComponents::NumericAbility::Initialize()) {
            LOG_ERR("CustomComponents: numeric value API initialization failed");
            return;
        }

        if (!CustomComponents::GrantableAbilityRuntime::Initialize()) {
            LOG_ERR("CustomComponents: GrantableAbility runtime initialization failed");
            return;
        }

        STARTUP_BOUNDARY(2, "combat-charge-thrash-armor-and-zone-systems");
        // Components register first so the framework can resolve their serialized
        // names and construct their managed instances immediately afterward.
        const int combatHookMode = ReadCombatHookMode();
        const bool chargeHooks = (combatHookMode & 1) != 0;
        const bool thrashHooks = (combatHookMode & 2) != 0;
        LOG_INFO("CustomComponents/COMBAT_DIAG: mode=%d ChargeHooks=%d ThrashHooks=%d",
                 combatHookMode, chargeHooks ? 1 : 0, thrashHooks ? 1 : 0);

        // Always register managed component schemas to keep deserialization valid.
        if (!CustomComponents::Systems::ChargeSystem::RegisterComponent()) {
            LOG_ERR("CustomComponents: Charge component registration failed");
            return;
        }
        if (chargeHooks &&
            !CustomComponents::Systems::ChargeSystem::ResolveEngineApiAndInstallHooks()) {
            LOG_ERR("CustomComponents: Charge hook installation failed");
            return;
        }
        LOG_INFO("CustomComponents/COMBAT_DIAG: Charge hooks %s", chargeHooks ? "ON" : "OFF");

        if (!CustomComponents::Systems::ThrashSystem::RegisterComponent()) {
            LOG_ERR("CustomComponents: Thrash component registration failed");
            return;
        }
        if (thrashHooks &&
            !CustomComponents::Systems::ThrashSystem::ResolveEngineApiAndInstallHooks()) {
            LOG_ERR("CustomComponents: Thrash hook installation failed");
            return;
        }
        LOG_INFO("CustomComponents/COMBAT_DIAG: Thrash hooks %s", thrashHooks ? "ON" : "OFF");

        if (!CustomComponents::Armor::Initialize()) {
            LOG_ERR("CustomComponents: Armor initialization failed");
            return;
        }

        if (!CustomComponents::Systems::DrawCardFromSubsetInDeckSystem::Initialize()) {
            LOG_ERR("CustomComponents: DrawCardFromSubsetInDeck initialization failed");
            return;
        }

        STARTUP_BOUNDARY(3, "deck-and-discard-utilities");
        // Keep main-deck entries backed by persistent card entities.
        if (!CustomComponents::Utilities::DeckMaterializer::Initialize()) {
            LOG_ERR("CustomComponents: DeckMaterializer initialization failed");
            return;
        }

        if (!CustomComponents::Utilities::DiscardPileViewer::InitializeDiscardPileViewer()) {
            LOG_ERR("CustomComponents: DiscardPileViewer initialization failed");
        }

        if (!CustomComponents::Systems::DiscardFromDeckSystem::Initialize()) {
            LOG_ERR("CustomComponents: DiscardFromDeck initialization failed");
            return;
        }

        if (!CustomComponents::Systems::DiscardFromHandSystem::Initialize()) {
            LOG_ERR("CustomComponents: DiscardFromHand initialization failed");
            return;
        }

        if (!CustomComponents::Systems::ReturnFromDiscardPileSystem::Initialize()) {
            LOG_ERR("CustomComponents: ReturnFromDiscardPile initialization failed");
            return;
        }

        // New targeted deck-order effect. It shares the existing stock
        // ReturnCardToPlayersDeckSystem hook owner (no extra native hook).
        if (!CustomComponents::Systems::MoveCardInDeckSystem::Initialize()) {
            LOG_ERR("CustomComponents: MoveCardInDeck initialization failed");
            return;
        }

        if (!CustomComponents::Systems::CustomTriggerSystem::Initialize()) {
            LOG_ERR("CustomComponents: CustomTriggerSystem initialization failed");
            return;
        }

        // Register Discarded before the custom-field bridge resolves schemas.
        // Only Hand/Deck discards receive the marker; register return-trigger cleanup hooks.
        if (!CustomComponents::Systems::DiscardedSystem::Initialize()) {
            LOG_ERR("CustomComponents: Discarded lifecycle initialization failed");
            return;
        }

        if (!CustomComponents::Systems::TargetingPresentationRefreshSystem::Initialize()) {
            LOG_ERR("CustomComponents: TargetingPresentationRefreshSystem initialization failed");
            return;
        }

        if (!CustomComponents::Systems::ZoneTriggeredAbilityPresentationSystem::Initialize()) {
            LOG_ERR("CustomComponents: ZoneTriggeredAbilityPresentationSystem initialization failed");
            return;
        }

        STARTUP_BOUNDARY(4, "custom-field-deserialization-bridge");
        // One shared bridge owns ReferenceTypeDeserializer.Deserialize and
        // CopyUtils.CopyObject post-clone preservation for registered custom fields.
        if (!CustomComponents::Systems::CustomFieldBridgeSystem::Initialize()) {
            LOG_ERR("CustomComponents: CustomFieldBridge initialization failed");
            return;
        }

        if (!CustomComponents::Systems::DiscardPileZoneEffectsSystem::Initialize()) {
            LOG_ERR("CustomComponents: DiscardPileZoneEffectsSystem initialization failed");
            return;
        }  

        STARTUP_BOUNDARY(5, "target-queries");
        // Register entity-backed deck targeting before serializer hooks.
        if (!CustomComponents::Queries::InitializeInDeckQuery()) {
            LOG_ERR("CustomComponents: InDeckQuery initialization failed");
            return;
        }

        if (!CustomComponents::Queries::InitializeInDiscardPileQuery()) {
            LOG_ERR("CustomComponents: InDiscardPileQuery initialization failed");
            return;
        }

        // Both picked-target queries are sourced from a specific Pick3 activation.
        // The GUID variant compares against CardDatabase backing cards, never live IDs.
        if (!CustomComponents::Queries::InitializePickedFromThreeQueries()) {
            LOG_ERR("CustomComponents: PickedFromThree queries initialization failed");
            return;
        }

        // Register discard-origin queries after the other zone queries.
        if (!CustomComponents::Queries::InitializeDiscardedFromQuery()) {
            LOG_ERR("CustomComponents: DiscardedFromQuery initialization failed");
            return;
        }

        // Enable ordered-zone ListPosition sorting through stock target scoping.
        if (!CustomComponents::Systems::TargetScopeSupport::Initialize()) {
            LOG_ERR("CustomComponents: TargetScopeSupport initialization failed");
            return;
        }

        STARTUP_BOUNDARY(6, "type-and-component-resolvers");
        if (!CustomComponents::Systems::ComponentSupport::InstallTypeResolver()) {
            LOG_ERR("CustomComponents: type resolver initialization failed");
            return;
        }

        if (!CustomComponents::Systems::ComponentSupport::InstallComponentConstructor()) {
            LOG_ERR("CustomComponents: component constructor initialization failed");
            return;
        }


        STARTUP_BOUNDARY(7, "ability-registration-and-serialization");
        if (!CustomComponents::AbilityDefinitions::Initialize()) {
            LOG_ERR("CustomComponents: ability registry initialization failed");
            return;
        }

        // All feature enum registrations must precede the single shared
        // DefaultDeserializer.GetValue native hook. Do not install it in a
        // feature-specific serializer; this is the generic hook owner.
        if (!CustomComponents::GrantAbilitySerialization::RegisterAbilityEnums()) {
            LOG_ERR("CustomComponents: GrantAbility enum registration failed");
            return;
        }

        // PickFromThree=3 live random, UniqueGuidTargets=4 live unique GUIDs,
        // CardDatabase=5 database definitions with automatic recipient selection.
        // Legacy PickFromThreeRandomTargets is a NAME alias to value 3.
        // Existing integer 5 JSONs require migration before installing.
        if (BLOOM_CONFIG_BOOL("PickFromThree", "LiveCandidateProbe", false)) {
            auto type = BNM::Class("PvZCards.Engine.Components", "TargetSelectionType");
            if (!type.IsValid() ||
                !CustomComponents::EnumSerializationSupport::Register(
                    type.GetMonoType(), "PickFromThree", 3) ||
                !CustomComponents::EnumSerializationSupport::Register(
                    type.GetMonoType(), "PickFromThreeRandomTargets", 3) ||
                !CustomComponents::EnumSerializationSupport::Register(
                    type.GetMonoType(), "PickFromThreeCardDatabase", 5) ||
                !CustomComponents::EnumSerializationSupport::Register(
                    type.GetMonoType(), "PickFromThreeUniqueGuidTargets", 4)) {
                LOG_ERR("CustomComponents/Pick3/ENUM: enum registration failed");
                return;
            }
            LOG_INFO("CustomComponents/Pick3/ENUM: registered PickFromThree=3 UniqueGuidTargets=4 CardDatabase=5; RandomTargets legacy alias=3");
        }
        if (!CustomComponents::EnumSerializationSupport::Initialize()) {
            LOG_ERR("CustomComponents: shared enum deserialization initialization failed");
            return;
        }

        if (!CustomComponents::GrantAbility::Initialize()) {
            LOG_ERR("CustomComponents: GrantAbility initialization failed");
            return;
        }

        STARTUP_BOUNDARY(8, "pick3-gameplay-and-submission");
        const bool liveCandidateProbe = BLOOM_CONFIG_BOOL("PickFromThree", "LiveCandidateProbe", false);
        const bool submitPick3Gameplay = liveCandidateProbe &&
            BLOOM_CONFIG_BOOL("PickFromThree", "EnableGameplaySubmission", false);
        // Stage-8 split: keep the previous defaults unless explicitly isolated.
        // CandidateBridge=false also prevents dependent submission hooks.
        const int pick3HookMode = ReadPick3HookMode();
        const bool stage8CandidateHooks = pick3HookMode < 0
            ? BLOOM_CONFIG_BOOL("Pick3Stage8Isolation", "CandidateBridge", true)
            : (pick3HookMode & 1) != 0;
        const bool stage8SubmissionHooks = pick3HookMode < 0
            ? BLOOM_CONFIG_BOOL("Pick3Stage8Isolation", "SubmissionHooks", true)
            : (pick3HookMode & 2) != 0;
        if (pick3HookMode == 2) {
            LOG_ERR("CustomComponents/Pick3/HOOK_MODE: mode=2 unsupported without CandidateBridge; no Stage-8 hooks installed");
        }
        const bool installCandidate = liveCandidateProbe && stage8CandidateHooks;
        const bool installSubmission = submitPick3Gameplay && installCandidate && stage8SubmissionHooks;
        LOG_INFO("CustomComponents/Pick3/HOOK_MODE: mode=%d configuredCandidate=%d configuredSubmission=%d",
                 pick3HookMode, stage8CandidateHooks ? 1 : 0, stage8SubmissionHooks ? 1 : 0);
        LOG_INFO("CustomComponents/Pick3/STAGE8_SPLIT: requestedCandidates=%d requestedSubmission=%d candidate=%d submission=%d",
                 liveCandidateProbe ? 1 : 0, submitPick3Gameplay ? 1 : 0,
                 installCandidate ? 1 : 0, installSubmission ? 1 : 0);
        if (installCandidate && !CustomComponents::Pick3::TargetBridge::Initialize(
                true, installSubmission)) {
            LOG_ERR("CustomComponents/Pick3/STEP2: live candidate diagnostic unavailable");
        }
        if (installSubmission && !CustomComponents::Pick3::Submission::Initialize(
                true,
                BLOOM_CONFIG_BOOL("PickFromThree", "SubmitInPVE", true),
                false /* PvP submission disabled pending end-to-end verification */,
                BLOOM_CONFIG_INT("PickFromThree", "PvpTimeoutMarginMs", 600),
                BLOOM_CONFIG_BOOL("PickFromThree", "ResolveServiceFromInjectionBinder", true),
                BLOOM_CONFIG_BOOL("PickFromThree", "SyncLocalCompletionSignal", true))) {
            LOG_ERR("CustomComponents/Pick3/SUBMIT: preflight failed; preserving probe-only behavior");
        }
        STARTUP_BOUNDARY(9, "pick3-assets-ui-and-avatar-hooks");
        if (liveCandidateProbe || BLOOM_CONFIG_BOOL("PickFromThreePreview", "Enabled", false)) {
            using namespace CustomComponents::Presentation;
            Pick3LocalAssetBundles::SetProbeMode(
                BLOOM_CONFIG_INT("Pick3OwnedUnityFS","ProbeMode",1));
            Pick3LocalAssetBundles::SetAssetIndexLimit(
                BLOOM_CONFIG_INT("Pick3OwnedUnityFS","MaxAssetIndex",6));
            Pick3LocalAssetBundles::SetTextureFallbackEnabled(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS","AllowTextureToSprite",false));
            Pick3LocalAssetBundles::SetLoadNativeColorScheme(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS","LoadNativeColorScheme",false));
            Pick3LocalAssetBundles::SetEnableSplitRim(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS","EnableSplitRim",true));
            Pick3LocalAssetBundles::SetEnableButtonSlicing(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS","EnableButtonSlicing",false));
            Pick3LocalAssetBundles::SetEnabled(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS","Enabled",false));
            // Game-owned stock Sprite references. No LoadFromFile and no new
            // Unity Sprite creation; disable the separate Pick3AssetTracer
            // during this test because it hooks these same stock methods.
            // Bloom may still contain the previous temporary standalone
            // tracer. Never install two native hooks on the same stock methods.
            const bool tracerActive = ::Bloom::FindModule("Pick3AssetTracer") &&
                ::Bloom::ConfigBool("Pick3AssetTracer", "module", "enabled", true);
            if (tracerActive)
                LOG_ERR("CustomComponents/Pick3/STOCK_REGISTRY: TRACER_CONFLICT; disable Pick3AssetTracer to enable registry hooks");
            Pick3GameOwnedAssetRegistry::Initialize(
                !tracerActive && BLOOM_CONFIG_BOOL("Pick3GameOwnedAssets", "Enabled", true),
                BLOOM_CONFIG_BOOL("Pick3GameOwnedAssets", "UseInPicker", true),
                false);
            Pick3CardMetadata::useTraitIcons = BLOOM_CONFIG_BOOL("Pick3GameOwnedAssets", "UseTraitIcons", true);
            PickFromThreePreviewSystem::useStockClassRimTint = BLOOM_CONFIG_BOOL("Pick3GameOwnedAssets", "UseClassRims", true);
            PickFromThreePreviewSystem::ConfigureStartupPrewarm(
                false /* prewarm disabled: never create managed Unity assets at match start */,
                BLOOM_CONFIG_INT("Pick3OwnedUnityFS", "PrewarmStartFrames", 180),
                BLOOM_CONFIG_INT("Pick3OwnedUnityFS", "PrewarmStepFrames", 12),
                BLOOM_CONFIG_INT("Pick3OwnedUnityFS", "PrewarmMatchFrames", 180),
                false /* no match-start sprite creation */);
            // Private, fixed native-art implementation parameters. These are not
            // release-facing beta toggles. Failures fall back to static art.
            constexpr int animationRequestedStage=4;
            constexpr bool animationUnsafeOptIn=true;
            PickFromThreePreviewSystem::ConfigureReferenceResolution(
                BLOOM_CONFIG_INT("Pick3Screen","ReferenceWidth",720),
                BLOOM_CONFIG_INT("Pick3Screen","ReferenceHeight",1280));
            PickFromThreePreviewSystem::ConfigureLayout(
                BLOOM_CONFIG_INT("Pick3RewardLayout","TopYPercent",21),
                BLOOM_CONFIG_INT("Pick3RewardLayout","LowerYPercent",-24),
                BLOOM_CONFIG_INT("Pick3RewardLayout","SideGapPercent",24),
                BLOOM_CONFIG_INT("Pick3RewardLayout","PortraitYPercent",28),
                BLOOM_CONFIG_INT("Pick3RewardLayout","PortraitSizePercent",104),
                BLOOM_CONFIG_INT("Pick3RewardLayout","NameBelowStatsPercent",80),
                BLOOM_CONFIG_INT("Pick3RewardLayout","PlateBelowNamePercent",28),
                BLOOM_CONFIG_INT("Pick3RewardLayout","ButtonBelowPlatePercent",31),
                BLOOM_CONFIG_INT("Pick3RewardLayout","PlateWidthPercent",136),
                BLOOM_CONFIG_INT("Pick3RewardLayout","PlateHeightPercent",39),
                BLOOM_CONFIG_INT("Pick3RewardLayout","ButtonWidthPercent",98),
                BLOOM_CONFIG_INT("Pick3RewardLayout","ButtonHeightPercent",120),
                BLOOM_CONFIG_INT("Pick3RewardLayout","NameSizePercent",140),
                BLOOM_CONFIG_INT("Pick3RewardLayout","StatSizePercent",126),
                BLOOM_CONFIG_INT("Pick3RewardLayout","ButtonTextSizePercent",108),
                BLOOM_CONFIG_INT("Pick3RewardLayout","ButtonTextYOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3RewardLayout","BadgeSizePercent",24),
                BLOOM_CONFIG_INT("Pick3RewardLayout","StatSpacingPercent",13),
                BLOOM_CONFIG_INT("Pick3RewardLayout","StatsBelowPortraitPercent",40));
            PickFromThreePreviewSystem::ConfigureStatText(
                BLOOM_CONFIG_INT("Pick3StatText","AttackSizePercent",110),
                BLOOM_CONFIG_INT("Pick3StatText","HealthSizePercent",110),
                BLOOM_CONFIG_INT("Pick3StatText","CostSizePercent",110),
                BLOOM_CONFIG_INT("Pick3StatText","DescriptionSizePercent",120),
                BLOOM_CONFIG_INT("Pick3StatText","AttackXOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","AttackYOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","HealthXOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","HealthYOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","CostXOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","CostYOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","DescriptionXOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","DescriptionYOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3StatText","AttackWidthPercent",90),
                BLOOM_CONFIG_INT("Pick3StatText","HealthWidthPercent",90),
                BLOOM_CONFIG_INT("Pick3StatText","CostWidthPercent",90),
                BLOOM_CONFIG_INT("Pick3StatText","DescriptionWidthPercent",93),
                BLOOM_CONFIG_INT("Pick3StatText","DescriptionHeightPercent",87));
            PickFromThreePreviewSystem::ConfigureNativeArtwork(
                BLOOM_CONFIG_INT("Pick3NativeArtwork","ViewSizePercent",250),
                BLOOM_CONFIG_INT("Pick3NativeArtwork","EnvironmentViewSizePercent",250),
                BLOOM_CONFIG_INT("Pick3NativeArtwork","EnvironmentYOffsetPercent",0),
                BLOOM_CONFIG_INT("Pick3NativeArtwork","ViewHeightPercent",100));
            PickFromThreePreviewSystem::ConfigureDescription(
                BLOOM_CONFIG_BOOL("Pick3Description","AutoSizePlate",true),
                BLOOM_CONFIG_INT("Pick3Description","MinWidthPercent",75),
                BLOOM_CONFIG_INT("Pick3Description","MaxWidthPercent",155),
                BLOOM_CONFIG_INT("Pick3Description","MinHeightPercent",25),
                BLOOM_CONFIG_INT("Pick3Description","MaxHeightPercent",65),
                BLOOM_CONFIG_INT("Pick3Description","PaddingPercent",8),
                BLOOM_CONFIG_BOOL("Pick3Description","AutoFitText",true));
            PickFromThreePreviewSystem::ConfigureInspectionCircle(
                BLOOM_CONFIG_BOOL("Pick3NativeClassArt","CropTallSprites",true),
                BLOOM_CONFIG_INT("Pick3NativeClassArt","CircleCenterXPercent",50),
                BLOOM_CONFIG_INT("Pick3NativeClassArt","CircleCenterYPercent",68),
                BLOOM_CONFIG_INT("Pick3NativeClassArt","CircleWidthPercent",33),
                BLOOM_CONFIG_BOOL("Pick3NativeClassArt","ShowReferenceFallback",true),
                BLOOM_CONFIG_INT("Pick3NativeClassArt","GrassSizePercent",94));
            PickFromThreePreviewSystem::ConfigureInspectionPrefabLoading(
                BLOOM_CONFIG_BOOL("Pick3NativeClassArt","RequestNativePrefabExperimental",false));
            PickFromThreePreviewSystem::ConfigureTooltipStyle(
                BLOOM_CONFIG_INT("Pick3InlineTooltip","FontSizePercent",100),
                BLOOM_CONFIG_INT("Pick3InlineTooltip","MaxWidthPercent",64),
                BLOOM_CONFIG_INT("Pick3InlineTooltip","MaxHeightPercent",17),
                BLOOM_CONFIG_INT("Pick3InlineTooltip","PopupMilliseconds",120),
                BLOOM_CONFIG_INT("Pick3InlineTooltip","PopoutMilliseconds",90));
            PickFromThreePreviewSystem::ConfigureTargetingText(
                BLOOM_CONFIG_BOOL("Pick3TargetingText","BehindCards",false),
                BLOOM_CONFIG_INT("Pick3TargetingText","PositionX",0),
                BLOOM_CONFIG_INT("Pick3TargetingText","PositionY",520),
                BLOOM_CONFIG_INT("Pick3TargetingText","FontSizePercent",125));
            Pick3NativeAvatarRender::ConfigureFraming(
                BLOOM_CONFIG_INT("Pick3CardViewCamera","ZoomPercent",100),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","PaddingPercent",122),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","MinOrthoHundredths",20),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","MaxOrthoHundredths",1200),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","OffsetXPercent",0),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","OffsetYPercent",0),
                BLOOM_CONFIG_BOOL("Pick3CardViewCamera","UseDominantRenderer",true),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","EnvironmentZoomPercent",100),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","EnvironmentCameraYOffsetPercent",0),
                BLOOM_CONFIG_BOOL("Pick3CardViewCamera","UseStableInspectionFraming",true),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","FighterBaselineOrthoHundredths",700),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","EnvironmentBaselineOrthoHundredths",900),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","OtherBaselineOrthoHundredths",750),
                BLOOM_CONFIG_INT("Pick3CardViewCamera","SafetyOverscanPercent",120));
            // Three separate 512x512 RTs, paced independently at up to 60 Hz.
            // This removes the old combined 75-fps bottleneck; actual FPS still
            // depends on UnityMain cadence and device CPU/GPU headroom.
            Pick3NativeAvatarRender::ConfigureRenderCadence(
                BLOOM_CONFIG_INT("Pick3Performance","RenderEveryNTicks",1));
            Pick3NativeAvatarRender::ConfigureRenderBudgetFps(
                BLOOM_CONFIG_INT("Pick3Performance","GlobalRenderFpsCap",190));
            Pick3NativeAvatarRender::ConfigurePerCardFps(
                BLOOM_CONFIG_INT("Pick3Performance","FighterFps",60),
                BLOOM_CONFIG_INT("Pick3Performance","EnvironmentFps",60),
                BLOOM_CONFIG_INT("Pick3Performance","OtherFps",60));
            // Recovery baseline: use stock-tested native renderer framing.
            // Camera placement tuning is temporarily disabled until stable.
            constexpr bool isolateNativeAvatars=false;
            // Keep the native APIs initialized, but the runtime diagnostic mode
            // controls whether they are ever exercised when a popup opens.
            const bool nativeAnimationAllowed=animationRequestedStage>=2 && animationUnsafeOptIn &&
                !isolateNativeAvatars && BLOOM_CONFIG_BOOL("Pick3Performance","EnableNativeArtwork",true);
            const int animationEffectiveStage=nativeAnimationAllowed ? animationRequestedStage : 1;
            PickFromThreePreviewSystem::ConfigureAvatarExperiment(animationEffectiveStage,1,
                BLOOM_CONFIG_INT("Pick3Performance","LoadSpacingFrames",0));
            LOG_INFO("CustomComponents/Pick3/AVATAR: native=%d stage=%d fpsCap=%d",nativeAnimationAllowed?1:0,animationEffectiveStage,
                Pick3NativeAvatarRender::renderBudgetFps);
            if (!CustomComponents::Presentation::PickFromThreePreviewSystem::Initialize(
                    true, BLOOM_CONFIG_BOOL("PickFromThreePreview", "ShowCloseButton", false),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "CardId1", 114),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "CardId2", 628),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "CardId3", 684),
                    BLOOM_CONFIG_BOOL("PickFromThreePreview", "AsyncGameArt", true),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "CardColor1", 4),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "CardColor2", 32),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "CardColor3", 128),
                    false,
                    BLOOM_CONFIG_BOOL("PickFromThreePreview", "ResolveInstances", false),
                    true, // capture game-owned services for real portraits
                    BLOOM_CONFIG_BOOL("PickFromThreePreview", "NativeSpriteCacheTest", false),
                    BLOOM_CONFIG_BOOL("PickFromThreePreview", "NativeSpriteCacheRequestArt", false),
                    liveCandidateProbe,
                    submitPick3Gameplay && CustomComponents::Pick3::Submission::enabled,
                    BLOOM_CONFIG_INT("PickFromThreePreview", "PreloadMilliseconds", 700),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "EntranceMilliseconds", 300),
                    BLOOM_CONFIG_BOOL("PickFromThreePreview", "MaskStockTargetUI", true),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "PressFeedbackMilliseconds", 500),
                    BLOOM_CONFIG_INT("PickFromThreePreview", "ExitAnimationMilliseconds", 180),
                    false, // never resurrect legacy factory probe from stale config
                    nativeAnimationAllowed,
                    nativeAnimationAllowed && animationEffectiveStage>=3)) {
                LOG_ERR("CustomComponents: optional PickFromThree UI test unavailable");
            }
            PickFromThreePreviewSystem::ConfigureBattlefieldDimming(
                BLOOM_CONFIG_INT("Pick3Overlay","BattlefieldDarkeningPercent",42));
            PickFromThreePreviewSystem::ConfigureChoiceMotion(
                BLOOM_CONFIG_INT("Pick3ChoiceMotion","GlideMilliseconds",400),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion","GlideExponentHundredths",350),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion","CenterHoldMilliseconds",70),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion","FadeMilliseconds",180),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion","OtherCardsDismissMilliseconds",240),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion","LowerCardArcPercent",5),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion","PuffPercent",31));
        }

        LOG_INFO("CustomComponents/STARTUP_DIAG: COMPLETED full startup");
#undef STARTUP_BOUNDARY
        LOG_INFO(
            "CustomComponents: ready (%zu registered component%s, %zu registered abilities)",
            CustomComponents::CustomComponentRegistry::Count(),
            CustomComponents::CustomComponentRegistry::Count() == 1 ? "" : "s",
            CustomComponents::AbilityRegistry::Count()
        );
    }

} // namespace

BLOOM_REGISTER_PNP(Init);