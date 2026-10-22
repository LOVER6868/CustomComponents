#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "abilities/Charge.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "abilities/GrantableAbilityRuntime.hpp"
#include "log.h"

namespace CustomComponents::Systems {
    namespace ChargeSystem {
        using ProcessBlockableDamageFn =
            void (*)(void* self, void* effect, void* targetEntity, const void* method);
        using GetChargeAmountFn =
            int (*)(void* self, void* blocker, const void* method);
        inline ProcessBlockableDamageFn originalProcessBlockableDamage = nullptr;
        inline GetChargeAmountFn originalGetChargeAmount = nullptr;

        inline BNM::Field<int> sourceId;
        inline BNM::Field<int> targetId;
        inline BNM::Field<int> laneId;
        inline BNM::Method<int> getDamageAmount;
        inline BNM::Field<void*> entityModel;
        inline BNM::Method<void*> getEntityById;
        inline BNM::Method<bool> entityHasComponent;
        inline BNM::Method<void*> entityGetComponent;
        inline BNM::MonoType* monoType = nullptr;

        // Team-Up uses the highest active Charge counter total among allied fighters in the lane.
        inline BNM::MonoType* ownerMonoType = nullptr;
        inline BNM::Field<int> ownerIdField;
        inline BNM::MonoType* laneMonoType = nullptr;
        inline BNM::Method<void*> getFightersForPlayer;

        // Scoped to the current blockable-damage call.
        inline thread_local bool activeCharge = false;
        inline thread_local int activeChargeAmount = 0;

        inline bool TryGetChargeAmount(
            void* blockingSystem,
            void* effect,
            int& outAmount
        ) {
            outAmount = 0;

            if (!blockingSystem || !effect || !monoType)
                return false;

            void* model = entityModel[blockingSystem].Get();
            if (!model)
                return false;

            const int source = sourceId[effect].Get();
            void* sourceEntity = getEntityById[model](source);
            if (!sourceEntity)
                return false;

            if (!entityHasComponent[sourceEntity](monoType))
                return false;

            void* charge = entityGetComponent[sourceEntity](monoType);
            if (!charge || !GrantableAbilityRuntime::Active(charge))
                return false;

            outAmount = GrantableAbilityRuntime::SumValues(charge);


            return true;
        }

        inline void* GetChargeComponent(void* entity);

        inline bool TryGetTeamUpChargeAmount(
            void* blockingSystem,
            void* effect,
            int& outAmount
        ) {
            outAmount = 0;

            if (!blockingSystem || !effect || !monoType || !ownerMonoType ||
                !laneMonoType || !ownerIdField.IsValid() ||
                !getFightersForPlayer.IsValid()) {
                return false;
            }

            void* model = entityModel[blockingSystem].Get();
            if (!model) return false;

            const int source = sourceId[effect].Get();
            const int effectLaneId = laneId[effect].Get();
            if (source < 0 || effectLaneId < 0) return false;

            void* sourceEntity = getEntityById[model](source);
            if (!sourceEntity) return false;

            // The attacking fighter must have Charge before Team-Up can extend it.
            void* sourceCharge = GetChargeComponent(sourceEntity);
            if (!sourceCharge || !GrantableAbilityRuntime::Active(sourceCharge)) return false;

            int bestAmount = GrantableAbilityRuntime::SumValues(sourceCharge);

            void* owner = entityGetComponent[sourceEntity](ownerMonoType);
            if (!owner) {
                outAmount = bestAmount;
                return true;
            }
            const int ownerId = ownerIdField[owner].Get();

            // DamageEffect.LaneId is the lane entity id.
            void* laneEntity = getEntityById[model](effectLaneId);
            if (!laneEntity) {
                outAmount = bestAmount;
                return true;
            }

            void* lane = entityGetComponent[laneEntity](laneMonoType);
            if (!lane) {
                outAmount = bestAmount;
                return true;
            }

            void* fighters = getFightersForPlayer[lane](ownerId);
            if (!fighters) {
                outAmount = bestAmount;
                return true;
            }

            // Resolve List<int> through its managed API rather than its memory layout.
            BNM::Class fightersClass(
                reinterpret_cast<BNM::IL2CPP::Il2CppObject*>(fighters)
            );
            auto getCount = fightersClass.GetMethod("get_Count", 0).cast<int>();
            auto getItem = fightersClass.GetMethod("get_Item", 1).cast<int>();

            if (!getCount.IsValid() || !getItem.IsValid()) {
                outAmount = bestAmount;
                return true;
            }

            const int count = getCount[fighters]();
            for (int i = 0; i < count; ++i) {
                const int fighterId = getItem[fighters](i);
                if (fighterId == source) continue;

                void* fighterEntity = getEntityById[model](fighterId);
                if (!fighterEntity) continue;

                void* fighterCharge = GetChargeComponent(fighterEntity);
                if (!fighterCharge || !GrantableAbilityRuntime::Active(fighterCharge)) continue;

                const int fighterAmount = GrantableAbilityRuntime::SumValues(fighterCharge);
                if (fighterAmount > bestAmount) {
                    bestAmount = fighterAmount;
                }

            }

            outAmount = bestAmount;


            return true;
        }

