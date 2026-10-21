#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "effects/RemoveFromDiscardPileEffectDescriptor.hpp"
#include "zones/DiscardFromHandSystem.hpp"
#include "zones/DiscardProvenanceSystem.hpp"
#include "presentation/DiscardFromHandPresentationSystem.hpp"
#include "effects/ReturnFromDiscardPileEffectDescriptor.hpp"
#include "effects/ReturnFromDiscardPileToDeckEffectDescriptor.hpp"
#include "effects/ReturnFromDiscardPileToLaneEffectDescriptor.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "triggers/CustomTriggerSystem.hpp"
#include "presentation/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "core/CardViewLifecycleCoordinator.hpp"
#include "core/SceneViewContext.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
#include "log.h"

namespace CustomComponents::Systems::DiscardPileZoneEffectsSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    // ---------------------------------------------------------------------
    // Shared Effect / entity fields
    // ---------------------------------------------------------------------
    inline BNM::Field<int> effectTargetId;
    inline BNM::Field<int> effectSecondaryTargetId;
    inline BNM::Field<int> effectEffectEntityId;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;

    inline BNM::Field<int> discardAssociationId;
    inline BNM::Field<int> deckAssociationId;
    inline BNM::Field<int> handAssociationId;
    inline BNM::Field<MonoList<int>*> discardCardIds;
    inline BNM::Field<MonoList<int>*> handCardIds;

    inline BNM::Method<int> getPlayerIdFromCard;
    inline BNM::Method<void> discardRemoveCard;
    inline BNM::Method<void> handAddCard;
    inline BNM::Method<void> handRemoveContainingCard;
    inline BNM::Method<void> discardAddCard;
    inline BNM::Field<Obj*> handModifierEntities;
    inline BNM::Class handModifierClass;


    // A scratch DiscardPileModifier lets systems that do not inject one still
    // use the game's canonical RemoveCard implementation. Its only dependency
    // is EntityModel, which we set from the active stock system instance.

    inline BNM::Field<Obj*> discardModifierEntities;
    inline BNM::Class discardModifierClass;

    // ---------------------------------------------------------------------
    // RemoveFromDiscardPile routing
    // ---------------------------------------------------------------------
    inline BNM::Field<int> returnDeckPosition;
    inline BNM::Field<Obj*> returnSystemModel;
    inline BNM::Field<Obj*> returnSystemDiscardModifier;
    inline BNM::Field<Obj*> returnSystemHandModifier;
    inline BNM::Field<Obj*> returnSystemHistory;

    // ---------------------------------------------------------------------
    // ReturnFromDiscardPileToDeck routing
    // ---------------------------------------------------------------------
    inline BNM::Field<int> createDeckDescriptorPosition;
    inline BNM::Field<int> createDeckRuntimeCardGuid;
    inline BNM::Field<int> createDeckRuntimePosition;
    inline BNM::Field<Obj*> createDeckSystemModel;
    inline BNM::Field<Obj*> createDeckSystemModifier;
    inline BNM::Field<Obj*> createDeckSystemHistory;
    inline BNM::Method<void> deckAddCardAt;

    // ---------------------------------------------------------------------
    // ReturnFromDiscardPileToLane routing
    // ---------------------------------------------------------------------
    inline BNM::Field<int> moveLaneRuntimeOrder;
    inline BNM::Field<Obj*> moveLaneSystemModel;
    inline BNM::Field<Obj*> moveLaneSystemModifier;
    inline BNM::Field<Obj*> moveLaneSystemHistory;
    inline BNM::Method<void> lanePlayCard;

    // Stock LaneModifier, Lane, and LaneElement dependencies used by preflight.
    inline BNM::Field<Obj*> laneModifierModel;
    inline BNM::Field<MonoList<int>*> laneElementIds;
    inline BNM::Method<int> laneGetNumFightersForPlayer;
    inline BNM::Method<int> laneGetNumTotalFighters;


    // ---------------------------------------------------------------------
    // DiscardOutsidePlayTrigger event sources
    // ---------------------------------------------------------------------
    inline BNM::Field<Obj*> moveToDiscardSystemModel;
    inline BNM::Field<int> moveToDiscardEffectPileId;

    // ---------------------------------------------------------------------
    // History / frontend bridge
    // ---------------------------------------------------------------------
    inline BNM::Method<Obj*> historyAddRecordByType;

    inline BNM::Method<void> returnedDeckRecordSetCardId;
    inline BNM::Method<void> returnedDeckRecordSetDeckId;

    inline BNM::Method<Obj*> cardIdDataCreateFromEntity;
    inline BNM::Method<void> drawRecordSetHandId;
    inline BNM::Method<void> drawRecordSetDeckId;
    inline BNM::Method<void> drawRecordSetCard;
    inline BNM::Method<void> createLaneRecordSetCard;
    inline BNM::Method<void> createLaneRecordSetOrder;
    inline BNM::Method<void> createLaneRecordSetFaceDown;
    inline BNM::Method<void> effectRecordSetDefaultIds;
    inline BNM::Method<void> effectRecordSetTargetId;

    using DescriptorToEffectFn = Obj* (*)(void*, Obj*, Obj*, const void*);
    using ProcessEffectFn = void (*)(void*, Obj*, const void*);

    inline DescriptorToEffectFn originalReturnDescriptorToEffect = nullptr;
    inline DescriptorToEffectFn originalCreateDeckDescriptorToEffect = nullptr;
    inline DescriptorToEffectFn originalMoveLaneDescriptorToEffect = nullptr;

    inline ProcessEffectFn originalReturnProcessEffect = nullptr;
    inline ProcessEffectFn originalCreateDeckProcessEffect = nullptr;
    inline ProcessEffectFn originalMoveLaneProcessEffect = nullptr;
    inline ProcessEffectFn originalMoveToDiscardProcessEffect = nullptr;

    // Transfer bypass flag for diagnostic preflight. Change only after the
    // underlying stock dependencies and full transfer path have been verified.
    // When 1, matched custom lane effects are intentionally not executed.
    inline constexpr bool kBypassCustomLaneTransferPendingVerification = true;


    template<typename T>
    #include "zones/detail/DiscardTransferHelpers.inl"
    #include "zones/detail/LegacyReturnDescriptorHook.inl"
    #include "zones/detail/DiscardReturnDescriptorRouting.inl"
    #include "zones/detail/DiscardHandZoneHelpers.inl"
    #include "zones/detail/LegacyReturnProcessHook.inl"
    #include "zones/detail/DiscardToDeck.inl"
    #include "zones/detail/DiscardToLane.inl"
    #include "zones/detail/HandToDiscard.inl"
    // =====================================================================
    // Registration / hooks
    // =====================================================================

    inline bool RegisterTypes() {
        struct Registration {
            const char* name;
            const char* baseName;
        };

        const Registration registrations[] = {
            {
                "ReturnFromDiscardPileEffectDescriptor",
                "ReturnCardToPlayersDeckEffectDescriptor"
            },
            {
                "RemoveFromDiscardPileEffectDescriptor",
                "ReturnCardToPlayersDeckEffectDescriptor"
            },
            {
                "ReturnFromDiscardPileToDeckEffectDescriptor",
                "CreateCardInDeckEffectDescriptor"
            },
            {
                "ReturnFromDiscardPileToLaneEffectDescriptor",
                "MoveCardToLanesEffectDescriptor"
            }
        };

        for (const auto& registration : registrations) {
            if (CustomComponentRegistry::FindByName(
                    "PvZCards.Engine.Components",
                    registration.name)) {
                continue;
            }

            const std::size_t registryCountBefore = CustomComponentRegistry::Count();
            BNM::Class pendingClass(
                "PvZCards.Engine.Components",
                registration.name
            );
            const bool pendingClassValid = pendingClass.IsValid();
            auto* pendingMonoType =
                pendingClassValid ? pendingClass.GetMonoType() : nullptr;

            if (!CustomComponentRegistry::Register({
                    registration.name,
                    "PvZCards.Engine.Components",
                    "EngineLib.dll",
                    registration.baseName,
                    "PvZCards.Engine.Components",
                    "EngineLib.dll"
                })) {
                LOG_ERR(
                    "CustomComponents/DiscardPileZoneEffects: failed to register %s "
                    "(registryCount=%zu max=%zu classValid=%d monoType=%p duplicate=%d base=%s)",
                    registration.name,
                    registryCountBefore,
                    CustomComponentRegistry::MaxComponents,
                    pendingClassValid ? 1 : 0,
                    pendingMonoType,
                    CustomComponentRegistry::FindByName(
                        "PvZCards.Engine.Components",
                        registration.name
                    ) ? 1 : 0,
                    registration.baseName
                );
                return false;
            }

            CC_DIAG(
                "CustomComponents/DiscardPileZoneEffects: registered %s "
                "(registryCount=%zu->%zu classValid=%d monoType=%p base=%s)",
                registration.name,
                registryCountBefore,
                CustomComponentRegistry::Count(),
                pendingClassValid ? 1 : 0,
                pendingMonoType,
                registration.baseName
            );
        }

        return true;
    }

    inline bool InstallHooks() {
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto playerUtils = BNM::Class("PvZCards.Core.Utilities", "PlayerUtils");
        auto moveToDiscardSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "MoveToDiscardPileSystem"
        );

        auto discardAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "DiscardPileAssociation"
        );
        auto deckAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "DeckAssociation"
        );
        auto handAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "HandAssociation"
        );
        auto discardPile = BNM::Class(
            "PvZCards.Engine.Components",
            "DiscardPile"
        );

        discardModifierClass = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "DiscardPileModifier"
        );
        handModifierClass = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "HandModifier"
        );
        auto deckModifier = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "DeckModifier"
        );
        auto laneModifier = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "LaneModifier"
        );

        auto returnDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "ReturnCardToPlayersDeckEffectDescriptor"
        );
        auto returnRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "ReturnCardToPlayersDeckEffect"
        );
        auto returnSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "ReturnCardToPlayersDeckSystem"
        );

        auto createDeckDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "CreateCardInDeckEffectDescriptor"
        );
        auto createDeckRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "CreateCardInDeckEffect"
        );
        auto createDeckSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "CreateCardInDeckSystem"
        );

        auto moveLaneDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "MoveCardToLanesEffectDescriptor"
        );
        auto moveLaneRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "MoveCardToLanesEffect"
        );
        auto moveLaneSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "MoveCardToLanesSystem"
        );

        auto laneComponentClass = BNM::Class(
            "PvZCards.Engine.Components", "Lane"
        );
        auto laneElementClass = BNM::Class(
            "PvZCards.Engine.Components", "LaneElement"
        );

        auto hand = BNM::Class(
            "PvZCards.Engine.Components",
            "Hand"
        );

        auto history = BNM::Class("PvZCards.Engine", "History");
        auto systemType = BNM::Class("System", "Type");

        auto returnedDeckRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "CardReturnedToPlayersDeckRecord"
        );
        auto cardIdData = BNM::Class(
            "PvZCards.Engine.Records",
            "CardIdData"
        );
        auto drawCardRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "DrawCardRecord"
        );
        auto createLaneRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "CreateCardInLaneRecord"
        );
        auto effectRecord = BNM::Class(
            "PvZCards.Engine.Records",
            "EffectRecord"
        );

        if (!effect.IsValid() || !entity.IsValid() || !model.IsValid() ||
            !playerUtils.IsValid() || !moveToDiscardSystem.IsValid() ||
            !discardAssociation.IsValid() ||
            !deckAssociation.IsValid() || !handAssociation.IsValid() ||
            !discardPile.IsValid() || !hand.IsValid() ||
            !discardModifierClass.IsValid() || !handModifierClass.IsValid() ||
            !deckModifier.IsValid() ||
            !laneModifier.IsValid() || !laneComponentClass.IsValid() ||
            !laneElementClass.IsValid() || !returnDescriptor.IsValid() ||
            !returnRuntime.IsValid() || !returnSystem.IsValid() ||
            !createDeckDescriptor.IsValid() || !createDeckRuntime.IsValid() ||
            !createDeckSystem.IsValid() || !moveLaneDescriptor.IsValid() ||
            !moveLaneRuntime.IsValid() || !moveLaneSystem.IsValid() ||
            !history.IsValid() || !systemType.IsValid() ||
            !returnedDeckRecord.IsValid() || !cardIdData.IsValid() ||
            !drawCardRecord.IsValid() || !createLaneRecord.IsValid() ||
            !effectRecord.IsValid()) {
            LOG_ERR("CustomComponents/DiscardPileZoneEffects: required classes missing");
            return false;
        }

        effectTargetId = effect.GetField("TargetId");
        effectSecondaryTargetId = effect.GetField("SecondaryTargetId");
        effectEffectEntityId = effect.GetField("EffectEntityId");

        moveToDiscardSystemModel =
            moveToDiscardSystem.GetField("<Model>k__BackingField");
        moveToDiscardEffectPileId = BNM::Class("PvZCards.Engine.Effects", "MoveToDiscardPileEffect").GetField("DiscardPileId");

        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");

        discardAssociationId = discardAssociation.GetField("DiscardPileId");
        deckAssociationId = deckAssociation.GetField("DeckEntityId");
        handAssociationId = handAssociation.GetField("HandEntityId");
        discardCardIds = discardPile.GetField("CardIds");
        handCardIds = hand.GetField("CardIds");

        getPlayerIdFromCard = playerUtils.GetMethod("GetPlayerIdFromCard", 1).cast<int>();
        discardRemoveCard = discardModifierClass.GetMethod("RemoveCard", 2).cast<void>();
        handAddCard = handModifierClass.GetMethod("AddCard", 2).cast<void>();
        handRemoveContainingCard = handModifierClass.GetMethod("RemoveCardFromContainingHand", 1).cast<void>();
        discardAddCard = discardModifierClass.GetMethod("AddCard", 2).cast<void>();
        handModifierEntities = handModifierClass.GetField("<Entities>k__BackingField");
        discardModifierEntities = discardModifierClass.GetField("<Entities>k__BackingField");

        returnDeckPosition = returnRuntime.GetField("DeckPosition");
        returnSystemModel = returnSystem.GetField("<Model>k__BackingField");
        returnSystemDiscardModifier = returnSystem.GetField("<DiscardPileModifier>k__BackingField");
        returnSystemHandModifier = returnSystem.GetField("<HandModifier>k__BackingField");
        returnSystemHistory = returnSystem.GetField("<History>k__BackingField");

        createDeckDescriptorPosition = createDeckDescriptor.GetField("DeckPosition");
        createDeckRuntimeCardGuid = createDeckRuntime.GetField("CardGuid");
        createDeckRuntimePosition = createDeckRuntime.GetField("DeckPosition");
        createDeckSystemModel = createDeckSystem.GetField("<Model>k__BackingField");
        createDeckSystemModifier = createDeckSystem.GetField("<DeckModifier>k__BackingField");
        createDeckSystemHistory = createDeckSystem.GetField("<History>k__BackingField");
        deckAddCardAt = deckModifier.GetMethod("AddCardAt", 3).cast<void>();

        moveLaneRuntimeOrder = moveLaneRuntime.GetField("OrderInLane");
        moveLaneSystemModel = moveLaneSystem.GetField("<Entities>k__BackingField");
        moveLaneSystemModifier = moveLaneSystem.GetField("<LaneModifier>k__BackingField");
        moveLaneSystemHistory = moveLaneSystem.GetField("<History>k__BackingField");
        laneModifierModel = laneModifier.GetField("<Entities>k__BackingField");
        laneElementIds = laneElementClass.GetField("LaneIds");
        laneGetNumFightersForPlayer = laneComponentClass.GetMethod("GetNumFightersForPlayer", 1).cast<int>();
        laneGetNumTotalFighters = laneComponentClass.GetMethod("GetNumTotalFighters", 0).cast<int>();
        lanePlayCard = laneModifier.GetMethod(
            "PlayCardInLane",
            {BNM::Class("System", "Int32").GetCompileTimeClass(),
             BNM::Class("System", "Int32").GetCompileTimeClass(),
             BNM::Class("System", "Int32").GetCompileTimeClass()}
        ).cast<void>();

        historyAddRecordByType = history.GetMethod(
            "AddRecord",
            {systemType.GetCompileTimeClass()}
        ).cast<Obj*>();

        returnedDeckRecordSetCardId = returnedDeckRecord.GetMethod("set_CardId", 1).cast<void>();
        returnedDeckRecordSetDeckId = returnedDeckRecord.GetMethod("set_DeckId", 1).cast<void>();

        cardIdDataCreateFromEntity = cardIdData.GetMethod("CreateFromEntity", 1).cast<Obj*>();
        drawRecordSetHandId = drawCardRecord.GetMethod("set_HandId", 1).cast<void>();
        drawRecordSetDeckId = drawCardRecord.GetMethod("set_DeckId", 1).cast<void>();
        drawRecordSetCard = drawCardRecord.GetMethod("set_Card", 1).cast<void>();
        createLaneRecordSetCard = createLaneRecord.GetMethod("set_Card", 1).cast<void>();
        createLaneRecordSetOrder = createLaneRecord.GetMethod("set_OrderInLane", 1).cast<void>();
        createLaneRecordSetFaceDown = createLaneRecord.GetMethod("set_IsFaceDown", 1).cast<void>();
        effectRecordSetDefaultIds = effectRecord.GetMethod("SetDefaultIdsFromEffect", 1).cast<void>();
        effectRecordSetTargetId = effectRecord.GetMethod("set_TargetId", 1).cast<void>();

        auto returnToEffect = returnDescriptor.GetMethod("ToEffect", 2);
        auto createDeckToEffect = createDeckDescriptor.GetMethod("ToEffect", 2);
        auto moveLaneToEffect = moveLaneDescriptor.GetMethod("ToEffect", 2);

        auto returnProcess = returnSystem.GetMethod("ProcessEffect", 1);
        auto createDeckProcess = createDeckSystem.GetMethod("ProcessEffect", 1);
        auto moveLaneProcess = moveLaneSystem.GetMethod("ProcessEffect", 1);
        auto moveToDiscardProcess =
            moveToDiscardSystem.GetMethod("ProcessEffect", 1);

        CC_DIAG(
            "CustomComponents/DiscardPileZoneEffects: DiscardOutsidePlay HAND API "
            "moveToDiscardClass=%d model=%d process=%d",
            moveToDiscardSystem.IsValid() ? 1 : 0,
            moveToDiscardSystemModel.IsValid() ? 1 : 0,
            moveToDiscardProcess.IsValid() ? 1 : 0
        );

        CC_DIAG(
            "CustomComponents/DiscardPileZoneEffects: stock return hooks owned by ReturnFromDiscardPileSystem; not re-hooking"
        );

        if (!effectTargetId.IsValid() || !effectSecondaryTargetId.IsValid() ||
            !effectEffectEntityId.IsValid() ||
            !moveToDiscardSystemModel.IsValid() ||
            !modelEntities.IsValid() || !entityComponents.IsValid() ||
            !entityId.IsValid() || !discardAssociationId.IsValid() ||
            !deckAssociationId.IsValid() || !handAssociationId.IsValid() ||
            !discardCardIds.IsValid() || !handCardIds.IsValid() ||
            !getPlayerIdFromCard.IsValid() || !discardRemoveCard.IsValid() ||
            !handAddCard.IsValid() || !discardModifierEntities.IsValid() ||
            !handModifierEntities.IsValid() || !handRemoveContainingCard.IsValid() ||
            !discardAddCard.IsValid() ||
            !returnDeckPosition.IsValid() || !returnSystemModel.IsValid() ||
            !returnSystemDiscardModifier.IsValid() ||
            !returnSystemHandModifier.IsValid() || !returnSystemHistory.IsValid() ||
            !createDeckDescriptorPosition.IsValid() ||
            !createDeckRuntimeCardGuid.IsValid() || !createDeckRuntimePosition.IsValid() ||
            !createDeckSystemModel.IsValid() || !createDeckSystemModifier.IsValid() ||
            !createDeckSystemHistory.IsValid() || !deckAddCardAt.IsValid() ||
            !moveLaneRuntimeOrder.IsValid() || !moveLaneSystemModel.IsValid() ||
            !moveLaneSystemModifier.IsValid() || !moveLaneSystemHistory.IsValid() ||
            !lanePlayCard.IsValid() || !laneModifierModel.IsValid() ||
            !laneElementIds.IsValid() || !laneGetNumFightersForPlayer.IsValid() ||
            !laneGetNumTotalFighters.IsValid() || !historyAddRecordByType.IsValid() ||
            !returnedDeckRecordSetCardId.IsValid() ||
            !returnedDeckRecordSetDeckId.IsValid() ||
            !cardIdDataCreateFromEntity.IsValid() ||
            !drawRecordSetHandId.IsValid() || !drawRecordSetDeckId.IsValid() ||
            !drawRecordSetCard.IsValid() || !createLaneRecordSetCard.IsValid() ||
            !createLaneRecordSetOrder.IsValid() || !createLaneRecordSetFaceDown.IsValid() ||
            !effectRecordSetDefaultIds.IsValid() || !effectRecordSetTargetId.IsValid() ||
            !returnToEffect.IsValid() ||
            !createDeckToEffect.IsValid() ||
            !moveLaneToEffect.IsValid() || !returnProcess.IsValid() ||
            !createDeckProcess.IsValid() || !moveLaneProcess.IsValid() ||
            !moveToDiscardProcess.IsValid()) {
            LOG_ERR("CustomComponents/DiscardPileZoneEffects: required API missing");
            return false;
        }

        BNM::BasicHook(
            createDeckToEffect,
            CreateDeckDescriptorToEffectHook,
            originalCreateDeckDescriptorToEffect
        );
        BNM::BasicHook(
            moveLaneToEffect,
            MoveLaneDescriptorToEffectHook,
            originalMoveLaneDescriptorToEffect
        );

        BNM::BasicHook(
            createDeckProcess,
            CreateDeckProcessEffectHook,
            originalCreateDeckProcessEffect
        );
        BNM::BasicHook(
            moveLaneProcess,
            MoveLaneProcessEffectHook,
            originalMoveLaneProcessEffect
        );

        BNM::BasicHook(
            moveToDiscardProcess,
            MoveToDiscardProcessEffectHook,
            originalMoveToDiscardProcessEffect
        );

        bool hookInstallationFailed = false;

