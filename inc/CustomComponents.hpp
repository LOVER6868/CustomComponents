#pragma once

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
#include "queries/PickedFromThreeQuery.hpp"

// Custom triggers and shared presentation infrastructure
#include "triggers/CustomTriggerSystem.hpp"
#include "presentation/TargetingPresentationRefreshSystem.hpp"
#include "pick3/PickFromThreeCore.hpp"
#include "pick3/PickFromThreeTargetBridge.hpp"
#include "presentation/PickFromThreePreviewSystem.hpp"
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