#include <bloom/bloom.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include "log.h"
#include "CustomComponents.hpp"
#include "pick3/Pick3ReleaseSafety.hpp"
#include "DrawCardFromSubsetInDeck.hpp"
#include "DiscardPileZoneEffects.hpp"

namespace {
    // ----------------------------------------------------------------------------
    // Module startup
    // ----------------------------------------------------------------------------

    // Startup isolation reads a decimal cutoff on EVERY process launch.
    // File is game-external so testing another cutoff does not require an APK rebuild.
    // -1 / missing file means the complete original startup sequence.
    static int ReadStartupCutoff() { return -1; }

    // Read once before hooks are installed. Modes: 0 neither, 1 Charge,
    // 2 Thrash, 3 both. Missing file retains original behavior (both).
    static int ReadCombatHookMode() { return 3; }

    // Pick3 stage-8 hook selector. -1/missing = existing config.ini behavior.
    // 0 = neither; 1 = candidate only; 2 = submission requested without
    // candidate (unsupported: fail closed); 3 = both.
    // Read once per process launch, prior to installing Stage-8 hooks.
    static int ReadPick3HookMode() { return 3; }

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
                    type.GetMonoType(), "PickFromThreeUniqueGuidTargets", 4) ||
                !CustomComponents::EnumSerializationSupport::Register(
                    type.GetMonoType(), "PickFromThreeUniqueCardIds", 4)) {
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
        // No PvP tracer hooks. The stock RemoteSelectTargetService is used directly
        // by the same submission adapter as PvE; only existing game-input hooks remain.
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
        // Honor the actual user switch. A second, hidden experimental opt-in
        // previously disabled remote submission even with SubmitInPVP=true.
        // Do not equate an enabled transport with network-side capability:
        // remote CardDatabase still requires GUID replication.
        const bool pvpRequested = BLOOM_CONFIG_BOOL("PickFromThree", "SubmitInPVP", true);
        if (installSubmission && !CustomComponents::Pick3::Submission::Initialize(
            true,
            BLOOM_CONFIG_BOOL("PickFromThree", "SubmitInPVE", true),
            pvpRequested,
            BLOOM_CONFIG_INT("PickFromThree", "PvpTimeoutMarginMs", 600),
            BLOOM_CONFIG_BOOL("PickFromThree", "ResolveServiceFromInjectionBinder", true),
            BLOOM_CONFIG_BOOL("PickFromThree", "SyncLocalCompletionSignal", true))) {
            LOG_ERR("CustomComponents/Pick3/SUBMIT: preflight failed; preserving probe-only behavior");
        }
        STARTUP_BOUNDARY(9, "pick3-assets-ui-and-avatar-hooks");
        if (liveCandidateProbe || BLOOM_CONFIG_BOOL("PickFromThreePreview", "Enabled", false)) {
            using namespace CustomComponents::Presentation;
            Pick3LocalAssetBundles::SetProbeMode(
                BLOOM_CONFIG_INT("Pick3OwnedUnityFS", "ProbeMode", 1));
            Pick3LocalAssetBundles::SetAssetIndexLimit(
                BLOOM_CONFIG_INT("Pick3OwnedUnityFS", "MaxAssetIndex", 6));
            Pick3LocalAssetBundles::SetTextureFallbackEnabled(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS", "AllowTextureToSprite", false));
            Pick3LocalAssetBundles::SetLoadNativeColorScheme(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS", "LoadNativeColorScheme", false));
            Pick3LocalAssetBundles::SetEnableSplitRim(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS", "EnableSplitRim", true));
            Pick3LocalAssetBundles::SetEnableButtonSlicing(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS", "EnableButtonSlicing", false));
            Pick3LocalAssetBundles::SetEnabled(
                BLOOM_CONFIG_BOOL("Pick3OwnedUnityFS", "Enabled", false));
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
            // Native art uses the same user setting as the factory pipeline.
            // Callback receiver fields are declared in BNM metadata to match
            // their native layout; keep the artwork factory functional.
            constexpr int animationRequestedStage = 4;
            PickFromThreePreviewSystem::ConfigureReferenceResolution(
                BLOOM_CONFIG_INT("Pick3Screen", "ReferenceWidth", 720),
                BLOOM_CONFIG_INT("Pick3Screen", "ReferenceHeight", 1280));
            PickFromThreePreviewSystem::ConfigureLayout(
                BLOOM_CONFIG_INT("Pick3Layout", "TopYPercent", 21),
                BLOOM_CONFIG_INT("Pick3Layout", "LowerYPercent", -24),
                BLOOM_CONFIG_INT("Pick3Layout", "SideGapPercent", 24),
                BLOOM_CONFIG_INT("Pick3Layout", "PortraitYPercent", 28),
                BLOOM_CONFIG_INT("Pick3Layout", "PortraitSizePercent", 104),
                BLOOM_CONFIG_INT("Pick3Layout", "NameBelowStatsPercent", 80),
                BLOOM_CONFIG_INT("Pick3Layout", "PlateBelowNamePercent", 28),
                BLOOM_CONFIG_INT("Pick3Layout", "ButtonBelowPlatePercent", 31),
                BLOOM_CONFIG_INT("Pick3Layout", "PlateWidthPercent", 136),
                BLOOM_CONFIG_INT("Pick3Layout", "PlateHeightPercent", 39),
                BLOOM_CONFIG_INT("Pick3Layout", "ButtonWidthPercent", 98),
                BLOOM_CONFIG_INT("Pick3Layout", "ButtonHeightPercent", 120),
                BLOOM_CONFIG_INT("Pick3Text", "NameSizePercent", 140),
                BLOOM_CONFIG_INT("Pick3Layout", "StatSizePercent", 126),
                BLOOM_CONFIG_INT("Pick3Text", "ButtonTextSizePercent", 108),
                BLOOM_CONFIG_INT("Pick3Layout", "ButtonTextYOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Layout", "BadgeSizePercent", 24),
                BLOOM_CONFIG_INT("Pick3Layout", "StatSpacingPercent", 13),
                BLOOM_CONFIG_INT("Pick3Layout", "StatsBelowPortraitPercent", 40));
            PickFromThreePreviewSystem::ConfigureBadgeYOffsets(
                BLOOM_CONFIG_INT("Pick3Layout", "AttackBadgeYOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Layout", "HealthBadgeYOffsetPercent", 0));
            PickFromThreePreviewSystem::ConfigureStatText(
                BLOOM_CONFIG_INT("Pick3Text", "AttackSizePercent", 110),
                BLOOM_CONFIG_INT("Pick3Text", "HealthSizePercent", 110),
                BLOOM_CONFIG_INT("Pick3Text", "CostSizePercent", 110),
                BLOOM_CONFIG_INT("Pick3Description", "FontSizePercent", 120),
                BLOOM_CONFIG_INT("Pick3Text", "AttackXOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "AttackYOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "HealthXOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "HealthYOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "CostXOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "CostYOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "DescriptionXOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "DescriptionYOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3Text", "AttackWidthPercent", 90),
                BLOOM_CONFIG_INT("Pick3Text", "HealthWidthPercent", 90),
                BLOOM_CONFIG_INT("Pick3Text", "CostWidthPercent", 90),
                BLOOM_CONFIG_INT("Pick3Text", "DescriptionWidthPercent", 93),
                BLOOM_CONFIG_INT("Pick3Text", "DescriptionHeightPercent", 87));
            PickFromThreePreviewSystem::ConfigureNativeArtwork(
                BLOOM_CONFIG_INT("Pick3NativeArtwork", "ViewSizePercent", 250),
                BLOOM_CONFIG_INT("Pick3NativeArtwork", "EnvironmentViewSizePercent", 250),
                BLOOM_CONFIG_INT("Pick3NativeArtwork", "EnvironmentYOffsetPercent", 0),
                BLOOM_CONFIG_INT("Pick3NativeArtwork", "ViewHeightPercent", 100));
            PickFromThreePreviewSystem::ConfigureDescription(
                BLOOM_CONFIG_BOOL("Pick3Description", "AutoSizePlate", true),
                BLOOM_CONFIG_INT("Pick3Description", "MinWidthPercent", 75),
                BLOOM_CONFIG_INT("Pick3Description", "MaxWidthPercent", 155),
                BLOOM_CONFIG_INT("Pick3Description", "MinHeightPercent", 25),
                BLOOM_CONFIG_INT("Pick3Description", "MaxHeightPercent", 65),
                BLOOM_CONFIG_INT("Pick3Description", "PaddingPercent", 8),
                BLOOM_CONFIG_BOOL("Pick3Description", "AutoFitText", true));
            PickFromThreePreviewSystem::ConfigureInspectionCircle(
                BLOOM_CONFIG_BOOL("Pick3NativeClassArt", "CropTallSprites", true),
                BLOOM_CONFIG_INT("Pick3NativeClassArt", "CircleCenterXPercent", 50),
                BLOOM_CONFIG_INT("Pick3NativeClassArt", "CircleCenterYPercent", 68),
                BLOOM_CONFIG_INT("Pick3NativeClassArt", "CircleWidthPercent", 33),
                BLOOM_CONFIG_BOOL("Pick3NativeClassArt", "ShowReferenceFallback", true),
                BLOOM_CONFIG_INT("Pick3NativeClassArt", "GrassSizePercent", 94));
            PickFromThreePreviewSystem::ConfigureInspectionPrefabLoading(
                BLOOM_CONFIG_BOOL("Pick3NativeClassArt", "RequestNativePrefabExperimental", false));
            PickFromThreePreviewSystem::ConfigureTooltipStyle(
                BLOOM_CONFIG_INT("Pick3InlineTooltip", "FontSizePercent", 100),
                BLOOM_CONFIG_INT("Pick3InlineTooltip", "MaxWidthPercent", 64),
                BLOOM_CONFIG_INT("Pick3InlineTooltip", "MaxHeightPercent", 17),
                BLOOM_CONFIG_INT("Pick3InlineTooltip", "PopupMilliseconds", 120),
                BLOOM_CONFIG_INT("Pick3InlineTooltip", "PopoutMilliseconds", 90));
            PickFromThreePreviewSystem::ConfigureTargetingText(
                BLOOM_CONFIG_BOOL("Pick3TargetingText", "BehindCards", false),
                BLOOM_CONFIG_INT("Pick3TargetingText", "PositionX", 0),
                BLOOM_CONFIG_INT("Pick3TargetingText", "PositionY", 520),
                BLOOM_CONFIG_INT("Pick3TargetingText", "FontSizePercent", 125));
            Pick3NativeAvatarRender::ConfigureFraming(
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "ZoomPercent", 100),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "PaddingPercent", 122),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "MinOrthoHundredths", 20),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "MaxOrthoHundredths", 1200),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "OffsetXPercent", 0),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "OffsetYPercent", 0),
                BLOOM_CONFIG_BOOL("Pick3CardViewCamera", "UseDominantRenderer", true),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "EnvironmentZoomPercent", 100),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "EnvironmentCameraYOffsetPercent", 0),
                BLOOM_CONFIG_BOOL("Pick3CardViewCamera", "UseStableInspectionFraming", true),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "FighterBaselineOrthoHundredths", 700),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "EnvironmentBaselineOrthoHundredths", 900),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "OtherBaselineOrthoHundredths", 750),
                BLOOM_CONFIG_INT("Pick3CardViewCamera", "SafetyOverscanPercent", 120));
            // Experimental renderer only. Keep the budget conservative when
            // explicitly enabled during isolated device verification.
            Pick3NativeAvatarRender::ConfigureRenderCadence(
                BLOOM_CONFIG_INT("Pick3Performance", "RenderEveryNTicks", 1));
            Pick3NativeAvatarRender::ConfigureRenderBudgetFps(
                BLOOM_CONFIG_INT("Pick3Performance", "GlobalRenderFpsCap", 75));
            Pick3NativeAvatarRender::ConfigurePerCardFps(
                BLOOM_CONFIG_INT("Pick3Performance", "FighterFps", 30),
                BLOOM_CONFIG_INT("Pick3Performance", "EnvironmentFps", 24),
                BLOOM_CONFIG_INT("Pick3Performance", "OtherFps", 24));
            // Recovery baseline: use stock-tested native renderer framing.
            // Camera placement tuning is temporarily disabled until stable.
            constexpr bool isolateNativeAvatars = false;
            // Keep the native APIs initialized, but the runtime diagnostic mode
            // controls whether they are ever exercised when a popup opens.
            const bool nativeAnimationAllowed = !isolateNativeAvatars &&
                BLOOM_CONFIG_BOOL("Pick3Performance", "EnableNativeArtwork", true);
            const int animationEffectiveStage = nativeAnimationAllowed ? animationRequestedStage : 1;
            PickFromThreePreviewSystem::ConfigureAvatarExperiment(animationEffectiveStage, 1,
                BLOOM_CONFIG_INT("Pick3Performance", "LoadSpacingFrames", 2));
            LOG_INFO("CustomComponents/Pick3/AVATAR: native=%d stage=%d fpsCap=%d", nativeAnimationAllowed ? 1 : 0, animationEffectiveStage,
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
                nativeAnimationAllowed && animationEffectiveStage >= 3)) {
                LOG_ERR("CustomComponents: optional PickFromThree UI test unavailable");
            }
            PickFromThreePreviewSystem::ConfigureBattlefieldDimming(
                BLOOM_CONFIG_INT("Pick3Overlay", "BattlefieldDarkeningPercent", 42));
            PickFromThreePreviewSystem::ConfigureChoiceMotion(
                BLOOM_CONFIG_INT("Pick3ChoiceMotion", "GlideMilliseconds", 400),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion", "GlideExponentHundredths", 350),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion", "CenterHoldMilliseconds", 70),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion", "FadeMilliseconds", 180),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion", "OtherCardsDismissMilliseconds", 240),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion", "LowerCardArcPercent", 5),
                BLOOM_CONFIG_INT("Pick3ChoiceMotion", "PuffPercent", 31));
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

// Cross-module callback API. Bloom modules are linked into the same sprig.so.
// No second SetCardData hook is installed by SuperpowerEditor.
extern "C" bool Bloom_RegisterSetCardDataObserver(
    void (*observer)(void*, int, void*, const void*)) {
    return CustomComponents::Presentation::Pick3CardMetadata::RegisterSharedObserver(observer);
}

BLOOM_REGISTER_PNP(Init);