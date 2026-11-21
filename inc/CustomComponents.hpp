#pragma once
#include "engine/AiPerformance.hpp"
#include <cstring>
#include "effects/CardMetadataEffects.hpp"
#include "effects/SwapPositionEffectDescriptor.hpp"
#include "presentation/CombatFacingPresentation.hpp"
#include "combat/DamageRedirection.hpp"
#include "combat/ExtraCombat.hpp"
#include "abilities/OverhealToHealthSystem.hpp"
#include "variables/System.hpp"
#include "level/LevelSystem.hpp"
#include "gameplay/PlayRequirement.hpp"
#include "presentation/DiscardHerald.hpp"
#include "presentation/StatIconSystem.hpp"
#include "presentation/FrameIconRegistry.hpp"
#include "effects/CreateCardInDeckFromSubset.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/SubsetSelection.hpp"

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include "log.h"


// Shared component and ability registries
#include "core/CustomComponentRegistry.hpp"
#include "core/AbilityRegistry.hpp"
#include "core/EnumSerializationSupport.hpp"
#include "core/CustomFieldBridgeSystem.hpp"

// Ability components
#include "abilities/Charge.hpp"
#include "abilities/Armor.hpp"
#include "abilities/Thrash.hpp"

// Entity-backed and discard provenance queries
#include "queries/QuerySupport.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/InDiscardPileQuery.hpp"
#include "queries/DiscardedFromQuery.hpp"
#include "queries/ListPositionQuery.hpp"
#include "queries/PickedFromThreeQuery.hpp"

// Custom triggers and shared presentation infrastructure
#include "triggers/CustomTriggerSystem.hpp"
#include "presentation/TargetingPresentationRefreshSystem.hpp"
#include "pick3/PickFromThreeCore.hpp"
#include "pick3/PickFromThreeTargetBridge.hpp"
#include "presentation/PickFromThreeDiscoverPresentation.hpp"
#include "presentation/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "triggers/CustomTrigger.hpp"
#include "triggers/CustomTriggerCarrierEffect.hpp"

// Ability runtime and gameplay systems
#include "abilities/NumericAbility.hpp"
#include "abilities/GrantableAbilityRuntime.hpp"
#include "core/ComponentSupport.hpp"
#include "abilities/AbilityDefinitions.hpp"
#include "abilities/GrantAbilityPresentation.hpp"
#include "abilities/ChargeSystem.hpp"
#include "abilities/ThrashSystem.hpp"
#include "abilities/GrantAbilitySystem.hpp"
#include "queries/TargetScopeSupport.hpp"

// Deck persistence and discard inspection systems
#include "zones/DeckMaterializer.hpp"
#include "presentation/DiscardPileViewer.hpp"

#include "effects/SubsetCreationExtensions.hpp"

#include "queries/PlacementQueries.hpp"
#include "queries/LaneQueries.hpp"
#include "queries/PrimaryTargetsQuery.hpp"
#include "queries/WasDamagedThisTurnQuery.hpp"
#include "queries/CardClassificationQueries.hpp"

#include "cost/CostModifierLifetime.hpp"

#include "abilities/BoardAbilitySystem.hpp"
#include "diagnostics/Pick3PredictionIsolationAudit.hpp"
#include "diagnostics/PvpInitialModelAudit.hpp"
#include "diagnostics/PvpDeckPrepopulatePrototype.hpp"
#include "pick3/Pick3ReleaseSafety.hpp"
#include "pick3/CreationProvenance.hpp"
#include "DrawCardFromSubsetInDeck.hpp"
#include "DiscardPileZoneEffects.hpp"

