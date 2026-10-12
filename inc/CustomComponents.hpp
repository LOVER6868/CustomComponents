#pragma once

// Core registries
#include "core/CustomComponentRegistry.hpp"
#include "core/AbilityRegistry.hpp"
#include "systems/CustomFieldBridgeSystem.hpp"

// Traits/Ability Components
#include "components/Charge.hpp"
#include "components/Armor.hpp"
#include "components/Thrash.hpp"

//Queries
#include "queries/QuerySupport.hpp"
#include "queries/InDeckQuery.hpp"
#include "queries/InDiscardPileQuery.hpp"
#include "queries/DiscardedFromQuery.hpp"

// Custom trigger framework
#include "systems/CustomTriggerSystem.hpp"
#include "systems/TargetingPresentationRefreshSystem.hpp"
#include "systems/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "components/CustomTrigger.hpp"
#include "components/CustomTriggerCarrierEffect.hpp"

// Runtime systems
#include "systems/NumericAbility.hpp"
#include "systems/GrantableAbilityRuntime.hpp"
#include "systems/ComponentSupport.hpp"
#include "systems/AbilityDefinitions.hpp"
#include "systems/GrantAbilityPresentation.hpp"
#include "systems/ChargeSystem.hpp"
#include "systems/ThrashSystem.hpp"
#include "systems/GrantAbilitySystem.hpp"
#include "systems/TargetScopeSupport.hpp"

//Extra Gameplay Functions
#include "utilities/DeckMaterializer.hpp"
#include "utilities/DiscardPileViewer.hpp"