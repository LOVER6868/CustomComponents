#pragma once
#include <bloom/bloom.h>
#include "../CustomComponents.hpp"
#include "queries/IgnoreCheatTagQuery.hpp"
#include "queries/SuperpowerDeckQueries.hpp"
#include "zones/PreDiscardSystem.hpp"
#include "zones/CreateSuperpowerSubset.hpp"
#include "abilities/TriggeredAbilityGrants.hpp"
#include "presentation/SuperpowerDeckPresentation.hpp"
namespace CustomComponents::Startup {
    // ----------------------------------------------------------------------------
    // Module startup
    // ----------------------------------------------------------------------------

    // Release startup always installs the complete feature set.
    static int ReadStartupCutoff() { return -1; }
    static int ReadCombatHookMode() { return 3; }
    static int ReadPick3HookMode() { return -1; }

    inline void Initialize() {
        Core::PresentationInvalidation::invalidateFrames=[](){Systems::ZoneTriggeredAbilityPresentationSystem::frameStatsDirty=true;};
        Core::PresentationInvalidation::invalidateCard=[](int id){Systems::ZoneTriggeredAbilityPresentationSystem::InvalidateCard(id);};
        if (!BLOOM_CONFIG_BOOL("General", "Enabled", true)) {
            LOG_INFO("CustomComponents: disabled");
            return;
        }

        CustomComponents::Diagnostics::HistoryTrace::enabled=BLOOM_CONFIG_BOOL("CustomHistory", "Trace", false);
        CustomComponents::Core::Performance::enabled=BLOOM_CONFIG_BOOL("Performance", "Enabled", false);
        if(!CustomComponents::Engine::AiPerformance::Initialize())
            LOG_WARN("CustomComponents/Performance: AI candidate timing unavailable");
        if(!CustomComponents::Engine::EntityAccess::Initialize()) {
            LOG_ERR("CustomComponents: shared entity metadata initialization failed");
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
        if(!CustomComponents::Engine::PlayTransport::Initialize()){LOG_ERR("CustomComponents: play transport initialization failed");return;}
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

        if(!CustomComponents::Effects::SubsetCreationExtensions::Initialize()) {
            LOG_ERR("CustomComponents: subset creation extensions initialization failed");return;
        }

        // Off by default. Opt in via config.ini to observe stock prediction copying.
        // This hook only logs; it never spawns, moves or materializes cards.
        if (BLOOM_CONFIG_BOOL("Pick3Diagnostics", "PredictionCloneAudit", false)) {
            if (!CustomComponents::Diagnostics::PredictionIsolationAudit::Install()) {
                LOG_ERR("CustomComponents/Pick3/CLONE_AUDIT: installation failed, leaving stock copy unchanged");
            }
        }

        STARTUP_BOUNDARY(3, "deck-and-discard-utilities");
        // Keep main-deck entries backed by persistent card entities.
        if (!CustomComponents::Utilities::DeckMaterializer::Initialize()) {
            LOG_ERR("CustomComponents: DeckMaterializer initialization failed");
            return;
        }

        // Optional single checkpoint at the exact PvP InitialGameState handoff.
        // NEVER materializes or modifies cards. Disabled by default.
        if (BLOOM_CONFIG_BOOL("PvpDeckDiagnostics", "InitSnapshot", false)) {
            if (!CustomComponents::Diagnostics::PvpInitialModelAudit::Install()) {
                LOG_ERR("CustomComponents/PVP_INIT_AUDIT: install failed; continuing without diagnostic");
            }
        }

        // One user-facing feature switch; staged one/two-card trials are not
        // part of the release configuration. Keep the checked native backend.
        const bool customDecks=BLOOM_CONFIG_BOOL("Multiplayer","CustomDeckInitialization",true);
        if(!CustomComponents::Diagnostics::PvpDeckPrepopulatePrototype::Install(
            customDecks?2:0,customDecks,customDecks,customDecks,false,false,1,customDecks,
            BLOOM_CONFIG("Multiplayer","ExpectedDeckFingerprint"),
            BLOOM_CONFIG_BOOL("Multiplayer","MaterializeInsertedCards",true)))
            LOG_ERR("CustomComponents/PVP_PREPOP: installation failed; stock PvP remains in effect");

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
        if(!CustomComponents::Engine::SubsetSelection::Initialize()) {LOG_ERR("CustomComponents: subset selection metadata initialization failed");return;}
        // One shared bridge owns ReferenceTypeDeserializer.Deserialize and
        // CopyUtils.CopyObject post-clone preservation for registered custom fields.
        if (!CustomComponents::Systems::CustomFieldBridgeSystem::Initialize()) {
            LOG_ERR("CustomComponents: CustomFieldBridge initialization failed");
            return;
        }

        if (!CustomComponents::Systems::ZoneTransferSystem::Initialize()) {
            LOG_ERR("CustomComponents: ZoneTransferSystem initialization failed");
            return;
        }
        CustomComponents::Pick3::TargetBridge::fighterFindValid=
            CustomComponents::Systems::ZoneTransferSystem::Targeting::FighterUiTargets;
        CustomComponents::Pick3::TargetBridge::fighterShouldSecondary=
            CustomComponents::Systems::ZoneTransferSystem::Targeting::FighterShouldChooseSecondary;

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
        if(!CustomComponents::Queries::InitializeListPositionQuery()) {
            LOG_ERR("CustomComponents: ListPositionQuery initialization failed");return;
        }

        if(!CustomComponents::Queries::InitializePlacementQueries()) {LOG_ERR("CustomComponents: placement queries initialization failed");return;}
        if(!CustomComponents::Queries::InitializeLaneQueries()) {LOG_ERR("CustomComponents: lane queries initialization failed");return;}
        if(!CustomComponents::Abilities::TriggeredGrants::Initialize()) {LOG_ERR("CustomComponents: triggered ability grants initialization failed");return;}
        if(!CustomComponents::Queries::CardPools::Initialize()) {LOG_ERR("CustomComponents: card pool query initialization failed");return;}
        if(!CustomComponents::Queries::CheatSelection::Initialize()) {LOG_ERR("CustomComponents: cheat subset query initialization failed");return;}
        if(!CustomComponents::Queries::InitializePrimaryTargetsQuery()) {LOG_ERR("CustomComponents: primary targets query initialization failed");return;}
        if(!CustomComponents::Queries::InitializeWasDamagedThisTurnQuery()) {LOG_ERR("CustomComponents: damage-history query initialization failed");return;}
        if(!CustomComponents::Queries::InitializeCardClassificationQueries()) {LOG_ERR("CustomComponents: card set/rarity query initialization failed");return;}

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

        if(!CustomComponents::Abilities::Withering::InitializeSystem() || !CustomComponents::Abilities::RandomKeyword::Initialize() || !CustomComponents::Abilities::RandomKeyword::Register() ||
           !CustomComponents::Abilities::BoardAbility::Initialize()) {
            LOG_ERR("CustomComponents: selected ability initialization failed");return;
        }
        if(!CustomComponents::Effects::CreateDeckSubset::Initialize()) {LOG_ERR("CustomComponents: subset deck creation initialization failed");return;}
        if(!CustomComponents::Presentation::SuperpowerDeck::Initialize()) {LOG_ERR("CustomComponents: superpower deck presentation initialization failed");return;}
        if(!CustomComponents::Zones::CreateSuperpowerSubset::Initialize()) {LOG_ERR("CustomComponents: superpower subset creation initialization failed");return;}
        if(!CustomComponents::Zones::PreDiscard::Initialize()){LOG_ERR("CustomComponents: pre-discard initialization failed");return;}
        if(!CustomComponents::Systems::InitializeStatIcons() || !CustomComponents::Presentation::FrameIcons::Initialize()) {LOG_ERR("CustomComponents: icon service initialization failed");return;}
        if(!CustomComponents::Presentation::Herald::Initialize()) {LOG_ERR("CustomComponents: discarded herald initialization failed");return;}
        // All feature enum registrations must precede the single shared
        // DefaultDeserializer.GetValue native hook. Do not install it in a
        // feature-specific serializer; this is the generic hook owner.
        if (!CustomComponents::Effects::AbilityRemoval::Initialize() || !CustomComponents::GrantAbilitySerialization::RegisterAbilityEnums()) {
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
        if(!CustomComponents::Level::Initialize()){LOG_ERR("CustomComponents: Level initialization failed");return;}
        if(!CustomComponents::PlayRequirements::Initialize()){LOG_ERR("CustomComponents: PlayRequirement initialization failed");return;}
        if(!CustomComponents::Variables::Initialize()){LOG_ERR("CustomComponents: Variables initialization failed");return;}
        // The Pick3 listener bridge subscribes to QueuedTriggerValues::afterCreate;
        // initializing it at stage 3 used to fail before that observer existed.
        if(!CustomComponents::Systems::CustomTriggerSystem::InitializePick3AfterVariables())
            LOG_ERR("CustomComponents/Pick3: transaction pipeline unavailable after Variables initialization");
        if(!CustomComponents::Effects::CardMetadata::Initialize()){LOG_ERR("CustomComponents: CardMetadata effect initialization failed");return;}
        if(!CustomComponents::Abilities::OverhealToHealth::InitializeSystem()){LOG_ERR("CustomComponents: OverhealToHealth hook initialization failed");return;}
        if(!CustomComponents::Abilities::CombatModifiers::InitializeHooks(BLOOM_CONFIG_BOOL("NonCombatTruestrike","UseTruestrikeIcons",false))){LOG_ERR("CustomComponents: damage/buff modifier hook initialization failed");return;}
        if(!CustomComponents::CostModifierLifetime::Initialize()){LOG_ERR("CustomComponents: cost modifier lifetime hook initialization failed");return;}
        if(!CustomComponents::Effects::SwapPosition::Initialize()){LOG_ERR("CustomComponents: SwapPosition effect initialization failed");return;}
        CustomComponents::Presentation::CombatFacing::enabled=BLOOM_CONFIG_BOOL("CombatPresentation","RotateFighters",true);
        if(!CustomComponents::Presentation::CombatFacing::Initialize())LOG_ERR("CustomComponents: combat-facing presentation initialization failed");
        if(!CustomComponents::Combat::DamageRedirection::Initialize())LOG_ERR("CustomComponents: damage-redirection support initialization failed");
        if(!CustomComponents::ExtraCombat::Initialize()){LOG_ERR("CustomComponents: ExtraCombat registration failed");return;}
        CustomComponents::Level::Selection::inputBlocked=[](){return CustomComponents::Pick3::Submission::overlayCapturesInput;};
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
        // A requested submission is not safe until its candidate hooks have
        // actually initialized. Requested flags alone are not a dependency.
        const bool candidateReady = installCandidate &&
            CustomComponents::Pick3::TargetBridge::Initialize(true, installSubmission) &&
            CustomComponents::Pick3::TargetBridge::enabled;
        if (installCandidate && !candidateReady)
            LOG_ERR("CustomComponents/Pick3/STEP2: candidate bridge unavailable; submission disabled");
        if (installSubmission && !candidateReady)
            LOG_ERR("CustomComponents/Pick3/SUBMIT: dependency missing; submission hooks not installed");
        // Honor the actual user switch. A second, hidden experimental opt-in
        // previously disabled remote submission even with SubmitInPVP=true.
        // Do not equate an enabled transport with network-side capability:
        // remote CardDatabase still requires GUID replication.
        const bool pvpRequested = BLOOM_CONFIG_BOOL("PickFromThree", "SubmitInPVP", true);
        CustomComponents::Pick3::Submission::RegisterPick3TransferPairCheck(&CustomComponents::Systems::ZoneTransferSystem::Pick3CanTransferPhysicalPair);
        CustomComponents::Pick3::Submission::RegisterPick3PhysicalPairCheck(
            &CustomComponents::Systems::ZoneTransferSystem::Pick3CanRevivePhysicalPair);
        if (installSubmission && candidateReady && !CustomComponents::Pick3::Submission::Initialize(
            true,
            BLOOM_CONFIG_BOOL("PickFromThree", "SubmitInPVE", true),
            pvpRequested,
            BLOOM_CONFIG_INT("PickFromThree", "PvpTimeoutMarginMs", 600),
            BLOOM_CONFIG_BOOL("PickFromThree", "ResolveServiceFromInjectionBinder", true),
            BLOOM_CONFIG_BOOL("PickFromThree", "SyncLocalCompletionSignal", true),
            BLOOM_CONFIG_BOOL("Pick3PrePlayTrace", "Enabled", false))) {
            LOG_ERR("CustomComponents/Pick3/SUBMIT: preflight failed; preserving probe-only behavior");
        }
        // Independently reversible lifecycle fixes. These switches permit
        // field testing without rewriting the source or touching the FSM.
        CustomComponents::Pick3::TargetButtonTrace::enableGuardedReconcile =
            BLOOM_CONFIG_BOOL("Pick3LifecycleRepair", "GuardedStockButtonRecovery", true);
        // Preserve stock targeting/button recovery; the legacy popup and its
        // interruption/holder fallback settings no longer exist.
        CustomComponents::Pick3::TargetButtonTrace::enableGuardedReconcile =
            BLOOM_CONFIG_BOOL("Pick3LifecycleRepair", "GuardedStockButtonRecovery", true);
        // Binding is gameplay behavior. The trace switch controls optional
        // diagnostics, never whether produced cards receive their PF3 identity.
        const bool creationTraceRequested = BLOOM_CONFIG_BOOL("Pick3CreationTrace", "Enabled", false);
        if (installSubmission && candidateReady) {
            const bool provenanceReady=CustomComponents::Pick3::CreationProvenance::Initialize(creationTraceRequested);
            if(!provenanceReady)LOG_ERR("CustomComponents/Pick3/CREATION_BINDING: unavailable");
            if (!CustomComponents::Pick3::ExactRouting::Initialize(!CustomComponents::Pick3::CreationProvenance::originalFindCard))
                LOG_ERR("CustomComponents/Pick3/PATCH2_ROUTING: finder unavailable");
        }
        STARTUP_BOUNDARY(9, "pick3-discover-ui");
        if (candidateReady && BLOOM_CONFIG_BOOL("PickFromThreePresentation", "Enabled", true)) {
            using namespace CustomComponents::Presentation;
            PickFromThreeDiscoverPresentation::ConfigureTargetingText(
                BLOOM_CONFIG_BOOL("Pick3TargetingText", "BehindCards", false),
                BLOOM_CONFIG_INT("Pick3TargetingText", "PositionX", 0),
                BLOOM_CONFIG_INT("Pick3TargetingText", "PositionY", 520),
                BLOOM_CONFIG_INT("Pick3TargetingText", "FontSizePercent", 80),
                BLOOM_CONFIG_INT("Pick3TargetingText", "MaxCharactersPerLine", 32));
            CustomComponents::Pick3::Submission::preferStockCommandHandoff =
                BLOOM_CONFIG_BOOL("PickFromThreePresentation", "StockCommandHandoff", true);
            const bool presentationReady=PickFromThreeDiscoverPresentation::Initialize(
                true,
                BLOOM_CONFIG_BOOL("PickFromThreePresentation", "UseStockTargeting", false),
                BLOOM_CONFIG_BOOL("PickFromThreePresentation", "MaskStockTargetUI", true));
            LOG_INFO("CustomComponents/Pick3/RECEIVER1727: original board-drop and state-6 menu enabled; pre-Play return UI removed");
            if(!presentationReady) {
                LOG_ERR("CustomComponents/Pick3: Discover-style presentation unavailable; gameplay bridge preserved");
            }
        }

        LOG_INFO("CustomComponents/BUILD: patch=6.7.0 built=%s %s", __DATE__, __TIME__);
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
