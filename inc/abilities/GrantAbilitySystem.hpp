#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
#include "core/AbilityRegistry.hpp"
#include "abilities/AbilityDefinitions.hpp"
#include "abilities/GrantAbilityPresentation.hpp"
#include "abilities/GrantAbilitySerialization.hpp"
#include "abilities/GrantableAbilityRuntime.hpp"
#include "presentation/ZoneTriggeredAbilityPresentationSystem.hpp"
namespace CustomComponents::GrantAbility {
    // Ability-specific storage, names and visuals are registered before this
    // system starts. This file owns only the generic grant/remove lifecycle.
    using ProcessFn = void (*)(void*, void*, const void*);
    inline ProcessFn originalGrant = nullptr, originalRemove = nullptr, originalTurnEnd = nullptr;
    using AttemptFn = void (*)(void*, const void*);
    inline AttemptFn originalNextFighter = nullptr;

    inline BNM::Field<int> sourceId, targetId, grantType, grantDuration, removeType, removeDuration;
    inline BNM::Field<void*> grantModel, grantHistory, removeModel, removeRemover, turnFinder, turnRemover, nextFinder, nextRemover, finderModel;
    inline BNM::Field<void*> removerHistory;
    inline BNM::Method<void*> entityById, allEntities;
    inline BNM::Method<int> entityId;
    inline BNM::Method<int> abilityValue;
    inline BNM::Method<void*> historyAddRecord;
    inline BNM::Method<void> recordSetDefaults, recordSetAbilityType, recordSetAbilityValue;
    inline BNM::MonoType* abilityGrantedRecordType = nullptr;
    inline BNM::MonoType* abilityRemovedRecordType = nullptr;
    inline BNM::Method<void> removedRecordSetTargetId, removedRecordSetAbilityType;

    static void GrantHookImpl(void* self, void* effect, const void* method) {
        auto* adapter = effect ? AbilityRegistry::Find(grantType[effect].Get()) : nullptr;
        if (!adapter) { originalGrant(self, effect, method); return; }
        auto* model = grantModel[self].Get();
        auto* target = model ? entityById[model](targetId[effect].Get()) : nullptr;
        const int duration = grantDuration[effect].Get();
        if (duration < 0 || duration > 2) {
            LOG_ERR("CustomComponents/GrantAbility: unsupported duration %d", duration); return;
        }
        bool applied = false;
        const int source = sourceId[effect].Get();
        const int value = abilityValue[effect]();
        const auto operation = GrantAbilitySerialization::Get(
            GrantAbilitySerialization::effects, effect);
        if (target) {
            if (adapter->storage == AbilityStorage::Numeric) {
                applied = NumericAbility::Apply(
                    target, adapter->numericValue(target, true), adapter->id,
                    source, duration, value, operation);
            } else {
                applied = GrantableAbilityRuntime::Apply(
                    adapter->component(target, true), duration, source, value, operation);
            }
        }
        if (!applied) {
            LOG_ERR("CustomComponents/GrantAbility: failed to apply %s", adapter->name); return;
        }
        // Stock GrantAbilitySystem also publishes an AbilityGrantedRecord into History.
        // Our custom path bypasses stock ProcessEffect, so recreate that record here.  The
        // Game-side AbilityGrantedRecordProcessor is what normally drives grant visuals.
        auto* history = grantHistory[self].Get();
        if (history && abilityGrantedRecordType && historyAddRecord.IsValid()) {
            auto* record = historyAddRecord[history](abilityGrantedRecordType);
            if (record) {
                if (recordSetDefaults.IsValid()) recordSetDefaults[record](effect);
                if (recordSetAbilityType.IsValid()) recordSetAbilityType[record](adapter->id);
                if (recordSetAbilityValue.IsValid()) recordSetAbilityValue[record](abilityValue[effect]());
            }
            else {
                LOG_ERR("CustomComponents/GrantAbility/History: History.AddRecord(AbilityGrantedRecord) returned null");
            }
        }
        else {
            LOG_ERR("CustomComponents/GrantAbility/History: unavailable history=%p recordType=%p addRecord=%d",
                history, abilityGrantedRecordType, historyAddRecord.IsValid() ? 1 : 0);
        }

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG("CustomComponents/GrantAbility: %s value=%d duration=%d source=%d target=%d",
            adapter->name, abilityValue[effect](), duration, sourceId[effect].Get(), targetId[effect].Get());
    }
    static void GrantHook(void* self, void* effect, const void* method) {
        Systems::ZoneTriggeredAbilityPresentationSystem::AuditOwnedEffect(2, self,
            static_cast<Systems::ZoneTriggeredAbilityPresentationSystem::Obj*>(effect),
            [&] { GrantHookImpl(self, effect, method); });
    }

