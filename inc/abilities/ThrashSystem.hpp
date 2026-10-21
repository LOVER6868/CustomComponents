#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/MethodBase.hpp>

#include <algorithm>
#include <vector>

#include "abilities/Thrash.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "abilities/GrantableAbilityRuntime.hpp"
#include "log.h"

namespace CustomComponents::Systems {
    namespace ThrashSystem {

        inline BNM::MonoType* monoType = nullptr;

        inline BNM::Method<bool> entityHasComponent;
        inline BNM::Method<void*> entityGetComponent;

        // -----------------------------------------------------------------
        // Thrash gameplay
        //
        // Any excess damage dealt by a Thrash source to a fighter is also
        // dealt directly to the hero opposing THAT SOURCE card.
        //
        // Normal / non-Team-Up:
        //     overflow = max(0, damage - healthBefore)
        //
        // Team-Up combat:
        //     wait until the complete contribution group has resolved
        //     create one shared overflow pool for the target
        //     allocate that pool back-to-front
        //     emit only positive per-source Thrash allocations
        //     so zero allocations never enter the block-meter pipeline
        //
        // DamageListBuilder gives us the combat construction batch so Team-Up
        // contributions can be grouped before DamageSystem applies them.
        // DamageSystem::DamageTarget gives us final resolved damage and the
        // target's health immediately before damage.
        // -----------------------------------------------------------------

        struct AttackData {
            int DamageAmount;
            bool IsOverriden;
            unsigned char _padding[3];
        };

        struct DamageData {
            int SourceId;
            int TargetId;
            int LaneId;
            AttackData Attack;
        };

        static_assert(sizeof(AttackData) == 8);
        static_assert(sizeof(DamageData) == 20);

        struct PendingHit {
            int sourceId = -1;
            int damage = 0;
            int effectGroupId = -1;
            bool consumed = false;
        };

        struct PendingGroup {
            int targetId = -1;
            int laneId = -1;
            int initialHealth = -1;
            int remainingOverflow = -1;
            std::vector<PendingHit> hits;
        };

        using CreateAndAddDamageEffectFn =
            void (*)(void*, DamageData, bool, const void*);
        inline CreateAndAddDamageEffectFn originalCreateAndAddDamageEffect = nullptr;

        using DamageTargetFn =
            void (*)(void*, void*, void*, int, const void*);
        inline DamageTargetFn originalDamageTarget = nullptr;

        inline BNM::Field<int> damageEffectSourceId;
        inline BNM::Field<int> damageEffectTargetId;
        inline BNM::Field<int> damageEffectPlayerId;
        inline BNM::Field<int> damageEffectEffectGroupId;
        inline BNM::Field<int> damageEffectLaneId;
        inline BNM::Field<bool> damageEffectShowParticleEffect;
        inline BNM::Method<void> damageEffectSetDamageAmount;
        inline BNM::Method<int> healthCalculateHealth;

        inline BNM::Field<void*> damageSystemEntities;
        inline BNM::Method<void*> getEntityById;

        inline BNM::MonoType* ownerMonoType = nullptr;
        inline BNM::Field<int> ownerIdField;
        inline BNM::MonoType* healthMonoType = nullptr;

        inline BNM::Field<void*> entityInstanceManager;
        inline BNM::MonoType* damageEffectMonoType = nullptr;
        inline BNM::Method<void*> instanceManagerAcquire;
        inline BNM::Method<void> instanceManagerReturn;

        using EffectRouterProcessFn =
            void (*)(void*, void*, const void*);
        inline EffectRouterProcessFn originalEffectRouterProcess = nullptr;

        inline thread_local void* activeEffectRouter = nullptr;
        inline thread_local std::vector<PendingGroup> pendingGroups;
        inline thread_local bool emittingThrashDamage = false;

