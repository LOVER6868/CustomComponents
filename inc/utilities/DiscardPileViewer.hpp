#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "log.h"

namespace CustomComponents::Utilities::DiscardPileViewer {

    using Obj = BNM::IL2CPP::Il2CppObject;
    using MonoObjList = BNM::Structures::Mono::List<Obj*>;
    using MonoIntList = BNM::Structures::Mono::List<int>;

    // -----------------------------------------------------------------------------
    // Stock paths used by this utility
    //
    // Activation:
    //   PvZCards.Game.InspectTappedCardEntityCommand::Execute()
    //       RVA 0x1F7A410
    //
    // The command already owns:
    //   EntityGestureData
    //   ShowCardInspectionOverlaySignal
    //   CardInspectionUtilities
    //
    // CardInspectionUtilities already owns the live EntityModel.
    //
    // UI:
    //   PvZCards.CardInspection.ShowCardInspectionOverlaySignal
    //   PvZCards.CardInspection.CardInspectionOverlayConfig
    //   PvZCards.CardInspection.InspectionItem
    //
    // No Unity GameObjects are created here.  We only create the stock managed
    // config/items and dispatch the stock signal.  ScreenLoader/game prefabs remain
    // responsible for constructing and destroying the actual overlay.
    // -----------------------------------------------------------------------------

    using InspectTappedExecuteFn =
        void (*)(void* self, const void* method);
    using PickTargetFn =
        int (*)(void* self, Obj* overlappedEntities, const void* method);

    inline InspectTappedExecuteFn originalInspectTappedExecute = nullptr;
    inline PickTargetFn originalPickTarget = nullptr;

    // InspectTappedCardEntityCommand
    inline BNM::Field<Obj*> commandGestureData;
    inline BNM::Field<Obj*> commandShowInspectionSignal;
    inline BNM::Field<Obj*> commandCardInspectionModel;

    // HandleBoardTappedCommand / overlap chain
    inline BNM::Field<Obj*> boardTapOverlapFinder;
    inline BNM::Field<Obj*> overlapFinderViewMapper;
    inline BNM::Field<Obj*> viewMapperEntityModel;

    // EntityGestureData
    inline BNM::Method<int> gestureGetTappedEntityId;

    // CardInspectionUtilities
    inline BNM::Field<Obj*> inspectionUtilitiesEntityModel;

    // Entity / model
    inline BNM::Field<MonoObjList*> entityComponents;
    inline BNM::Method<Obj*> modelGetEntityById;

    // Player -> discard association
    inline BNM::Field<int> discardAssociationPileId;
    inline BNM::IL2CPP::Il2CppClass* discardAssociationClass = nullptr;

    // Discard pile
    inline BNM::Field<MonoIntList*> discardPileCardIds;
    inline BNM::IL2CPP::Il2CppClass* discardPileClass = nullptr;

    // Card
    inline BNM::Field<int> cardGuid;
    inline BNM::IL2CPP::Il2CppClass* cardClass = nullptr;

    // InspectionItem
    inline BNM::Method<void> inspectionItemCtor;
    inline BNM::Method<void> inspectionItemSetEntityId;

    // CardInspectionOverlayConfig
    inline BNM::Method<void> inspectionConfigCtor;
    inline BNM::Field<MonoObjList*> inspectionConfigItems;
    inline BNM::Field<int> inspectionConfigInitialIndex;
    inline BNM::Field<bool> inspectionConfigWrapping;
    inline BNM::Field<bool> inspectionConfigLockUnownedHeroes;
    inline BNM::Field<bool> inspectionConfigAllowHeroPurchase;
    inline BNM::Field<bool> inspectionConfigShowCrafting;

    // Signal<CardInspectionOverlayConfig>
    inline BNM::Method<void> showInspectionDispatch;

    inline BNM::Class inspectionItemClass;
    inline BNM::Class inspectionConfigClass;

    template <typename T>
    inline T* ListAt(BNM::Structures::Mono::List<T*>* list, int index) {
        if (!list || !list->items || index < 0 || index >= list->size)
            return nullptr;
        return list->items->m_Items[index];
    }

