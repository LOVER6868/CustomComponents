#include <bloom/bloom.h>

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>

#include "log.h"
#include "CustomComponents.hpp"
#include "DrawCardFromSubsetInDeck.hpp"
#include "DiscardPileZoneEffects.hpp"
#include "systems/HandRemovalTraceSystem.hpp"

namespace {
    // ----------------------------------------------------------------------------
    // Module startup
    // ----------------------------------------------------------------------------

    static void Init() {
        if (!BLOOM_CONFIG_BOOL("General", "Enabled", true)) {
            LOG_INFO("CustomComponents: disabled");
            return;
        }

        LOG_INFO("CustomComponents: initializing");
        // Phase 5 read-only probe. Hooking is best-effort and never gates gameplay.
        CustomComponents::Systems::HandRemovalTraceSystem::Initialize();

        if (!CustomComponents::NumericAbility::Initialize()) {
            LOG_ERR("CustomComponents: numeric value API initialization failed");
            return;
        }

        if (!CustomComponents::GrantableAbilityRuntime::Initialize()) {
            LOG_ERR("CustomComponents: GrantableAbility runtime initialization failed");
            return;
        }

        // Components register first so the framework can resolve their serialized
        // names and construct their managed instances immediately afterward.
        if (!CustomComponents::Systems::InitializeCharge()) {
            LOG_ERR("CustomComponents: Charge initialization failed");
            return;
        }

        if (!CustomComponents::Systems::InitializeThrash()) {
            LOG_ERR("CustomComponents: Thrash initialization failed");
            return;
        }

        if (!CustomComponents::Armor::Initialize()) {
            LOG_ERR("CustomComponents: Armor initialization failed");
            return;
        }

        if (!CustomComponents::Systems::DrawCardFromSubsetInDeckSystem::Initialize()) {
            LOG_ERR("CustomComponents: DrawCardFromSubsetInDeck initialization failed");
            return;
        }

        // Phase 0: materialize starting decks into persistent card entities.
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

        // Phase 1: register entity-backed deck targeting before serializer hooks.
        if (!CustomComponents::Queries::InitializeInDeckQuery()) {
            LOG_ERR("CustomComponents: InDeckQuery initialization failed");
            return;
        }

        if (!CustomComponents::Queries::InitializeInDiscardPileQuery()) {
            LOG_ERR("CustomComponents: InDiscardPileQuery initialization failed");
            return;
        }

        // Phase 4: origin-aware discard query. Register after other zone queries.
        if (!CustomComponents::Queries::InitializeDiscardedFromQuery()) {
            LOG_ERR("CustomComponents: DiscardedFromQuery initialization failed");
            return;
        }

        // Phase 2: extend stock sorted target scoping with ordered-zone list position.
        if (!CustomComponents::Systems::TargetScopeSupport::Initialize()) {
            LOG_ERR("CustomComponents: TargetScopeSupport initialization failed");
            return;
        }

        if (!CustomComponents::Systems::ComponentSupport::InstallTypeResolver()) {
            LOG_ERR("CustomComponents: type resolver initialization failed");
            return;
        }

        if (!CustomComponents::Systems::ComponentSupport::InstallComponentConstructor()) {
            LOG_ERR("CustomComponents: component constructor initialization failed");
            return;
        }


        if (!CustomComponents::AbilityDefinitions::Initialize()) {
            LOG_ERR("CustomComponents: ability registry initialization failed");
            return;
        }

        if (!CustomComponents::GrantAbility::Initialize()) {
            LOG_ERR("CustomComponents: GrantAbility initialization failed");
            return;
        }

        LOG_INFO(
            "CustomComponents: ready (%zu registered component%s, %zu registered abilities)",
            CustomComponents::CustomComponentRegistry::Count(),
            CustomComponents::CustomComponentRegistry::Count() == 1 ? "" : "s",
            CustomComponents::AbilityRegistry::Count()
        );
    }

} // namespace

BLOOM_REGISTER_PNP(Init);