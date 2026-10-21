#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "zones/Discarded.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "presentation/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "log.h"

namespace CustomComponents::Systems::DiscardedSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;
    using Source = Components::DiscardSource;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;
    inline BNM::Field<Obj*> resolverModel;
    inline BNM::Field<Obj*> resolverEffectQueue;
    inline BNM::Field<Obj*> triggeredEffectTriggerData;
    inline BNM::Field<int> triggerDataEffectEntityId;
    inline BNM::Field<int> triggerDataSourceId;
    inline BNM::Field<Obj*> triggerDataContext;
    inline BNM::Field<int> contextTargetId;
    inline BNM::Field<MonoList<int>*> discardPileIds;
    inline BNM::Field<int> discardPileAssociationId;
    inline BNM::Method<Obj*> entityGetOrAdd;
    inline BNM::Method<Obj*> entityGet;
    inline BNM::Method<Obj*> entityRemove;
    inline BNM::Method<bool> entityHas;
    inline BNM::Method<bool> queueIsEmpty;
    // The engine drives EffectCollectionProcessor via EffectQueue.HasNext().
    // Observe that boundary, not only ProcessEffects callbacks.
    inline BNM::Field<MonoList<Obj*>*> queueEffects;
    inline BNM::Field<MonoList<Obj*>*> queueResponses;
    inline BNM::Field<MonoList<Obj*>*> queueGroupResponses;
    // Effect-group fields verified against the ARM64 dump:
    // EffectQueue.CurrentGroupId +0x28, NextGroupId +0x2C;
    // Effect.EffectGroupId +0x3C. Observation only.
    inline BNM::Field<int> queueCurrentGroupId;
    inline BNM::Field<int> queueNextGroupId;
    inline BNM::Field<int> effectGroupId;
    inline BNM::Field<int> effectSourceId;
    inline BNM::Field<int> effectEntityId;
    inline BNM::Field<int> effectTargetId;
    // Injected EntityModel fields of the stock trigger/buff processors.
    inline BNM::Field<Obj*> startSystemModel;
    inline BNM::Field<Obj*> buffSystemModel;
    inline BNM::Field<Obj*> endSystemModel;
    inline bool groupMetadataReady = false;
    inline BNM::Method<bool> triggeredHasTriggers;
    inline BNM::Method<int> playerIdForCard;
    inline BNM::MonoType* discardedType = nullptr;
    inline BNM::Field<int> discardedSource;

    inline bool ready = false;

    #include "zones/detail/DiscardSourceMarker.inl"
    #include "zones/detail/ReturnListenerProvenance.inl"
    #include "zones/detail/ReturnEffectQueueCompletion.inl"
    inline bool Initialize() {
        if (ready) return true;
        if (!CustomComponentRegistry::FindByName("PvZCards.Engine.Components", "Discarded")) {
            if (!CustomComponentRegistry::Register({
                    "Discarded", "PvZCards.Engine.Components", "EngineLib.dll",
                    "MarkerComponent", "PvZCards.Engine.Components", "EngineLib.dll"})) {
                LOG_ERR("CustomComponents/Discarded: component registration failed");
                return false;
            }
        }
        if (!CustomFieldBridgeSystem::Register(
                "PvZCards.Engine.Components", "Discarded",
                "PvZCards.Engine.Components", "MarkerComponent",
                {{"Source", CustomFieldBridgeSystem::FieldKind::Int32}},
                false)) return false;

        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto discardPile = BNM::Class("PvZCards.Engine.Components", "DiscardPile");
        auto discardAssoc = BNM::Class("PvZCards.Engine.Components", "DiscardPileAssociation");
        auto discarded = BNM::Class("PvZCards.Engine.Components", "Discarded");
        auto resolver = BNM::Class("PvZCards.Core.Systems", "TriggeredEffectResolverSystem");
        auto startSystem = BNM::Class("PvZCards.Core.Systems", "TriggeredAbilityStartSystem");
        auto buffSystem = BNM::Class("PvZCards.Core.Systems", "BuffSystem");
        auto endSystem = BNM::Class("PvZCards.Core.Systems", "TriggeredAbilityEndSystem");
        auto processor = BNM::Class("PvZCards.Engine", "EffectCollectionProcessor");
        auto triggered = BNM::Class("PvZCards.Engine.Effects", "TriggeredEffect");
        auto triggerData = BNM::Class("PvZCards.Engine", "TriggerData");
        auto context = BNM::Class("PvZCards.Engine", "EffectContext");
        auto queue = BNM::Class("PvZCards.Engine", "EffectQueue");
        auto effectClass = BNM::Class("PvZCards.Engine", "Effect");
        auto collection = BNM::Class("PvZCards.Engine", "TriggeredEffectCollection");
        auto playerUtils = BNM::Class("PvZCards.Core.Utilities", "PlayerUtils");
        if (!model.IsValid() || !entity.IsValid() ||
            !discardPile.IsValid() || !discardAssoc.IsValid() || !discarded.IsValid() ||
            !resolver.IsValid() || !processor.IsValid() ||
            !triggered.IsValid() || !triggerData.IsValid() || !context.IsValid() ||
            !queue.IsValid() || !collection.IsValid() || !playerUtils.IsValid()) return false;

        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");
        resolverModel = resolver.GetField("<Model>k__BackingField");
        resolverEffectQueue = resolver.GetField("<EffectQueue>k__BackingField");
        triggeredEffectTriggerData = triggered.GetField("TriggerData");
        triggerDataEffectEntityId = triggerData.GetField("EffectEntityId");
        triggerDataSourceId = triggerData.GetField("SourceId");
        triggerDataContext = triggerData.GetField("Context");
        contextTargetId = context.GetField("TargetId");
        discardPileIds = discardPile.GetField("CardIds");
        discardPileAssociationId = discardAssoc.GetField("DiscardPileId");
        discardedSource = discarded.GetField("Source");
        discardedType = discarded.GetMonoType();
        entityGetOrAdd = entity.GetMethod("GetOrAddComponent", 1).cast<Obj*>();
        entityGet = entity.GetMethod("GetComponent", 1).cast<Obj*>();
        entityRemove = entity.GetMethod("RemoveComponent", 1).cast<Obj*>();
        entityHas = entity.GetMethod("HasComponent", 1).cast<bool>();
        queueIsEmpty = queue.GetMethod("IsEmpty", 0).cast<bool>();
        queueEffects = queue.GetField("Effects");
        queueResponses = queue.GetField("ResponseEffects");
        queueGroupResponses = queue.GetField("GroupResponseEffects");
        if (effectClass.IsValid()) {
            queueCurrentGroupId = queue.GetField("CurrentGroupId");
            queueNextGroupId = queue.GetField("NextGroupId");
            effectGroupId = effectClass.GetField("EffectGroupId");
            effectSourceId = effectClass.GetField("SourceId");
            effectEntityId = effectClass.GetField("EffectEntityId");
            effectTargetId = effectClass.GetField("TargetId");
            groupMetadataReady = queueCurrentGroupId.IsValid() &&
                queueNextGroupId.IsValid() && effectGroupId.IsValid() &&
                effectSourceId.IsValid() && effectEntityId.IsValid() &&
                effectTargetId.IsValid();
        }
        triggeredHasTriggers = collection.GetMethod("HasTriggers", 0).cast<bool>();
        playerIdForCard = playerUtils.GetMethod("GetPlayerIdFromCard", 1).cast<int>();

        if (!modelEntities.IsValid() || !entityComponents.IsValid() || !entityId.IsValid() ||
            !resolverModel.IsValid() ||
            !resolverEffectQueue.IsValid() || !triggeredEffectTriggerData.IsValid() ||
            !triggerDataEffectEntityId.IsValid() || !triggerDataSourceId.IsValid() ||
            !triggerDataContext.IsValid() || !contextTargetId.IsValid() ||
            !discardPileIds.IsValid() || !discardPileAssociationId.IsValid() ||
            !discardedSource.IsValid() || !discardedType || !entityGetOrAdd.IsValid() ||
            !entityGet.IsValid() || !entityRemove.IsValid() || !entityHas.IsValid() ||
            !queueIsEmpty.IsValid() || !triggeredHasTriggers.IsValid() ||
            !playerIdForCard.IsValid()) {
            LOG_ERR("CustomComponents/Discarded: required metadata missing");
            return false;
        }

        // New stock handlers are OPTIONAL: cleanup v2 remains unchanged if
        // their metadata is absent or hooking cannot be installed.
        if (startSystem.IsValid())
            startSystemModel = startSystem.GetField("<Entities>k__BackingField");
        if (buffSystem.IsValid())
            buffSystemModel = buffSystem.GetField("<Entities>k__BackingField");
        if (endSystem.IsValid())
            endSystemModel = endSystem.GetField("<Model>k__BackingField");
        auto startMethod = startSystem.IsValid()
            ? startSystem.GetMethod("ProcessEffect", 1) : BNM::MethodBase{};
        auto buffMethod = buffSystem.IsValid()
            ? buffSystem.GetMethod("ProcessEffect", 1) : BNM::MethodBase{};
        auto endMethod = endSystem.IsValid()
            ? endSystem.GetMethod("ProcessEffect", 1) : BNM::MethodBase{};
        auto resolverMethod = resolver.GetMethod("ProcessEffect", 1);
        auto hasNextMethod = queue.GetMethod("HasNext", 0);
        auto shiftMethod = queue.GetMethod("Shift", 0);
        auto processEffectsMethod = processor.GetMethod("ProcessEffects", 1);
        auto completeEffectMethod = processor.GetMethod("CompleteEffect", 1);
        BNM::BasicHook(resolverMethod, ResolverHook, originalResolver);
        BNM::BasicHook(hasNextMethod, QueueHasNextHook, originalQueueHasNext);
        // Shift is diagnostic-only and optional; the existing cleanup behavior
        // is unchanged if this entry point cannot be hooked.
        if (groupMetadataReady && shiftMethod.IsValid())
            BNM::BasicHook(shiftMethod, QueueShiftHook, originalQueueShift);
        BNM::BasicHook(processEffectsMethod, ProcessEffectsHook, originalProcessEffects);
        // An optional *read-only* hook to expose the processor's completion
        // boundary. If unavailable the base v2 lifecycle still initializes.
        if (completeEffectMethod.IsValid())
            BNM::BasicHook(completeEffectMethod, CompleteEffectHook, originalCompleteEffect);
        if (groupMetadataReady && startSystemModel.IsValid() && startMethod.IsValid())
            BNM::BasicHook(startMethod, StartHandlerHook, originalStartHandler);
        if (groupMetadataReady && buffSystemModel.IsValid() && buffMethod.IsValid())
            BNM::BasicHook(buffMethod, BuffHandlerHook, originalBuffHandler);
        if (groupMetadataReady && endSystemModel.IsValid() && endMethod.IsValid())
            BNM::BasicHook(endMethod, EndHandlerHook, originalEndHandler);
        CC_DIAG("CustomComponents/Discarded/CHAIN: stock handlers START=%d BUFF=%d END=%d (read-only; RVAs 0x21FC764 / 0x21FD444 / 0x21F2034)",
            originalStartHandler ? 1 : 0,
            originalBuffHandler ? 1 : 0,
            originalEndHandler ? 1 : 0);
        if constexpr (false) CC_DIAG("CustomComponents/Discarded/DIAG: ProcessEffects hooked=%d CompleteEffect hooked=%d HasNext hooked=%d",
            originalProcessEffects ? 1 : 0,
            originalCompleteEffect ? 1 : 0,
            originalQueueHasNext ? 1 : 0);
        if (!originalResolver || !originalQueueHasNext || !originalProcessEffects) {
            LOG_ERR("CustomComponents/Discarded: stock hook installation failed");
            return false;
        }
        ready = true;
        if constexpr (false) CC_DIAG("CustomComponents/Discarded/GROUP: metadata=%d Shift hooked=%d (RVA 0x227FBDC; read-only group lineage diagnostics)",
            groupMetadataReady ? 1 : 0, originalQueueShift ? 1 : 0);
        CC_DIAG("CustomComponents/Discarded: ready (Hand/Deck only; attempt 6 exact stock Start/Buff/End completion, v2 fallback)");
        return true;
    }
} // namespace CustomComponents::Systems::DiscardedSystem