        inline Traits::Thrash* GetThrashComponent(void* entity) {
            if (!entity || !monoType ||
                !entityHasComponent.IsValid() ||
                !entityGetComponent.IsValid()) {
                return nullptr;
            }

            if (!entityHasComponent[entity](monoType))
                return nullptr;

            return static_cast<Traits::Thrash*>(
                entityGetComponent[entity](monoType)
            );
        }

        inline bool HasThrash(void* entity) {
            auto* thrash = GetThrashComponent(entity);
            return GrantableAbilityRuntime::Active(thrash);
        }

        inline bool GroupAlreadyHasSource(
            const PendingGroup& group,
            int sourceId
        ) {
            for (const auto& hit : group.hits) {
                if (hit.sourceId == sourceId)
                    return true;
            }
            return false;
        }

        inline void RecordPendingDamage(const DamageData& data) {
            // A repeated source for the same target/lane starts a new combat
            // construction group. This keeps repeated simulations from being
            // folded into one enormous Team-Up batch.
            const bool canAppend =
                !pendingGroups.empty() &&
                pendingGroups.back().targetId == data.TargetId &&
                pendingGroups.back().laneId == data.LaneId &&
                !GroupAlreadyHasSource(
                    pendingGroups.back(),
                    data.SourceId
                );

            if (!canAppend) {
                PendingGroup group;
                group.targetId = data.TargetId;
                group.laneId = data.LaneId;
                pendingGroups.push_back(group);
            }

            pendingGroups.back().hits.push_back({
                data.SourceId,
                data.Attack.DamageAmount,
                -1,
                false
            });

            // This queue is only correlation state. Never let stale simulation
            // batches grow without bound.
            if (pendingGroups.size() > 64) {
                pendingGroups.erase(
                    pendingGroups.begin(),
                    pendingGroups.begin() + 32
                );
            }
        }

        inline PendingGroup* MatchPendingGroup(
            int sourceId,
            int targetId,
            int laneId,
            int resolvedDamage,
            int effectGroupId
        ) {
            for (auto& group : pendingGroups) {
                if (group.targetId != targetId ||
                    group.laneId != laneId) {
                    continue;
                }

                for (auto& hit : group.hits) {
                    if (!hit.consumed &&
                        hit.sourceId == sourceId &&
                        hit.damage == resolvedDamage) {
                        hit.effectGroupId = effectGroupId;
                        hit.consumed = true;
                        return &group;
                    }
                }
            }

            return nullptr;
        }

        inline int SumGroupDamage(const PendingGroup& group) {
            int total = 0;
            for (const auto& hit : group.hits)
                total += std::max(0, hit.damage);
            return total;
        }

        inline bool IsTeamUpGroup(const PendingGroup& group) {
            if (group.hits.size() < 2)
                return false;

            const int firstSource = group.hits.front().sourceId;
            for (const auto& hit : group.hits) {
                if (hit.sourceId != firstSource)
                    return true;
            }
            return false;
        }

        inline void CleanupConsumedGroups() {
            pendingGroups.erase(
                std::remove_if(
                    pendingGroups.begin(),
                    pendingGroups.end(),
                    [](const PendingGroup& group) {
                        if (group.hits.empty())
                            return true;

                        for (const auto& hit : group.hits) {
                            if (!hit.consumed)
                                return false;
                        }
                        return true;
                    }
                ),
                pendingGroups.end()
            );
        }

        static void CreateAndAddDamageEffectHook(
            void* self,
            DamageData damageData,
            bool showParticleEffect,
            const void* method
        ) {
            RecordPendingDamage(damageData);

            originalCreateAndAddDamageEffect(
                self,
                damageData,
                showParticleEffect,
                method
            );
        }

        static void EffectRouterProcessHook(
            void* self,
            void* effect,
            const void* method
        ) {
            void* previousRouter = activeEffectRouter;
            activeEffectRouter = self;

            originalEffectRouterProcess(self, effect, method);

            activeEffectRouter = previousRouter;
        }

