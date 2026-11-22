#pragma once

// Central include point for CustomComponents startup features.
// Startup sequencing and Bloom config remain in startup/Module.hpp.

// C/C++ standard library and shared logging
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "log.h"

// Core registries, hooks and shared runtime
#include "core/CustomComponentRegistry.hpp"
#include "core/AbilityRegistry.hpp"
#include "core/EnumSerializationSupport.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ComponentSupport.hpp"

// Engine access and gameplay integration
#include "engine/AiPerformance.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/SubsetSelection.hpp"

// Diagnostics (loaded before ability systems that use tracing)
#include "diagnostics/Pick3PredictionIsolationAudit.hpp"
#include "diagnostics/PvpInitialModelAudit.hpp"
#include "diagnostics/PvpDeckPrepopulatePrototype.hpp"

// Abilities and ability systems
#include "abilities/OverhealToHealthSystem.hpp"
#include "abilities/Charge.hpp"
#include "abilities/Armor.hpp"
#include "abilities/Thrash.hpp"
#include "abilities/NumericAbility.hpp"
#include "abilities/GrantableAbilityRuntime.hpp"
#include "abilities/AbilityDefinitions.hpp"
#include "abilities/GrantAbilityPresentation.hpp"
#include "abilities/ChargeSystem.hpp"
#include "abilities/ThrashSystem.hpp"
#include "abilities/GrantAbilitySystem.hpp"
#include "abilities/BoardAbilitySystem.hpp"
#include "abilities/TriggeredAbilityGrants.hpp"

// Combat systems
#include "combat/DamageRedirection.hpp"
#include "combat/ExtraCombat.hpp"

// Costs
#include "cost/CostModifierLifetime.hpp"

// Effect descriptors and handlers
#include "effects/CardMetadataEffects.hpp"
#include "effects/SwapPositionEffectDescriptor.hpp"
#include "effects/CreateCardInDeckFromSubset.hpp"
#include "effects/SubsetCreationExtensions.hpp"
#include "effects/TransformCardIntoExistingCard.hpp"

// Gameplay validation
#include "gameplay/PlayRequirement.hpp"

// Level system
#include "level/LevelSystem.hpp"

// Queries and target selection
#include "queries/QuerySupport.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/InDiscardPileQuery.hpp"
#include "queries/DiscardedFromQuery.hpp"
#include "queries/ListPositionQuery.hpp"
#include "queries/PickedFromThreeQuery.hpp"
#include "queries/TargetScopeSupport.hpp"
#include "queries/PlacementQueries.hpp"
#include "queries/LaneQueries.hpp"
#include "queries/PrimaryTargetsQuery.hpp"
#include "queries/WasDamagedThisTurnQuery.hpp"
#include "queries/CardClassificationQueries.hpp"
#include "queries/IgnoreCheatTagQuery.hpp"
#include "queries/SuperpowerDeckQueries.hpp"

// Pick-from-three
#include "pick3/PickFromThreeCore.hpp"
#include "pick3/PickFromThreeTargetBridge.hpp"
#include "pick3/Pick3ReleaseSafety.hpp"
#include "pick3/CreationProvenance.hpp"

// Presentation and UI
#include "presentation/CombatFacingPresentation.hpp"
#include "presentation/DiscardHerald.hpp"
#include "presentation/StatIconSystem.hpp"
#include "presentation/FrameIconRegistry.hpp"
#include "presentation/TargetingPresentationRefreshSystem.hpp"
#include "presentation/PickFromThreeDiscoverPresentation.hpp"
#include "presentation/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "presentation/DiscardPileViewer.hpp"
#include "presentation/SuperpowerDeckPresentation.hpp"

// Trigger systems
#include "triggers/CustomTriggerSystem.hpp"
#include "triggers/CustomTrigger.hpp"
#include "triggers/CustomTriggerCarrierEffect.hpp"

// Variables
#include "variables/System.hpp"

// Zone systems
#include "zones/DeckMaterializer.hpp"
#include "zones/PreDiscardSystem.hpp"
#include "zones/CreateSuperpowerSubset.hpp"

// Root-level feature adapters
#include "DrawCardFromSubsetInDeck.hpp"
#include "DiscardPileZoneEffects.hpp"
