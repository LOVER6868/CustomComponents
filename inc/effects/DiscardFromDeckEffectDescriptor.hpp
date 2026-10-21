#pragma once

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

namespace CustomComponents::Components {

    // JSON-facing bridge for EA's dormant stock DiscardFromDeckEffect.
    //
    // The descriptor intentionally has no additional JSON fields.
    // One descriptor creates one stock DiscardFromDeckEffect.  DeckId and
    // DiscardPileId are left as -1 here and filled from the TARGET owner by
    // the deck-discard system hook after stock effect context assigns TargetId.
    struct DiscardFromDeckEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            DiscardFromDeckEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "DiscardFromDeckEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "DestroyCardEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        static BNM::IL2CPP::Il2CppObject* CreateRuntime(
            BNM::IL2CPP::Il2CppObject* instances
        ) {
            if (!instances)
                return nullptr;

            auto runtimeClass = BNM::Class(
                "PvZCards.Engine.Effects",
                "DiscardFromDeckEffect"
            );
            auto instanceManager = BNM::Class(
                "PvZCards.Engine",
                "InstanceManager"
            );
            if (!runtimeClass.IsValid() || !instanceManager.IsValid())
                return nullptr;

            auto acquire = instanceManager.GetMethod("Acquire", 2).cast<void*>();
            auto deckId = runtimeClass.GetField("DeckId").cast<int>();
            auto discardPileId = runtimeClass.GetField("DiscardPileId").cast<int>();
            if (!acquire.IsValid() || !deckId.IsValid() || !discardPileId.IsValid())
                return nullptr;

            void* runtime = acquire[instances](runtimeClass.GetMonoType(), true);
            if (!runtime)
                return nullptr;

            // Sentinel used only by our bridge. The stock system never sees
            // these values: ProcessEffectHook resolves them from the selected target card owner.
            deckId[runtime] = -1;
            discardPileId[runtime] = -1;
            return static_cast<BNM::IL2CPP::Il2CppObject*>(runtime);
        }

        BNM::IL2CPP::Il2CppObject* ToEffect(
            BNM::IL2CPP::Il2CppObject* instances,
            BNM::IL2CPP::Il2CppObject* effectEntity
        ) {
            (void)effectEntity;
            return CreateRuntime(instances);
        }

        BNM_CustomMethod(
            ToEffect,
            false,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "Effect",
                "EngineLib.dll"
            ).Build(),
            "ToEffect",
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "InstanceManager",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "Entity",
                "EngineLib.dll"
            ).Build()
        );
    };

} // namespace CustomComponents::Components
