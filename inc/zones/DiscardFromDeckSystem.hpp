#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include <string>
#include <vector>

#include "effects/DiscardFromDeckEffectDescriptor.hpp"
#include "effects/DiscardFromHandEffectDescriptor.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "triggers/CustomTriggerSystem.hpp"
#include "log.h"

namespace CustomComponents::Systems::DiscardFromDeckSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    inline BNM::Field<int> effectPlayerId;
    inline BNM::Field<int> effectSourceId;
    inline BNM::Field<int> effectTargetId;
    inline BNM::Field<int> effectDeckId;
    inline BNM::Field<int> effectDiscardPileId;
    inline BNM::Field<Obj*> systemModel;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;

    inline BNM::Field<int> deckAssociationId;
    inline BNM::Field<int> discardAssociationId;
    inline BNM::Method<int> getPlayerIdFromCard;
    inline BNM::Field<MonoList<Obj*>*> orderedPoolEntries;
    inline BNM::Field<int> cardPoolEntryEntityId;

    using DescriptorToEffectFn = Obj * (*)(void*, Obj*, Obj*, const void*);
    using ProcessEffectFn = void (*)(void*, Obj*, const void*);

    inline DescriptorToEffectFn originalDestroyDescriptorToEffect = nullptr;
    inline ProcessEffectFn originalProcessEffect = nullptr;

    template<typename T>
    inline std::vector<T> Items(MonoList<T>* list) {
        std::vector<T> out;
        if (!list || !list->items)
            return out;
        out.reserve(list->size);
        for (int i = 0; i < list->size; ++i)
            out.push_back(list->items->m_Items[i]);
        return out;
    }

    inline std::string TypeName(Obj* object) {
        if (!object)
            return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "?";
    }

    inline Obj* FindComponent(Obj* entity, const char* name) {
        if (!entity)
            return nullptr;
        for (auto* component : Items(entityComponents[entity].Get())) {
            if (TypeName(component) == name)
                return component;
        }
        return nullptr;
    }

    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;
        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

    inline bool DeckContainsCard(Obj* model, int deckId, int cardId) {
        if (!model || deckId < 0 || cardId <= 0)
            return false;
        auto* deck = FindEntity(model, deckId);
        auto* pool = FindComponent(deck, "OrderedCardPool");
        auto* list = pool ? orderedPoolEntries[pool].Get() : nullptr;
        if (!list)
            return false;
        for (auto* entry : Items(list)) {
            if (entry && cardPoolEntryEntityId[entry].Get() == cardId)
                return true;
        }
        return false;
    }

    inline bool ResolveAssociations(Obj* model, Obj* effect) {
        if (!model || !effect)
            return false;

        const int sourceId = effectSourceId[effect].Get();
        const int targetId = effectTargetId[effect].Get();
        const int contextPlayerId = effectPlayerId[effect].Get();

        // DiscardFromDeck is target-owned.  The source only caused the discard;
        // it must never decide which deck/discard pile receives the card.
        auto* target = FindEntity(model, targetId);
        if (!target) {
            LOG_ERR(
                "CustomComponents/DiscardFromDeck: target entity %d not found (SourceId=%d ContextPlayerId=%d)",
                targetId, sourceId, contextPlayerId
            );
            return false;
        }

        Obj* targetPlayer = nullptr;
        int targetPlayerId = -1;

        // A player entity already owns DeckAssociation + DiscardPileAssociation.
        auto* directDeckAssociation = FindComponent(target, "DeckAssociation");
        auto* directDiscardAssociation = FindComponent(target, "DiscardPileAssociation");
        if (directDeckAssociation && directDiscardAssociation) {
            targetPlayer = target;
            targetPlayerId = entityId[target].Get();
        }
        else {
            // Card/hero/fighter target: ask the game's own PlayerUtils who owns it.
            targetPlayerId = getPlayerIdFromCard[static_cast<Obj*>(nullptr)](target);
            targetPlayer = FindEntity(model, targetPlayerId);
        }

        if (!targetPlayer) {
            LOG_ERR(
                "CustomComponents/DiscardFromDeck: could not resolve TARGET owner TargetId=%d -> PlayerId=%d (SourceId=%d ContextPlayerId=%d)",
                targetId, targetPlayerId, sourceId, contextPlayerId
            );
            return false;
        }

        auto* deckAssociation = FindComponent(targetPlayer, "DeckAssociation");
        auto* discardAssociation = FindComponent(targetPlayer, "DiscardPileAssociation");
        if (!deckAssociation || !discardAssociation) {
            LOG_ERR(
                "CustomComponents/DiscardFromDeck: target owner %d missing associations (deck=%s discard=%s)",
                targetPlayerId,
                deckAssociation ? "yes" : "no",
                discardAssociation ? "yes" : "no"
            );
            return false;
        }

        const int deckId = deckAssociationId[deckAssociation].Get();
        const int discardPileId = discardAssociationId[discardAssociation].Get();

        // InDeckQuery now returns the real entity stored by CardPoolEntry.EntityId.
        // Validate that identity before delegating to the stock system.
        if (!DeckContainsCard(model, deckId, targetId)) {
            LOG_ERR(
                "CustomComponents/DiscardFromDeck: TargetId=%d is not present in target owner deck %d",
                targetId, deckId
            );
            return false;
        }

        effectDeckId[effect] = deckId;
        effectDiscardPileId[effect] = discardPileId;
        return true;
    }

    static Obj* DestroyDescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        auto* descriptor = static_cast<Obj*>(self);
        const std::string typeName = TypeName(descriptor);

        if (typeName == "DiscardFromHandEffectDescriptor") {
            Obj* runtime = Components::DiscardFromHandEffectDescriptor::CreateRuntime(instances);
            CC_DIAG("CustomComponents/DiscardFromHand: descriptor -> MoveToDiscardPileEffect=%p", runtime);
            return runtime;
        }

        if (typeName != "DiscardFromDeckEffectDescriptor")
            return originalDestroyDescriptorToEffect(
                self, instances, effectEntity, method
            );

        Obj* runtime = Components::DiscardFromDeckEffectDescriptor::CreateRuntime(
            instances
        );

        return runtime;
    }

    static void ProcessEffectHook(void* self, Obj* effect, const void* method) {
        if (!effect) {
            originalProcessEffect(self, effect, method);
            return;
        }

        // Effects produced by our descriptor leave these associations unset.
        // Resolve them from the actual deck-card target, then delegate unchanged
        // to the stock DiscardFromDeckSystem.
        const bool ours =
            effectDeckId[effect].Get() < 0 &&
            effectDiscardPileId[effect].Get() < 0;

        if (ours) {
            Obj* model = systemModel[static_cast<Obj*>(self)].Get();
            if (!ResolveAssociations(model, effect)) {
                LOG_ERR("CustomComponents/DiscardFromDeck: unresolved custom effect; skipping stock ProcessEffect");
                return;
            }
        }

        Obj* model = systemModel[static_cast<Obj*>(self)].Get();
        const int cardId = effectTargetId[effect].Get();
        const int deckId = effectDeckId[effect].Get();

        // Stock DiscardFromDeck may use a PLAYER as Effect.TargetId and pick
        // a random card from the deck. Capture the actual entity IDs in the
        // ordered deck pool rather than assuming TargetId is the card.
        std::vector<int> preDiscardDeckCards;
        if (model && deckId >= 0) {
            Obj* deck = FindEntity(model, deckId);
            Obj* pool = FindComponent(deck, "OrderedCardPool");
            Obj* entries = pool ? static_cast<Obj*>(pool) : nullptr;
            if (entries) {
                for (Obj* entry : Items(orderedPoolEntries[entries].Get())) {
                    if (entry) preDiscardDeckCards.push_back(cardPoolEntryEntityId[entry].Get());
                }
            }
        }

        originalProcessEffect(self, effect, method);

        for (int priorCardId : preDiscardDeckCards) {
            if (priorCardId >= 0 &&
                !DeckContainsCard(model, deckId, priorCardId)) {
                DiscardedSystem::Mark(model, priorCardId, DiscardedSystem::Source::Deck);
            }
        }

        // This system is the single owner of the DiscardFromDeckSystem hook.
        // Fire only after stock processing, and only when the targeted live card
        // is no longer present in the source deck.
        if (model && cardId >= 0 && deckId >= 0 &&
            !DeckContainsCard(model, deckId, cardId)) {
            CC_DIAG(
                "CustomComponents/DiscardOutsidePlayTrigger: DECK EntityId=%d DeckId=%d",
                cardId,
                deckId
            );
            // The exact moved entity was tagged from the pre/post deck diff.
            CustomTriggerSystem::FireDiscardOutsidePlay(effect);
        }
    }


    inline bool RegisterType() {
        if (!CustomComponentRegistry::FindByName(
            "PvZCards.Engine.Components",
            "DiscardFromDeckEffectDescriptor")) {
            if (!CustomComponentRegistry::Register({
                    "DiscardFromDeckEffectDescriptor",
                    "PvZCards.Engine.Components",
                    "EngineLib.dll",
                    "DestroyCardEffectDescriptor",
                    "PvZCards.Engine.Components",
                    "EngineLib.dll"
                })) {
                return false;
            }
        }
        return true;
    }

    inline bool InstallHooks() {
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto playerUtils = BNM::Class("PvZCards.Core.Utilities", "PlayerUtils");
        auto destroyDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "DestroyCardEffectDescriptor"
        );
        auto runtime = BNM::Class("PvZCards.Engine.Effects", "DiscardFromDeckEffect");
        auto system = BNM::Class("PvZCards.Core.Systems", "DiscardFromDeckSystem");
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto deckAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "DeckAssociation"
        );
        auto discardAssociation = BNM::Class(
            "PvZCards.Engine.Components",
            "DiscardPileAssociation"
        );
        auto orderedPool = BNM::Class(
            "PvZCards.Engine.Components",
            "OrderedCardPool"
        );
        auto cardPoolEntry = BNM::Class(
            "PvZCards.Engine.Components.Data",
            "CardPoolEntry"
        );

        if (!effect.IsValid() || !playerUtils.IsValid() || !destroyDescriptor.IsValid() ||
            !runtime.IsValid() || !system.IsValid() ||
            !model.IsValid() || !entity.IsValid() ||
            !deckAssociation.IsValid() || !discardAssociation.IsValid() ||
            !orderedPool.IsValid() || !cardPoolEntry.IsValid()) {
            LOG_ERR("CustomComponents/DiscardFromDeck: required classes missing");
            return false;
        }

        effectPlayerId = effect.GetField("PlayerId");
        effectSourceId = effect.GetField("SourceId");
        effectTargetId = effect.GetField("TargetId");
        effectDeckId = runtime.GetField("DeckId");
        effectDiscardPileId = runtime.GetField("DiscardPileId");
        systemModel = system.GetField("<Model>k__BackingField");
        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");
        deckAssociationId = deckAssociation.GetField("DeckEntityId");
        discardAssociationId = discardAssociation.GetField("DiscardPileId");
        getPlayerIdFromCard = playerUtils.GetMethod("GetPlayerIdFromCard", 1).cast<int>();
        orderedPoolEntries = orderedPool.GetField("Entries");
        cardPoolEntryEntityId = cardPoolEntry.GetField("EntityId");

        auto descriptorToEffect = destroyDescriptor.GetMethod("ToEffect", 2);
        auto process = system.GetMethod("ProcessEffect", 1);

        if (!effectPlayerId.IsValid() || !effectSourceId.IsValid() ||
            !effectTargetId.IsValid() || !effectDeckId.IsValid() ||
            !effectDiscardPileId.IsValid() || !systemModel.IsValid() ||
            !modelEntities.IsValid() || !entityComponents.IsValid() ||
            !entityId.IsValid() || !deckAssociationId.IsValid() ||
            !getPlayerIdFromCard.IsValid() ||
            !discardAssociationId.IsValid() || !orderedPoolEntries.IsValid() ||
            !cardPoolEntryEntityId.IsValid() ||
            !descriptorToEffect.IsValid() || !process.IsValid()) {
            LOG_ERR("CustomComponents/DiscardFromDeck: required API missing");
            return false;
        }

        BNM::BasicHook(
            descriptorToEffect,
            DestroyDescriptorToEffectHook,
            originalDestroyDescriptorToEffect
        );
        BNM::BasicHook(process, ProcessEffectHook, originalProcessEffect);

        if (!originalDestroyDescriptorToEffect || !originalProcessEffect) {
            LOG_ERR("CustomComponents/DiscardFromDeck: hook installation failed");
            return false;
        }

        CC_DIAG("CustomComponents/DiscardFromDeck: ready");
        return true;
    }

    inline bool Initialize() {
        return RegisterType() && InstallHooks();
    }

} // namespace CustomComponents::Systems::DiscardFromDeckSystem