#define CHECK_DISCARD_ZONE_HOOK(name, value) \
        do { \
            if (!(value)) { \
                LOG_ERR( \
                    "CustomComponents/DiscardPileZoneEffects: HOOK FAILED: %s", \
                    name \
                ); \
                hookInstallationFailed = true; \
            } \
        } while (0)

        CHECK_DISCARD_ZONE_HOOK(
            "CreateCardInDeckEffectDescriptor.ToEffect",
            originalCreateDeckDescriptorToEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "MoveCardToLanesEffectDescriptor.ToEffect",
            originalMoveLaneDescriptorToEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "CreateCardInDeckSystem.ProcessEffect",
            originalCreateDeckProcessEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "MoveCardToLanesSystem.ProcessEffect",
            originalMoveLaneProcessEffect
        );
        CHECK_DISCARD_ZONE_HOOK(
            "MoveToDiscardPileSystem.ProcessEffect",
            originalMoveToDiscardProcessEffect
        );

#undef CHECK_DISCARD_ZONE_HOOK

        if (hookInstallationFailed) {
            LOG_ERR(
                "CustomComponents/DiscardPileZoneEffects: hook installation failed; see HOOK FAILED line(s) above"
            );
            return false;
        }

        CC_DIAG(
            "CustomComponents/DiscardPileZoneEffects: ready (deck/lane returns + DiscardOutsidePlayTrigger hand path; hand/remove owned by ReturnFromDiscardPileSystem; deck-discard owned by DiscardFromDeckSystem)"
        );
        return true;
    }

    inline bool Initialize() {
        return RegisterTypes() &&
               DiscardFromHandSystem::Initialize() &&
               DiscardFromHandPresentationSystem::Initialize() &&
               InstallHooks() && moveToDiscardEffectPileId.IsValid();
    }

} // namespace CustomComponents::Systems::DiscardPileZoneEffectsSystem