        inline void ProcessBlockableDamageHook(
            void* self, void* effect, void* targetEntity, const void* method
        ) {
            // Restore the enclosing hook's thread-local state on every C++ exit path.
            struct ScopedChargeState {
                bool savedActive;
                int savedAmount;
                ScopedChargeState() : savedActive(activeCharge), savedAmount(activeChargeAmount) {}
                ~ScopedChargeState() {
                    activeCharge = savedActive;
                    activeChargeAmount = savedAmount;
                }
                ScopedChargeState(const ScopedChargeState&) = delete;
                ScopedChargeState& operator=(const ScopedChargeState&) = delete;
            } scopedState;

            int amount = 0;
            if (TryGetTeamUpChargeAmount(self, effect, amount)) {
                activeCharge = true;
                activeChargeAmount = amount;

            }
            else {
                activeCharge = false;
                activeChargeAmount = 0;
            }

            if (originalProcessBlockableDamage)
                originalProcessBlockableDamage(self, effect, targetEntity, method);
        }

        inline int GetChargeAmountHook(void* self, void* blocker, const void* method) {
            if (activeCharge) {
                return activeChargeAmount;
            }

            return originalGetChargeAmount(self, blocker, method);
        }

        inline void* GetChargeComponent(void* entity) {
            if (!entity || !monoType ||
                !entityHasComponent.IsValid() || !entityGetComponent.IsValid()) {
                return nullptr;
            }

            if (!entityHasComponent[entity](monoType))
                return nullptr;

            return entityGetComponent[entity](monoType);
        }

        inline bool RegisterComponent() {
            auto chargeClass = BNM::Class("PvZCards.Engine.Components", "Charge");
            if (!chargeClass.IsValid()) {
                LOG_ERR("CustomComponents/Charge: managed class was not registered");
                return false;
            }

            if (!CustomComponentRegistry::FindByName("PvZCards.Engine.Components", "Charge")) {
                if (!CustomComponentRegistry::Register({
                        "Charge",
                        "PvZCards.Engine.Components",
                        "EngineLib.dll",
                        "GrantableAbility",
                        "PvZCards.Engine.Components",
                        "EngineLib.dll"
                    })) {
                    LOG_ERR("CustomComponents/Charge: registry registration failed");
                    return false;
                }
            }

            auto* descriptor =
                CustomComponentRegistry::FindByName("PvZCards.Engine.Components", "Charge");
            monoType = descriptor ? descriptor->monoType : nullptr;
            auto chargeDeepCopy = chargeClass.GetMethod("DeepCopyFrom", 2);
            auto chargePoolReturn = chargeClass.GetMethod("HandleReturnToPool", 1);

            if (!monoType || !chargeDeepCopy.IsValid() || !chargePoolReturn.IsValid()) {
                LOG_ERR("CustomComponents/Charge: MonoType/lifecycle initialization failed");
                return false;
            }

            return true;
        }