        inline bool EmitThrashDamageEffect(
            void* sourceEntity,
            int sourceId,
            int heroId,
            int laneId,
            int damageAmount,
            int effectGroupId
        ) {
            if (!sourceEntity ||
                !activeEffectRouter ||
                damageAmount <= 0 ||
                !entityInstanceManager.IsValid() ||
                !damageEffectMonoType ||
                !instanceManagerAcquire.IsValid() ||
                !instanceManagerReturn.IsValid()) {
                return false;
            }

            void* instances =
                entityInstanceManager[sourceEntity].Get();
            if (!instances)
                return false;

            void* effect =
                instanceManagerAcquire[instances](
                    damageEffectMonoType,
                    true
                );
            if (!effect)
                return false;

            int playerId = -1;
            if (ownerMonoType &&
                entityHasComponent[sourceEntity](ownerMonoType)) {
                void* sourceOwner =
                    entityGetComponent[sourceEntity](ownerMonoType);
                if (sourceOwner)
                    playerId = ownerIdField[sourceOwner].Get();
            }

            damageEffectSourceId[effect].Set(sourceId);
            damageEffectTargetId[effect].Set(heroId);
            damageEffectPlayerId[effect].Set(playerId);
            damageEffectEffectGroupId[effect].Set(effectGroupId);
            damageEffectLaneId[effect].Set(laneId);
            damageEffectShowParticleEffect[effect].Set(true);
            damageEffectSetDamageAmount[effect](damageAmount);

            // Route EACH Thrash source's synthetic hit separately through the
            // stock EffectRouter. In a Team-Up lane this means two Thrash cards
            // can produce two independent block-meter charges, while the normal
            // combined Team-Up combat hit remains completely untouched and keeps
            // the game's stock one-charge behavior.
            //
            // The SourceId remains the actual Thrash card, so only the routed
            // Thrash shots get this extra per-source processing.
            emittingThrashDamage = true;
            originalEffectRouterProcess(
                activeEffectRouter,
                effect,
                nullptr
            );
            emittingThrashDamage = false;

            instanceManagerReturn[instances](effect);
            return true;
        }

        inline bool GroupComplete(const PendingGroup& group) {
            if (group.hits.empty())
                return false;

            for (const auto& hit : group.hits) {
                if (!hit.consumed)
                    return false;
            }
            return true;
        }

        inline int ResolveOpposingHeroId(
            void* sourceEntity,
            void* targetEntity
        ) {
            if (!sourceEntity ||
                !targetEntity ||
                !ownerMonoType ||
                !entityHasComponent[sourceEntity](ownerMonoType) ||
                !entityHasComponent[targetEntity](ownerMonoType)) {
                return -1;
            }

            void* sourceOwner =
                entityGetComponent[sourceEntity](ownerMonoType);
            void* targetOwner =
                entityGetComponent[targetEntity](ownerMonoType);

            if (!sourceOwner || !targetOwner)
                return -1;

            const int sourceHeroId =
                ownerIdField[sourceOwner].Get();
            const int candidateOpponentHeroId =
                ownerIdField[targetOwner].Get();

            // The target fighter is only used to identify the other player.
            // The important rule is SOURCE-relative: never route Thrash to the
            // source card's own hero. If the target belongs to the same owner,
            // there is no safe opponent resolution at this boundary.
            if (sourceHeroId < 0 ||
                candidateOpponentHeroId < 0 ||
                candidateOpponentHeroId == sourceHeroId) {
                return -1;
            }

            return candidateOpponentHeroId;
        }