    // Stock GrantableAbilityRemover::RecordAbilityRemoval creates an AbilityRemovedRecord,
    // stores TargetId (RecordKey 1) and GrantableAbilityType (RecordKey 22), then History
    // dispatches it. Custom NumericAbility removal bypasses that path, so reproduce the
    // same record shape here. This is the removal-side counterpart to AbilityGrantedRecord.
    inline void PublishAbilityRemoved(void* remover, void* entity, int abilityType) {
        auto* history = remover && removerHistory.IsValid() ? removerHistory[remover].Get() : nullptr;
        if (!history || !entity || !abilityRemovedRecordType || !historyAddRecord.IsValid() ||
            !entityId.IsValid() || !removedRecordSetTargetId.IsValid() || !removedRecordSetAbilityType.IsValid()) {
            LOG_ERR("CustomComponents/GrantAbility/RemoveHistory: unavailable remover=%p history=%p entity=%p recordType=%p",
                remover, history, entity, abilityRemovedRecordType);
            return;
        }
        auto* record = historyAddRecord[history](abilityRemovedRecordType);
        if (!record) {
            LOG_ERR("CustomComponents/GrantAbility/RemoveHistory: History.AddRecord(AbilityRemovedRecord) returned null");
            return;
        }
        const int id = entityId[entity]();
        removedRecordSetTargetId[record](1, id);       // RecordKey::TargetId
        removedRecordSetAbilityType[record](22, abilityType); // RecordKey::GrantableAbilityType
    }

    inline bool RemoveCustomAbility(void* remover, void* entity, const AbilityAdapter& adapter,
        int duration, int source, bool expiry = false) {
        if (!entity) return false;

        bool before = false;
        bool after = false;
        if (adapter.storage == AbilityStorage::Numeric) {
            auto* value = adapter.numericValue(entity, false);
            before = value && NumericAbility::calculate(value) > 0;
            NumericAbility::Remove(entity, value, adapter.id, duration, source, expiry);
            value = adapter.numericValue(entity, false);
            after = value && NumericAbility::calculate(value) > 0;
        } else {
            auto* component = adapter.component(entity, false);
            before = GrantableAbilityRuntime::Active(component);
            GrantableAbilityRuntime::Remove(component, duration, source, expiry);
            after = GrantableAbilityRuntime::Active(component);
        }

        if (before && !after) {
            PublishAbilityRemoved(remover, entity, adapter.id);
            return true;
        }
        return false;
    }

    static void RemoveHookImpl(void* self, void* effect, const void* method) {
        auto* adapter = effect ? AbilityRegistry::Find(removeType[effect].Get()) : nullptr;
        if (!adapter) { originalRemove(self, effect, method); return; }
        auto* model = removeModel[self].Get();
        auto* target = model ? entityById[model](targetId[effect].Get()) : nullptr;
        auto* remover = removeRemover.IsValid() ? removeRemover[self].Get() : nullptr;
        if (target) RemoveCustomAbility(remover, target, *adapter,
            removeDuration[effect].Get(), sourceId[effect].Get());
    }

    static void RemoveHook(void* self, void* effect, const void* method) {
        Systems::ZoneTriggeredAbilityPresentationSystem::AuditOwnedEffect(3, self,
            static_cast<Systems::ZoneTriggeredAbilityPresentationSystem::Obj*>(effect),
            [&] { RemoveHookImpl(self, effect, method); });
    }