        inline bool ResolveEngineApiAndInstallHooks() {
            auto effectClass = BNM::Class("PvZCards.Engine", "Effect");
            auto entityModelClass = BNM::Class("PvZCards.Engine", "EntityModel");
            auto entityClass = BNM::Class("PvZCards.Engine", "Entity");
            auto ownerClass = BNM::Class("PvZCards.Engine.Components", "Owner");
            auto laneClass = BNM::Class("PvZCards.Engine.Components", "Lane");
            auto damageClass = BNM::Class("PvZCards.Engine.Effects", "DamageEffect");
            auto blockingClass = BNM::Class("PvZCards.Core.Systems", "HeroBlockingSystem");
            auto meterClass = BNM::Class(
                "PvZCards.Core.Systems.Utilities", "HeroBlockMeterModifier"
            );

            if (!effectClass.IsValid() || !entityModelClass.IsValid() ||
                !entityClass.IsValid() || !ownerClass.IsValid() || !laneClass.IsValid() ||
                !damageClass.IsValid() || !blockingClass.IsValid() || !meterClass.IsValid()) {
                LOG_ERR("CustomComponents/Charge: required engine classes were not found");
                return false;
            }

            sourceId = effectClass.GetField("SourceId");
            targetId = effectClass.GetField("TargetId");
            laneId = damageClass.GetField("LaneId");
            getDamageAmount = damageClass.GetMethod("get_DamageAmount", 0);
            entityModel = blockingClass.GetField("<EntityModel>k__BackingField");
            getEntityById = entityModelClass.GetMethod("GetEntityById", 1);
            entityHasComponent = entityClass.GetMethod("HasComponent", 1);
            entityGetComponent = entityClass.GetMethod("GetComponent", 1);
            ownerMonoType = ownerClass.GetMonoType();
            ownerIdField = ownerClass.GetField("OwnerId");
            laneMonoType = laneClass.GetMonoType();
            getFightersForPlayer = laneClass.GetMethod("GetFightersForPlayer", 1);

            if (!sourceId.IsValid() || !targetId.IsValid() || !laneId.IsValid() ||
                !getDamageAmount.IsValid() || !entityModel.IsValid() ||
                !getEntityById.IsValid() || !entityHasComponent.IsValid() ||
                !entityGetComponent.IsValid() || !ownerMonoType || !ownerIdField.IsValid() ||
                !laneMonoType || !getFightersForPlayer.IsValid()) {
                LOG_ERR("CustomComponents/Charge: required engine fields/methods were not found");
                return false;
            }

            auto processBlockableDamage = blockingClass.GetMethod("ProcessBlockableDamage", 2);
            auto getChargeAmount = meterClass.GetMethod("GetChargeAmount", 1);

            if (!processBlockableDamage.IsValid() || !getChargeAmount.IsValid()) {
                LOG_ERR("CustomComponents/Charge: required gameplay hook targets were not found");
                return false;
            }

            BNM::BasicHook(
                processBlockableDamage, ProcessBlockableDamageHook, originalProcessBlockableDamage
            );
            BNM::BasicHook(getChargeAmount, GetChargeAmountHook, originalGetChargeAmount);

            if (!originalProcessBlockableDamage || !originalGetChargeAmount) {
                LOG_ERR("CustomComponents/Charge: gameplay hook installation failed");
                return false;
            }

            CC_DIAG("CustomComponents/Charge: ready");
            return true;
        }

    } // namespace ChargeSystem

    // Registers Charge and installs its runtime integration.
    inline bool InitializeCharge() {
        return ChargeSystem::RegisterComponent() &&
            ChargeSystem::ResolveEngineApiAndInstallHooks();
    }

} // namespace CustomComponents::Systems