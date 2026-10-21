#pragma once

// Discard-pile removal and return effects
#include "effects/RemoveFromDiscardPileEffectDescriptor.hpp"
#include "effects/ReturnFromDiscardPileToDeckEffectDescriptor.hpp"
#include "effects/ReturnFromDiscardPileToLaneEffectDescriptor.hpp"
#include "effects/ReturnFromDiscardPileEffectDescriptor.hpp"
#include "effects/MoveCardInDeckEffectDescriptor.hpp"
#include "zones/MoveCardInDeckSystem.hpp"
#include "zones/ReturnDiscardToHandSystem.hpp"

// Hand/deck discard effects
#include "effects/DiscardFromDeckEffectDescriptor.hpp"
#include "effects/DiscardFromHandEffectDescriptor.hpp"
#include "zones/DiscardFromDeckSystem.hpp"
#include "zones/DiscardFromHandSystem.hpp"
#include "presentation/DiscardFromHandPresentationSystem.hpp"

// Discard pile transfers and origin tracking
#include "zones/DiscardPileTransferSystem.hpp"
#include "zones/DiscardProvenanceSystem.hpp"

// Discard and return trigger components
#include "triggers/ReturnedFromDiscardPileTrigger.hpp"
#include "triggers/DiscardOutsidePlayTrigger.hpp"