    inline void Expire(void* finder, void* remover, int duration) {
        if (!finder) return;
        auto* model = finderModel[finder].Get();
        auto* list = model ? allEntities[model]() : nullptr;
        if (!list) return;
        BNM::Class klass(static_cast<BNM::IL2CPP::Il2CppObject*>(list));
        auto count = klass.GetMethod("get_Count", 0).cast<int>();
        auto item = klass.GetMethod("get_Item", 1).cast<void*>();
        if (!count.IsValid() || !item.IsValid()) return;
        for (int i = 0, n = count[list](); i < n; ++i) {
            auto* entity = item[list](i);
            AbilityRegistry::ForEach([&](const AbilityAdapter& adapter) {
                RemoveCustomAbility(remover, entity, adapter, duration, 0, true);
            });
        }
    }

    static void TurnEndHook(void* self, void* effect, const void* method) {
        originalTurnEnd(self, effect, method); Expire(turnFinder[self].Get(), turnRemover[self].Get(), 1);
    }
    static void NextFighterHook(void* self, const void* method) {
        originalNextFighter(self, method); Expire(nextFinder[self].Get(), nextRemover[self].Get(), 2);
    }

    inline bool Initialize() {
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto finder = BNM::Class("PvZCards.Engine", "EntityFinder");
        auto grant = BNM::Class("PvZCards.Engine.Effects", "GrantAbilityEffect");
        auto remove = BNM::Class("PvZCards.Engine.Effects", "RemoveGrantAbilityEffect");
        auto gs = BNM::Class("PvZCards.Core.Systems", "GrantAbilitySystem");
        auto history = BNM::Class("PvZCards.Engine", "History");
        auto effectRecord = BNM::Class("PvZCards.Engine.Records", "EffectRecord");
        auto abilityGrantedRecord = BNM::Class("PvZCards.Engine.Records", "AbilityGrantedRecord");
        auto abilityRemovedRecord = BNM::Class("PvZCards.Engine.Records", "AbilityRemovedRecord");
        auto record = BNM::Class("PvZCards.Engine", "Record");
        auto grantableAbilityType = BNM::Class("PvZCards.Engine.Components", "GrantableAbilityType");
        auto systemInt32 = BNM::Class("System", "Int32");
        auto rs = BNM::Class("PvZCards.Core.Systems", "RemoveGrantAbilitySystem");
        auto ts = BNM::Class("PvZCards.Core.Systems", "EndOfTurnAbilityRemovalSystem");
        auto ns = BNM::Class("PvZCards.Core.Systems", "NextFighterAbilityRemovalSystem");
        auto grantableAbilityRemover = BNM::Class("PvZCards.Core.Systems", "GrantableAbilityRemover");
        CC_DIAG("CustomComponents/GrantAbility/INIT: entered");
#define GRANT_CHECK_CLASS(v) if (!(v).IsValid()) LOG_ERR("CustomComponents/GrantAbility/INIT: missing class " #v);
        GRANT_CHECK_CLASS(effect); GRANT_CHECK_CLASS(entity); GRANT_CHECK_CLASS(model);
        GRANT_CHECK_CLASS(finder); GRANT_CHECK_CLASS(grant); GRANT_CHECK_CLASS(remove);
        GRANT_CHECK_CLASS(gs); GRANT_CHECK_CLASS(history); GRANT_CHECK_CLASS(effectRecord);
        GRANT_CHECK_CLASS(abilityGrantedRecord); GRANT_CHECK_CLASS(abilityRemovedRecord);
        GRANT_CHECK_CLASS(record); GRANT_CHECK_CLASS(grantableAbilityType);
        GRANT_CHECK_CLASS(systemInt32); GRANT_CHECK_CLASS(rs); GRANT_CHECK_CLASS(ts);
        GRANT_CHECK_CLASS(ns); GRANT_CHECK_CLASS(grantableAbilityRemover);
#undef GRANT_CHECK_CLASS
        if (!effect.IsValid() || !entity.IsValid() || !model.IsValid() || !finder.IsValid() ||
            !grant.IsValid() || !remove.IsValid() || !gs.IsValid() || !history.IsValid() ||
            !effectRecord.IsValid() || !abilityGrantedRecord.IsValid() || !abilityRemovedRecord.IsValid() ||
            !record.IsValid() || !grantableAbilityType.IsValid() || !systemInt32.IsValid() || !rs.IsValid() ||
            !ts.IsValid() || !ns.IsValid() || !grantableAbilityRemover.IsValid()) return false;
        sourceId = effect.GetField("SourceId"); targetId = effect.GetField("TargetId");
        grantType = grant.GetField("GrantableAbilityType"); grantDuration = grant.GetField("Duration");
        removeType = remove.GetField("GrantableAbilityType"); removeDuration = remove.GetField("Duration");
        abilityValue = grant.GetMethod("get_AbilityValue", 0);
        grantModel = gs.GetField("<Entities>k__BackingField");
        grantHistory = gs.GetField("<History>k__BackingField");
        removeModel = rs.GetField("<Model>k__BackingField");
        removeRemover = rs.GetField("<GrantableAbilityRemover>k__BackingField");
        removerHistory = grantableAbilityRemover.GetField("<History>k__BackingField");
        historyAddRecord = history.GetMethod("AddRecord", 1);
        recordSetDefaults = effectRecord.GetMethod("SetDefaultIdsFromEffect", 1);
        recordSetAbilityType = abilityGrantedRecord.GetMethod("set_AbilityType", 1);
        recordSetAbilityValue = abilityGrantedRecord.GetMethod("set_AbilityValue", 1);
        abilityGrantedRecordType = abilityGrantedRecord.GetMonoType();
        abilityRemovedRecordType = abilityRemovedRecord.GetMonoType();
        auto recordSetValue = record.GetMethod("SetValue", 2);
        if (!recordSetValue.IsValid()) recordSetValue = record.GetMethod("SetValue", 1);
        if (recordSetValue.IsValid()) {
            removedRecordSetTargetId = recordSetValue.GetGeneric({systemInt32.GetCompileTimeClass()}).cast<void>();
            removedRecordSetAbilityType = recordSetValue.GetGeneric({grantableAbilityType.GetCompileTimeClass()}).cast<void>();
        }
        turnFinder = ts.GetField("<EntityFinder>k__BackingField");
        turnRemover = ts.GetField("<GrantableAbilityRemover>k__BackingField");
        nextFinder = ns.GetField("<EntityFinder>k__BackingField");
        nextRemover = ns.GetField("<GrantableAbilityRemover>k__BackingField");
        finderModel = finder.GetField("Model"); allEntities = model.GetMethod("GetAllEntities", 0);
        entityById = model.GetMethod("GetEntityById", 1);
        entityId = entity.GetMethod("get_Id", 0);
        auto gp = gs.GetMethod("ProcessEffect", 1), rp = rs.GetMethod("ProcessEffect", 1);
        auto tp = ts.GetMethod("ProcessEffect", 1), np = ns.GetMethod("AttemptEffectRemoval", 0);
#define GRANT_CHECK_LOOKUP(v) if (!(v).IsValid()) LOG_ERR("CustomComponents/GrantAbility/INIT: missing lookup " #v);
        GRANT_CHECK_LOOKUP(sourceId); GRANT_CHECK_LOOKUP(targetId);
        GRANT_CHECK_LOOKUP(grantType); GRANT_CHECK_LOOKUP(grantDuration);
        GRANT_CHECK_LOOKUP(removeType); GRANT_CHECK_LOOKUP(removeDuration);
        GRANT_CHECK_LOOKUP(abilityValue); GRANT_CHECK_LOOKUP(grantModel);
        GRANT_CHECK_LOOKUP(grantHistory); GRANT_CHECK_LOOKUP(historyAddRecord);
        GRANT_CHECK_LOOKUP(recordSetDefaults); GRANT_CHECK_LOOKUP(recordSetAbilityType);
        GRANT_CHECK_LOOKUP(recordSetAbilityValue);
        if (!abilityGrantedRecordType) LOG_ERR("CustomComponents/GrantAbility/INIT: missing abilityGrantedRecordType");
        if (!abilityRemovedRecordType) LOG_ERR("CustomComponents/GrantAbility/INIT: missing abilityRemovedRecordType");
        GRANT_CHECK_LOOKUP(removedRecordSetTargetId);
        GRANT_CHECK_LOOKUP(removedRecordSetAbilityType);
        GRANT_CHECK_LOOKUP(removeModel); GRANT_CHECK_LOOKUP(removeRemover);
        GRANT_CHECK_LOOKUP(removerHistory); GRANT_CHECK_LOOKUP(turnFinder);
        GRANT_CHECK_LOOKUP(turnRemover); GRANT_CHECK_LOOKUP(nextFinder);
        GRANT_CHECK_LOOKUP(nextRemover); GRANT_CHECK_LOOKUP(entityId);
        GRANT_CHECK_LOOKUP(finderModel); GRANT_CHECK_LOOKUP(allEntities);
        GRANT_CHECK_LOOKUP(entityById);
        GRANT_CHECK_LOOKUP(gp); GRANT_CHECK_LOOKUP(rp);
        GRANT_CHECK_LOOKUP(tp); GRANT_CHECK_LOOKUP(np);
#undef GRANT_CHECK_LOOKUP
        if (!sourceId.IsValid() || !targetId.IsValid() || !grantType.IsValid() || !grantDuration.IsValid() ||
            !removeType.IsValid() || !removeDuration.IsValid() || !abilityValue.IsValid() ||
            !grantModel.IsValid() || !grantHistory.IsValid() || !historyAddRecord.IsValid() ||
            !recordSetDefaults.IsValid() || !recordSetAbilityType.IsValid() || !recordSetAbilityValue.IsValid() ||
            !abilityGrantedRecordType || !abilityRemovedRecordType || !removedRecordSetTargetId.IsValid() ||
            !removedRecordSetAbilityType.IsValid() || !removeModel.IsValid() || !removeRemover.IsValid() ||
            !removerHistory.IsValid() || !turnFinder.IsValid() || !turnRemover.IsValid() ||
            !nextFinder.IsValid() || !nextRemover.IsValid() || !entityId.IsValid() ||
            !finderModel.IsValid() || !allEntities.IsValid() || !entityById.IsValid() || !gp.IsValid() ||
            !rp.IsValid() || !tp.IsValid() || !np.IsValid()) return false;
        CC_DIAG("CustomComponents/GrantAbility/INIT: core metadata resolved");
        if (!GrantAbilitySerialization::Initialize()) {
            LOG_ERR("CustomComponents/GrantAbility/INIT: serialization FAILED");
            return false;
        }
        CC_DIAG("CustomComponents/GrantAbility/INIT: serialization ready");
        if (!GrantAbilityPresentation::Initialize()) {
            LOG_ERR("CustomComponents/GrantAbility/INIT: presentation FAILED");
            return false;
        }
        CC_DIAG("CustomComponents/GrantAbility/INIT: presentation ready");

        // Enable GrantAbilitySystem + RemoveGrantAbilitySystem and both duration-removal systems.
        BNM::BasicHook(gp, GrantHook, originalGrant);
        if (!originalGrant) {
            LOG_ERR("CustomComponents/GrantAbility: GrantAbilitySystem::ProcessEffect hook failed");
            return false;
        }

        BNM::BasicHook(rp, RemoveHook, originalRemove);
        if (!originalRemove) {
            LOG_ERR("CustomComponents/GrantAbility: RemoveGrantAbilitySystem::ProcessEffect hook failed");
            return false;
        }

        BNM::BasicHook(tp, TurnEndHook, originalTurnEnd);
        if (!originalTurnEnd) {
            LOG_ERR("CustomComponents/GrantAbility: EndOfTurnAbilityRemovalSystem::ProcessEffect hook failed");
            return false;
        }

        BNM::BasicHook(np, NextFighterHook, originalNextFighter);
        if (!originalNextFighter) {
            LOG_ERR("CustomComponents/GrantAbility: NextFighterAbilityRemovalSystem::AttemptEffectRemoval hook failed");
            return false;
        }

        CC_DIAG(
            "CustomComponents/GrantAbility: registry-driven Grant + Remove + "
            "EndOfTurn(duration=1) + NextFighter(duration=2) hooks ENABLED"
        );
        return true;
    }
}