    inline int IntAt(MonoIntList* list, int index) {
        if (!list || !list->items || index < 0 || index >= list->size)
            return -1;
        return list->items->m_Items[index];
    }

    inline Obj* FindExactComponent(
        Obj* entity,
        BNM::IL2CPP::Il2CppClass* wantedClass
    ) {
        if (!entity || !wantedClass || !entityComponents.IsValid())
            return nullptr;

        MonoObjList* components = entityComponents[entity].Get();
        if (!components || !components->items)
            return nullptr;

        for (int i = 0; i < components->size; ++i) {
            Obj* component = ListAt(components, i);
            if (component && component->klass == wantedClass)
                return component;
        }

        return nullptr;
    }

    inline int GetCardGuid(Obj* entity) {
        Obj* card = FindExactComponent(entity, cardClass);
        if (!card)
            return -1;

        return cardGuid[card].Get();
    }

    inline bool BuildAndShowDiscardPile(
        Obj* model,
        Obj* showSignal,
        Obj* tappedPlayer
    ) {
        if (!model || !showSignal || !tappedPlayer)
            return false;

        Obj* discardAssociation =
            FindExactComponent(tappedPlayer, discardAssociationClass);

        // This is also our hero/player test.  A normal card/lane/etc. does not own
        // the player's DiscardPileAssociation, so stock card-inspection behavior is
        // left untouched for every non-player tap.
        if (!discardAssociation)
            return false;

        const int discardPileId =
            discardAssociationPileId[discardAssociation].Get();

        if (discardPileId <= 0) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer: tapped player has invalid DiscardPileId=%d",
                discardPileId
            );
            return true;
        }

        Obj* discardPileEntity = modelGetEntityById[model](discardPileId);
        if (!discardPileEntity) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer: DiscardPile entity %d was not found",
                discardPileId
            );
            return true;
        }

        Obj* discardPile =
            FindExactComponent(discardPileEntity, discardPileClass);

        if (!discardPile) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer: entity %d has no DiscardPile component",
                discardPileId
            );
            return true;
        }

        MonoIntList* ids = discardPileCardIds[discardPile].Get();
        if (!ids || !ids->items || ids->size <= 0) {
            LOG_INFO(
                "CustomComponents/DiscardPileViewer: hero tapped; DiscardPileId=%d is empty",
                discardPileId
            );
            return true;
        }

        // Create a real managed List<InspectionItem>.  This is a stock managed data
        // structure, not a Unity GameObject.
        MonoObjList* items = inspectionItemClass.NewList<Obj*>();
        if (!items) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer: failed to create List<InspectionItem>"
            );
            return true;
        }

        int skipped = 0;

        // Preserve literal DiscardPile.CardIds order:
        //   index 0       = oldest
        //   index Count-1 = newest/top
        for (int i = 0; i < ids->size; ++i) {
            const int entityId = IntAt(ids, i);
            if (entityId <= 0) {
                ++skipped;
                continue;
            }

            Obj* cardEntity = modelGetEntityById[model](entityId);
            if (!cardEntity) {
                ++skipped;
                LOG_ERR(
                    "CustomComponents/DiscardPileViewer: stale discard EntityId=%d pile=%d index=%d",
                    entityId,
                    discardPileId,
                    i
                );
                continue;
            }

            const int guid = GetCardGuid(cardEntity);
            if (guid <= 0) {
                ++skipped;
                LOG_ERR(
                    "CustomComponents/DiscardPileViewer: discard EntityId=%d has no valid Card.Guid",
                    entityId
                );
                continue;
            }

            Obj* item = inspectionItemClass.CreateNewInstance();
            if (!item) {
                ++skipped;
                LOG_ERR(
                    "CustomComponents/DiscardPileViewer: failed to allocate InspectionItem Guid=%d",
                    guid
                );
                continue;
            }

            inspectionItemCtor[item](guid);

            // Keep the exact discarded EntityId attached to the inspection entry.
            // Stock InspectionItem explicitly supports this field.
            inspectionItemSetEntityId[item](entityId);

            items->Add(item);
        }

        if (items->size <= 0) {
            LOG_INFO(
                "CustomComponents/DiscardPileViewer: hero tapped; pile=%d had %d ids but no inspectable live cards",
                discardPileId,
                ids->size
            );
            return true;
        }

        Obj* config = inspectionConfigClass.CreateNewInstance();
        if (!config) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer: failed to allocate CardInspectionOverlayConfig"
            );
            return true;
        }

        inspectionConfigCtor[config]();

        inspectionConfigItems[config] = items;

        // Open focused on the newest/top card.  The user can swipe backward through
        // older cards using the stock SnapScroller.
        inspectionConfigInitialIndex[config] = items->size - 1;
        inspectionConfigWrapping[config] = false;
        inspectionConfigLockUnownedHeroes[config] = false;
        inspectionConfigAllowHeroPurchase[config] = false;
        inspectionConfigShowCrafting[config] = false;

        LOG_INFO(
            "CustomComponents/DiscardPileViewer: opening pile=%d cards=%d skipped=%d",
            discardPileId,
            items->size,
            skipped
        );

        showInspectionDispatch[showSignal](config);
        return true;
    }

    static int PickTargetHook(
        void* self,
        Obj* overlappedEntities,
        const void* method
    ) {
        const int stockResult =
            originalPickTarget
            ? originalPickTarget(self, overlappedEntities, method)
            : -1;

        // Preserve every normal stock board-tap target.
        // EntityId 0 is valid in this build (the local player commonly uses it).
        if (stockResult >= 0)
            return stockResult;

        auto* command = static_cast<Obj*>(self);
        if (!command || !overlappedEntities)
            return stockResult;

        Obj* overlapFinder =
            boardTapOverlapFinder.IsValid()
            ? boardTapOverlapFinder[command].Get()
            : nullptr;
        if (!overlapFinder)
            return stockResult;

        Obj* viewMapper =
            overlapFinderViewMapper.IsValid()
            ? overlapFinderViewMapper[overlapFinder].Get()
            : nullptr;
        if (!viewMapper)
            return stockResult;

        Obj* model =
            viewMapperEntityModel.IsValid()
            ? viewMapperEntityModel[viewMapper].Get()
            : nullptr;
        if (!model)
            return stockResult;

        // HandleBoardTappedCommand passes a List<int> here in stock.  We only use
        // this fallback when stock found no selectable target at all.
        auto* ids = reinterpret_cast<MonoIntList*>(overlappedEntities);
        if (!ids || !ids->items)
            return stockResult;

        for (int i = 0; i < ids->size; ++i) {
            const int entityId = IntAt(ids, i);
            if (entityId < 0)
                continue;

            Obj* entity = modelGetEntityById[model](entityId);
            if (!entity)
                continue;

            // Player entities own DiscardPileAssociation.  This makes an otherwise
            // ignored plain tap on either hero/player view selectable for our
            // inspection path, while leaving every stock-selected target unchanged.
            if (FindExactComponent(entity, discardAssociationClass)) {
                LOG_INFO(
                    "CustomComponents/DiscardPileViewer: board-tap fallback selected player EntityId=%d",
                    entityId
                );
                return entityId;
            }
        }

        return stockResult;
    }

    static void InspectTappedExecuteHook(
        void* self,
        const void* method
    ) {
        auto* command = static_cast<Obj*>(self);

        if (!command) {
            if (originalInspectTappedExecute)
                originalInspectTappedExecute(self, method);
            return;
        }

        Obj* gesture =
            commandGestureData.IsValid()
            ? commandGestureData[command].Get()
            : nullptr;

        Obj* inspectionUtilities =
            commandCardInspectionModel.IsValid()
            ? commandCardInspectionModel[command].Get()
            : nullptr;

        Obj* showSignal =
            commandShowInspectionSignal.IsValid()
            ? commandShowInspectionSignal[command].Get()
            : nullptr;

        if (!gesture || !inspectionUtilities || !showSignal) {
            if (originalInspectTappedExecute)
                originalInspectTappedExecute(self, method);
            return;
        }

        const int tappedEntityId = gestureGetTappedEntityId[gesture]();

        Obj* model =
            inspectionUtilitiesEntityModel[inspectionUtilities].Get();

        Obj* tappedEntity =
            (model && tappedEntityId >= 0)
            ? modelGetEntityById[model](tappedEntityId)
            : nullptr;

        // Only consume taps on player/hero entities.  Every ordinary tapped card
        // continues into the original InspectTappedCardEntityCommand unchanged.
        if (!tappedEntity ||
            !FindExactComponent(tappedEntity, discardAssociationClass)) {
            if (originalInspectTappedExecute)
                originalInspectTappedExecute(self, method);
            return;
        }

        // A player/hero tap is ours.  Do not call the original card-inspection
        // command afterward.
        BuildAndShowDiscardPile(
            model,
            showSignal,
            tappedEntity
        );
    }

    inline bool InitializeDiscardPileViewer() {
        auto inspectCommand = BNM::Class(
            "PvZCards.Game",
            "InspectTappedCardEntityCommand"
        );
        auto boardTapCommand = BNM::Class(
            "PvZCards.Game",
            "HandleBoardTappedCommand"
        );
        auto overlapFinderClass = BNM::Class(
            "PvZCards.Game",
            "ViewEntityOverlapFinder"
        );
        auto viewMapperClass = BNM::Class(
            "PvZCards.Game",
            "EntityViewMapper"
        );
        auto gesture = BNM::Class(
            "PvZCards.Game",
            "EntityGestureData"
        );
        auto inspectionUtilities = BNM::Class(
            "PvZCards.Game",
            "CardInspectionUtilities"
        );
        auto model = BNM::Class(
            "PvZCards.Engine",
            "EntityModel"
        );
        auto entity = BNM::Class(
            "PvZCards.Engine",
            "Entity"
        );
        auto discardAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "DiscardPileAssociation"
        );
        auto discardPile = BNM::Class(
            "PvZCards.Engine.Components",
            "DiscardPile"
        );
        auto card = BNM::Class(
            "PvZCards.Engine.Components",
            "Card"
        );

        inspectionItemClass = BNM::Class(
            "PvZCards.CardInspection",
            "InspectionItem"
        );
        inspectionConfigClass = BNM::Class(
            "PvZCards.CardInspection",
            "CardInspectionOverlayConfig"
        );
        auto showSignalClass = BNM::Class(
            "PvZCards.CardInspection",
            "ShowCardInspectionOverlaySignal"
        );

        if (!inspectCommand.IsValid() ||
            !boardTapCommand.IsValid() ||
            !overlapFinderClass.IsValid() ||
            !viewMapperClass.IsValid() ||
            !gesture.IsValid() ||
            !inspectionUtilities.IsValid() ||
            !model.IsValid() ||
            !entity.IsValid() ||
            !discardAssociation.IsValid() ||
            !discardPile.IsValid() ||
            !card.IsValid() ||
            !inspectionItemClass.IsValid() ||
            !inspectionConfigClass.IsValid() ||
            !showSignalClass.IsValid()) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer: required class lookup failed"
            );
            return false;
        }

        commandGestureData =
            inspectCommand
            .GetField("<EntityGestureData>k__BackingField")
            .cast<Obj*>();

        commandShowInspectionSignal =
            inspectCommand
            .GetField("<ShowCardInspectionOverlaySignal>k__BackingField")
            .cast<Obj*>();

        commandCardInspectionModel =
            inspectCommand
            .GetField("<CardInspectionModel>k__BackingField")
            .cast<Obj*>();

        boardTapOverlapFinder =
            boardTapCommand
            .GetField("<ViewEntityOverlapFinder>k__BackingField")
            .cast<Obj*>();

        overlapFinderViewMapper =
            overlapFinderClass
            .GetField("<ViewMapper>k__BackingField")
            .cast<Obj*>();

        viewMapperEntityModel =
            viewMapperClass
            .GetField("<EntityModel>k__BackingField")
            .cast<Obj*>();

        gestureGetTappedEntityId =
            gesture
            .GetMethod("get_TappedEntityId", 0)
            .cast<int>();

        inspectionUtilitiesEntityModel =
            inspectionUtilities
            .GetField("<EntityModel>k__BackingField")
            .cast<Obj*>();

        entityComponents =
            entity
            .GetField("Components")
            .cast<MonoObjList*>();

        modelGetEntityById =
            model
            .GetMethod("GetEntityById", 1)
            .cast<Obj*>();

        discardAssociationPileId =
            discardAssociation
            .GetField("DiscardPileId")
            .cast<int>();

        discardPileCardIds =
            discardPile
            .GetField("CardIds")
            .cast<MonoIntList*>();

        cardGuid =
            card
            .GetField("Guid")
            .cast<int>();

        discardAssociationClass = discardAssociation.GetClass();
        discardPileClass = discardPile.GetClass();
        cardClass = card.GetClass();

        inspectionItemCtor =
            inspectionItemClass
            .GetMethod(".ctor", 1)
            .cast<void>();

        inspectionItemSetEntityId =
            inspectionItemClass
            .GetMethod("set_EntityId", 1)
            .cast<void>();

        inspectionConfigCtor =
            inspectionConfigClass
            .GetMethod(".ctor", 0)
            .cast<void>();

        inspectionConfigItems =
            inspectionConfigClass
            .GetField("ItemsToBeViewed")
            .cast<MonoObjList*>();

        inspectionConfigInitialIndex =
            inspectionConfigClass
            .GetField("InitiallyViewedItemIndex")
            .cast<int>();

        inspectionConfigWrapping =
            inspectionConfigClass
            .GetField("Wrapping")
            .cast<bool>();

        inspectionConfigLockUnownedHeroes =
            inspectionConfigClass
            .GetField("LockUnownedHeroes")
            .cast<bool>();

        inspectionConfigAllowHeroPurchase =
            inspectionConfigClass
            .GetField("AllowHeroPurchase")
            .cast<bool>();

        inspectionConfigShowCrafting =
            inspectionConfigClass
            .GetField("ShowCrafting")
            .cast<bool>();

        auto dispatchMethod =
            showSignalClass.GetMethod("Dispatch", 1);

        showInspectionDispatch =
            dispatchMethod.cast<void>();

        if (!dispatchMethod.IsValid()) {
            auto parent = showSignalClass.GetParent();
            LOG_ERR(
                "CustomComponents/DiscardPileViewer/API: "
                "ShowCardInspectionOverlaySignal.Dispatch(1) missing "
                "signalClass=%p parent=%p parentName='%s'",
                showSignalClass.GetClass(),
                parent.IsValid() ? parent.GetClass() : nullptr,
                (parent.IsValid() && parent.GetClass() && parent.GetClass()->name)
                ? parent.GetClass()->name
                : "<null>"
            );
        }

        auto execute =
            inspectCommand.GetMethod("Execute", 0);
        auto pickTarget =
            boardTapCommand.GetMethod("PickTarget", 1);

        const bool apiOk =
            commandGestureData.IsValid() &&
            commandShowInspectionSignal.IsValid() &&
            commandCardInspectionModel.IsValid() &&
            boardTapOverlapFinder.IsValid() &&
            overlapFinderViewMapper.IsValid() &&
            viewMapperEntityModel.IsValid() &&
            gestureGetTappedEntityId.IsValid() &&
            inspectionUtilitiesEntityModel.IsValid() &&
            entityComponents.IsValid() &&
            modelGetEntityById.IsValid() &&
            discardAssociationPileId.IsValid() &&
            discardPileCardIds.IsValid() &&
            cardGuid.IsValid() &&
            discardAssociationClass &&
            discardPileClass &&
            cardClass &&
            inspectionItemCtor.IsValid() &&
            inspectionItemSetEntityId.IsValid() &&
            inspectionConfigCtor.IsValid() &&
            inspectionConfigItems.IsValid() &&
            inspectionConfigInitialIndex.IsValid() &&
            inspectionConfigWrapping.IsValid() &&
            inspectionConfigLockUnownedHeroes.IsValid() &&
            inspectionConfigAllowHeroPurchase.IsValid() &&
            inspectionConfigShowCrafting.IsValid() &&
            showInspectionDispatch.IsValid() &&
            execute.IsValid() &&
            pickTarget.IsValid();

        if (!apiOk) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer/API: "
                "cmdGesture=%d cmdSignal=%d cmdInspectModel=%d "
                "boardOverlap=%d overlapViewMap=%d viewMapModel=%d tappedIdGetter=%d "
                "utilModel=%d entityComponents=%d getEntityById=%d "
                "discardAssocId=%d discardCardIds=%d cardGuid=%d "
                "assocClass=%d pileClass=%d cardClass=%d "
                "itemCtor=%d itemSetEntity=%d configCtor=%d "
                "configItems=%d configInitial=%d configWrapping=%d "
                "configLockHeroes=%d configHeroPurchase=%d configCrafting=%d "
                "signalDispatch=%d execute=%d pickTarget=%d",
                commandGestureData.IsValid() ? 1 : 0,
                commandShowInspectionSignal.IsValid() ? 1 : 0,
                commandCardInspectionModel.IsValid() ? 1 : 0,
                boardTapOverlapFinder.IsValid() ? 1 : 0,
                overlapFinderViewMapper.IsValid() ? 1 : 0,
                viewMapperEntityModel.IsValid() ? 1 : 0,
                gestureGetTappedEntityId.IsValid() ? 1 : 0,
                inspectionUtilitiesEntityModel.IsValid() ? 1 : 0,
                entityComponents.IsValid() ? 1 : 0,
                modelGetEntityById.IsValid() ? 1 : 0,
                discardAssociationPileId.IsValid() ? 1 : 0,
                discardPileCardIds.IsValid() ? 1 : 0,
                cardGuid.IsValid() ? 1 : 0,
                discardAssociationClass ? 1 : 0,
                discardPileClass ? 1 : 0,
                cardClass ? 1 : 0,
                inspectionItemCtor.IsValid() ? 1 : 0,
                inspectionItemSetEntityId.IsValid() ? 1 : 0,
                inspectionConfigCtor.IsValid() ? 1 : 0,
                inspectionConfigItems.IsValid() ? 1 : 0,
                inspectionConfigInitialIndex.IsValid() ? 1 : 0,
                inspectionConfigWrapping.IsValid() ? 1 : 0,
                inspectionConfigLockUnownedHeroes.IsValid() ? 1 : 0,
                inspectionConfigAllowHeroPurchase.IsValid() ? 1 : 0,
                inspectionConfigShowCrafting.IsValid() ? 1 : 0,
                showInspectionDispatch.IsValid() ? 1 : 0,
                execute.IsValid() ? 1 : 0,
                pickTarget.IsValid() ? 1 : 0
            );
            return false;
        }

        BNM::BasicHook(
            execute,
            InspectTappedExecuteHook,
            originalInspectTappedExecute
        );

        BNM::BasicHook(
            pickTarget,
            PickTargetHook,
            originalPickTarget
        );

        if (!originalInspectTappedExecute || !originalPickTarget) {
            LOG_ERR(
                "CustomComponents/DiscardPileViewer: hook install failed inspect=%p pickTarget=%p",
                reinterpret_cast<void*>(originalInspectTappedExecute),
                reinterpret_cast<void*>(originalPickTarget)
            );
            return false;
        }

        LOG_INFO(
            "CustomComponents/DiscardPileViewer: ready "
            "(tap either hero to inspect that player's discard pile; board-tap player fallback enabled)"
        );

        return true;
    }

} // namespace CustomComponents::Utilities::DiscardPileViewer
