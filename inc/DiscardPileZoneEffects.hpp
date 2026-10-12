#pragma once

//Discard to Location
#include "components/RemoveFromDiscardPileEffectDescriptor.hpp"
#include "components/ReturnFromDiscardPileToDeckEffectDescriptor.hpp"
#include "components/ReturnFromDiscardPileToLaneEffectDescriptor.hpp"
#include "components/ReturnFromDiscardPileEffectDescriptor.hpp"
#include "systems/ReturnFromDiscardPileSystem.hpp"

//Location to Discard
#include "components/DiscardFromDeckEffectDescriptor.hpp"
#include "components/DiscardFromHandEffectDescriptor.hpp"
#include "systems/DiscardFromDeckSystem.hpp"
#include "systems/DiscardFromHandSystem.hpp"
#include "systems/DiscardFromHandSystem.hpp"
#include "systems/DiscardFromHandPresentationSystem.hpp"

//Main Systems
#include "systems/DiscardPileZoneEffectsSystem.hpp"
#include "systems/DiscardedSystem.hpp"

//New Triggers
#include "components/ReturnedFromDiscardPileTrigger.hpp"
#include "components/DiscardOutsidePlayTrigger.hpp"