        inline void EmitCompletedTeamUpThrash(
            PendingGroup& group,
            void* model,
            void* targetEntity
        ) {
            if (!model ||
                !targetEntity ||
                group.initialHealth < 0 ||
                !GroupComplete(group)) {
                return;
            }

            int remainingOverflow =
                std::max(
                    0,
                    SumGroupDamage(group) - group.initialHealth
                );

            if (remainingOverflow <= 0)
                return;

            // Finalize only after every Team-Up contribution has resolved.
            // Allocate BACK -> FRONT so later/back Thrash owns overflow first.
            for (auto it = group.hits.rbegin();
                 it != group.hits.rend() && remainingOverflow > 0;
                 ++it) {

                void* sourceEntity =
                    getEntityById[model](it->sourceId);

                if (!sourceEntity || !HasThrash(sourceEntity))
                    continue;

                const int heroId =
                    ResolveOpposingHeroId(
                        sourceEntity,
                        targetEntity
                    );

                if (heroId < 0)
                    continue;

                const int allocation =
                    std::min(
                        std::max(0, it->damage),
                        remainingOverflow
                    );

                if (allocation <= 0)
                    continue;

                remainingOverflow -= allocation;

                if (EmitThrashDamageEffect(
                        sourceEntity,
                        it->sourceId,
                        heroId,
                        group.laneId,
                        allocation,
                        it->effectGroupId)) {
                    
                } else {
                    LOG_ERR(
                        "CustomComponents/Thrash: failed to route Team-Up hero damage source=%d hero=%d damage=%d",
                        it->sourceId,
                        heroId,
                        allocation
                    );
                }
            }
        }

        static void DamageTargetHook(
            void* self,
            void* damageEffect,
            void* healthComponent,
            int damageAmount,
            const void* method
        ) {
            if (!damageEffect || !healthComponent ||
                emittingThrashDamage) {
                originalDamageTarget(
                    self,
                    damageEffect,
                    healthComponent,
                    damageAmount,
                    method
                );
                return;
            }

            const int sourceId =
                damageEffectSourceId[damageEffect].Get();
            const int targetId =
                damageEffectTargetId[damageEffect].Get();
            const int laneId =
                damageEffectLaneId[damageEffect].Get();
            const int effectGroupId =
                damageEffectEffectGroupId[damageEffect].Get();

            const int healthBefore =
                healthCalculateHealth[healthComponent]();

            PendingGroup* group =
                MatchPendingGroup(
                    sourceId,
                    targetId,
                    laneId,
                    damageAmount,
                    effectGroupId
                );

            if (group && group->initialHealth < 0)
                group->initialHealth = healthBefore;

            void* model =
                damageSystemEntities[self].Get();

            void* sourceEntity =
                model ? getEntityById[model](sourceId) : nullptr;
            void* targetEntity =
                model ? getEntityById[model](targetId) : nullptr;

            bool validFighterTarget = false;

            if (targetEntity &&
                ownerMonoType &&
                healthMonoType &&
                entityHasComponent[targetEntity](ownerMonoType)) {

                void* targetOwner =
                    entityGetComponent[targetEntity](ownerMonoType);

                if (targetOwner) {
                    const int targetOwnerId =
                        ownerIdField[targetOwner].Get();

                    // Fighter-only target rule. A hero entity is its own owner.
                    validFighterTarget =
                        targetOwnerId >= 0 &&
                        targetOwnerId != targetId;
                }
            }

            const bool teamUp =
                group && IsTeamUpGroup(*group);

            // Stock always resolves the original fighter hit first.
            originalDamageTarget(
                self,
                damageEffect,
                healthComponent,
                damageAmount,
                method
            );

            if (validFighterTarget && model) {
                if (teamUp) {
                    // Critical sequencing rule:
                    // DO NOT emit any Team-Up Thrash shot until the complete
                    // contribution group has resolved. This prevents a front
                    // source whose final allocation is zero from ever creating
                    // a 0-damage DamageEffect (which can still proc block meter).
                    if (GroupComplete(*group)) {
                        EmitCompletedTeamUpThrash(
                            *group,
                            model,
                            targetEntity
                        );
                    }
                } else if (sourceEntity &&
                           HasThrash(sourceEntity)) {
                    const int heroDamage =
                        std::max(
                            0,
                            damageAmount - healthBefore
                        );

                    // Resolve the destination relative to the Thrash source,
                    // not from the damaged fighter alone.
                    const int heroId =
                        ResolveOpposingHeroId(
                            sourceEntity,
                            targetEntity
                        );

                    // Zero overflow or unresolved opponent means NO synthetic
                    // effect is created or routed at all.
                    if (heroDamage > 0 && heroId >= 0) {
                        if (EmitThrashDamageEffect(
                                sourceEntity,
                                sourceId,
                                heroId,
                                laneId,
                                heroDamage,
                                effectGroupId)) {
                            
                        } else {
                            LOG_ERR(
                                "CustomComponents/Thrash: failed to route hero damage source=%d hero=%d damage=%d",
                                sourceId,
                                heroId,
                                heroDamage
                            );
                        }
                    }
                }
            }

            CleanupConsumedGroups();
        }

        inline bool RegisterComponent() {
            auto thrashClass =
                BNM::Class("PvZCards.Engine.Components", "Thrash");

            if (!thrashClass.IsValid()) {
                LOG_ERR(
                    "CustomComponents/Thrash: managed class was not registered"
                );
                return false;
            }

            if (!CustomComponentRegistry::FindByName(
                    "PvZCards.Engine.Components",
                    "Thrash"
                )) {
                if (!CustomComponentRegistry::Register({
                        "Thrash",
                        "PvZCards.Engine.Components",
                        "EngineLib.dll",
                        "GrantableAbility",
                        "PvZCards.Engine.Components",
                        "EngineLib.dll"
                    })) {
                    LOG_ERR(
                        "CustomComponents/Thrash: registry registration failed"
                    );
                    return false;
                }
            }

            auto* descriptor =
                CustomComponentRegistry::FindByName(
                    "PvZCards.Engine.Components",
                    "Thrash"
                );

            monoType = descriptor ? descriptor->monoType : nullptr;

            auto deepCopy =
                thrashClass.GetMethod("DeepCopyFrom", 2);
            auto poolReturn =
                thrashClass.GetMethod("HandleReturnToPool", 1);

            if (!monoType ||
                !deepCopy.IsValid() ||
                !poolReturn.IsValid()) {
                LOG_ERR(
                    "CustomComponents/Thrash: lifecycle initialization failed"
                );
                return false;
            }

            return true;
        }

        inline bool ResolveEngineApiAndInstallHooks() {
            auto entityClass =
                BNM::Class("PvZCards.Engine", "Entity");
            auto entityModelClass =
                BNM::Class("PvZCards.Engine", "EntityModel");

            auto damageListBuilder =
                BNM::Class(
                    "PvZCards.Core.Systems.Utilities",
                    "DamageListBuilder"
                );
            auto damageSystem =
                BNM::Class("PvZCards.Core.Systems", "DamageSystem");
            auto damageEffect =
                BNM::Class("PvZCards.Engine.Effects", "DamageEffect");
            auto effect =
                BNM::Class("PvZCards.Engine", "Effect");
            auto effectRouter =
                BNM::Class("PvZCards.Engine", "EffectRouter");
            auto instanceManager =
                BNM::Class("PvZCards.Engine", "InstanceManager");

            auto health =
                BNM::Class(
                    "PvZCards.Engine.Components",
                    "Health"
                );
            auto owner =
                BNM::Class(
                    "PvZCards.Engine.Components",
                    "Owner"
                );

            if (!entityClass.IsValid() ||
                !entityModelClass.IsValid() ||
                !damageListBuilder.IsValid() ||
                !damageSystem.IsValid() ||
                !damageEffect.IsValid() ||
                !effect.IsValid() ||
                !effectRouter.IsValid() ||
                !instanceManager.IsValid() ||
                !health.IsValid() ||
                !owner.IsValid()) {
                LOG_ERR(
                    "CustomComponents/Thrash: required gameplay classes were not found"
                );
                return false;
            }

            entityHasComponent =
                entityClass.GetMethod("HasComponent", 1);
            entityGetComponent =
                entityClass.GetMethod("GetComponent", 1);
            getEntityById =
                entityModelClass.GetMethod("GetEntityById", 1);

            auto createAndAddDamageEffect =
                damageListBuilder.GetMethod(
                    "CreateAndAddDamageEffect",
                    2
                );
            auto damageTarget =
                damageSystem.GetMethod("DamageTarget", 3);

            damageSystemEntities =
                damageSystem.GetField("<Entities>k__BackingField");

            damageEffectSourceId =
                effect.GetField("SourceId");
            damageEffectTargetId =
                effect.GetField("TargetId");
            damageEffectPlayerId =
                effect.GetField("PlayerId");
            damageEffectEffectGroupId =
                effect.GetField("EffectGroupId");
            damageEffectLaneId =
                damageEffect.GetField("LaneId");
            damageEffectShowParticleEffect =
                damageEffect.GetField("ShowParticleEffect");
            damageEffectSetDamageAmount =
                damageEffect.GetMethod("set_DamageAmount", 1);

            entityInstanceManager =
                entityClass.GetField("InstanceManager");
            damageEffectMonoType =
                damageEffect.GetMonoType();
            instanceManagerAcquire =
                instanceManager.GetMethod("Acquire", 2);
            instanceManagerReturn =
                instanceManager.GetMethod("Return", 1);

            auto effectRouterProcess =
                effectRouter.GetMethod("ProcessEffect", 1);

            healthCalculateHealth =
                health.GetMethod("CalculateHealth", 0);

            ownerMonoType = owner.GetMonoType();
            ownerIdField = owner.GetField("OwnerId");
            healthMonoType = health.GetMonoType();

            if (!entityHasComponent.IsValid() ||
                !entityGetComponent.IsValid() ||
                !getEntityById.IsValid() ||
                !createAndAddDamageEffect.IsValid() ||
                !damageTarget.IsValid() ||
                !damageSystemEntities.IsValid() ||
                !damageEffectSourceId.IsValid() ||
                !damageEffectTargetId.IsValid() ||
                !damageEffectPlayerId.IsValid() ||
                !damageEffectEffectGroupId.IsValid() ||
                !damageEffectLaneId.IsValid() ||
                !damageEffectShowParticleEffect.IsValid() ||
                !damageEffectSetDamageAmount.IsValid() ||
                !entityInstanceManager.IsValid() ||
                !damageEffectMonoType ||
                !instanceManagerAcquire.IsValid() ||
                !instanceManagerReturn.IsValid() ||
                !effectRouterProcess.IsValid() ||
                !healthCalculateHealth.IsValid() ||
                !ownerMonoType ||
                !ownerIdField.IsValid() ||
                !healthMonoType) {
                LOG_ERR(
                    "CustomComponents/Thrash: required gameplay API was not found"
                );
                return false;
            }

            BNM::BasicHook(
                createAndAddDamageEffect,
                CreateAndAddDamageEffectHook,
                originalCreateAndAddDamageEffect
            );

            BNM::BasicHook(
                effectRouterProcess,
                EffectRouterProcessHook,
                originalEffectRouterProcess
            );

            BNM::BasicHook(
                damageTarget,
                DamageTargetHook,
                originalDamageTarget
            );

            if (!originalCreateAndAddDamageEffect ||
                !originalEffectRouterProcess ||
                !originalDamageTarget) {
                LOG_ERR(
                    "CustomComponents/Thrash: gameplay hook installation failed"
                );
                return false;
            }

            
            return true;
        }

    } // namespace ThrashSystem

    inline bool InitializeThrash() {
        return ThrashSystem::RegisterComponent() &&
            ThrashSystem::ResolveEngineApiAndInstallHooks();
    }

} // namespace CustomComponents::Systems
