#pragma once
#include <mutex>
#include <charconv>
#include <unordered_map>
#include <string>
#include <BNM/BasicMonoStructures.hpp>
#include "core/AbilityRegistry.hpp"
#include "systems/EnumSerializationSupport.hpp"
#include "systems/CustomFieldBridgeSystem.hpp"
#include "systems/NumericAbility.hpp"

namespace CustomComponents::GrantAbilitySerialization {
    using Operation = NumericAbility::Operation;
    inline std::mutex metadataMutex;
    // Native metadata contains no managed references. Clear at pool return and
    // overwrite on conversion/copy to prevent operations leaking between effects.
    inline std::unordered_map<void*, Operation> descriptors, effects;
    inline BNM::Class descriptorClass;
    inline BNM::Field<int> descriptorAbility;
    using PopulateFn = void (*)(void*, BNM::IL2CPP::Il2CppObject*, BNM::IL2CPP::Il2CppObject*, const void*);
    using ConvertFn = void* (*)(void*, void*, void*, const void*);
    using CopyFn = void (*)(void*, void*, void*, const void*);
    using PoolFn = void (*)(void*, void*, const void*);
    inline PopulateFn originalPopulate = nullptr;
    inline ConvertFn originalConvert = nullptr;
    inline CopyFn originalDescriptorCopy = nullptr, originalEffectCopy = nullptr;
    inline PoolFn originalDescriptorPool = nullptr, originalEffectPool = nullptr;
    inline Operation Get(std::unordered_map<void*, Operation>& map, void* key) {
        std::lock_guard lock(metadataMutex);
        const auto it = map.find(key); return it == map.end() ? Operation::Add : it->second;
    }
    inline void Put(std::unordered_map<void*, Operation>& map, void* key, Operation op) {
        std::lock_guard lock(metadataMutex);
        map.erase(key);
        if (key && op == Operation::Set) map[key] = op;
    }
    // SetObjectProperties is a shared/hot serializer path.  Keep the hook
    // invisible to stock descriptors unless our extension key is actually present.
    // The guard also prevents us from re-processing any nested/re-entrant call that
    // occurs while the original serializer is running.
    inline thread_local bool populateReentry = false;

    struct PopulateReentryGuard {
        bool& flag;
        explicit PopulateReentryGuard(bool& value) : flag(value) { flag = true; }
        ~PopulateReentryGuard() { flag = false; }
    };

    static void PopulateHook(void* self, BNM::IL2CPP::Il2CppObject* instance, BNM::IL2CPP::Il2CppObject* properties, const void* method) {
        if (!originalPopulate)
            return;

        if (populateReentry || !instance || !properties ||
            !BNM::IsA(static_cast<BNM::IL2CPP::Il2CppObject*>(instance), descriptorClass)) {
            originalPopulate(self, instance, properties, method);
            return;
        }

        BNM::Class dict(static_cast<BNM::IL2CPP::Il2CppObject*>(properties));
        auto contains = dict.GetMethod("ContainsKey", 1).cast<bool>();
        if (!contains.IsValid()) {
            originalPopulate(self, instance, properties, method);
            return;
        }

        // ModifyOperation is our discriminator.  Do not inspect GrantableAbilityType,
        // Duration, AbilityValue, or otherwise touch ordinary stock dictionaries.
        auto* operationKey = BNM::CreateMonoString("ModifyOperation");
        if (!contains[properties](operationKey)) {
            PopulateReentryGuard guard(populateReentry);
            originalPopulate(self, instance, properties, method);
            return;
        }

        auto item = dict.GetMethod("get_Item", 1).cast<void*>();
        auto remove = dict.GetMethod("Remove", 1).cast<bool>();
        auto cloneCtor = dict.GetMethod(".ctor", { std::string_view("dictionary") }).cast<void>();
        if (!item.IsValid() || !remove.IsValid() || !cloneCtor.IsValid()) {
            LOG_ERR("CustomComponents/GrantAbility: custom property dictionary API missing");
            PopulateReentryGuard guard(populateReentry);
            originalPopulate(self, instance, properties, method);
            return;
        }

        auto* abilityKey = BNM::CreateMonoString("GrantableAbilityType");

        const bool hasAbility = contains[properties](abilityKey);

        if (!hasAbility) {
            LOG_ERR("CustomComponents/GrantAbility: ModifyOperation present without GrantableAbilityType");
            PopulateReentryGuard guard(populateReentry);
            originalPopulate(self, instance, properties, method);
            return;
        }

        void* rawAbility = item[properties](abilityKey);

        const std::string name = EnumSerializationSupport::Text(rawAbility);

        const AbilityAdapter* adapter = AbilityRegistry::Find(name);

        if (!adapter && !name.empty()) {
            int id = -1;
            const auto parsed = std::from_chars(name.data(), name.data() + name.size(), id);
            if (parsed.ec == std::errc{} && parsed.ptr == name.data() + name.size())
                adapter = AbilityRegistry::Find(id);
        }

        void* rawOperation = item[properties](operationKey);

        const std::string operationText = EnumSerializationSupport::Text(rawOperation);

        Operation operation = Operation::Add;
        if (operationText == "Set" || operationText == "1") {
            operation = Operation::Set;
        } else if (operationText == "Add" || operationText == "2" ||
                   operationText == "None" || operationText == "0" ||
                   operationText.empty()) {
            operation = Operation::Add;
        } else {
            LOG_ERR(
                "CustomComponents/GrantAbility: invalid ModifyOperation '%s'",
                operationText.c_str()
            );
            PopulateReentryGuard guard(populateReentry);
            originalPopulate(self, instance, properties, method);
            return;
        }

        if (!adapter) {
            LOG_ERR(
                "CustomComponents/GrantAbility: ModifyOperation requires registered numeric ability (ability='%s')",
                name.c_str()
            );
            PopulateReentryGuard guard(populateReentry);
            originalPopulate(self, instance, properties, method);
            return;
        }

        // Do not mutate the caller's dictionary.  Strip only the two properties
        // that stock PvZ Heroes does not understand; Duration/AbilityValue remain
        // available to the normal serializer.
        auto* copy = dict.CreateNewInstance();
        if (!copy) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: dictionary allocation failed");
            return;
        }

        cloneCtor[copy](properties);
        remove[copy](abilityKey);
        remove[copy](operationKey);

        {
            PopulateReentryGuard guard(populateReentry);
            originalPopulate(self, instance, copy, method);
        }

        descriptorAbility[instance].Set(adapter->id);

        Put(descriptors, instance, operation);

    }

    static void* ConvertHook(void* self, void* instances, void* entity, const void* method) {
        auto* result = originalConvert(self, instances, entity, method);
        Put(effects, result, Get(descriptors, self)); return result;
    }
    static void DescriptorCopyHook(void* self, void* source, void* instances, const void* method) {
        const auto op = Get(descriptors, source);
        originalDescriptorCopy(self, source, instances, method); Put(descriptors, self, op);
    }
    static void EffectCopyHook(void* self, void* source, void* instances, const void* method) {
        const auto op = Get(effects, source);
        originalEffectCopy(self, source, instances, method); Put(effects, self, op);
    }
    static void DescriptorPoolHook(void* self, void* instances, const void* method) {
        Put(descriptors, self, Operation::Add); originalDescriptorPool(self, instances, method);
    }
    static void EffectPoolHook(void* self, void* instances, const void* method) {
        Put(effects, self, Operation::Add); originalEffectPool(self, instances, method);
    }
    inline bool Initialize() {

        descriptorClass = BNM::Class(
            "PvZCards.Engine.Components",
            "GrantAbilityEffectDescriptor"
        );
        auto effect = BNM::Class(
            "PvZCards.Engine.Effects",
            "GrantAbilityEffect"
        );
        auto serializer = BNM::Class(
            "PvZCards.Core.Serialization",
            "ReferenceTypeDeserializer"
        );
        auto grantableAbilityEnum = BNM::Class(
            "PvZCards.Engine.Components",
            "GrantableAbilityType"
        );

        auto targetScopeSortValueEnum = BNM::Class(
            "PvZCards.Engine.Components",
            "TargetScopeSortValue"
        );

#define SERIAL_CLASS_CHECK(label, value) do { \
            if (!(value).IsValid()) { \
                LOG_ERR("CustomComponents/GrantAbility/Serialization: class FAILED: " label); \
                return false; \
            } \
        } while (0)

        SERIAL_CLASS_CHECK("GrantAbilityEffectDescriptor", descriptorClass);
        SERIAL_CLASS_CHECK("GrantAbilityEffect", effect);
        SERIAL_CLASS_CHECK("ReferenceTypeDeserializer", serializer);
        SERIAL_CLASS_CHECK("GrantableAbilityType", grantableAbilityEnum);
        SERIAL_CLASS_CHECK("TargetScopeSortValue", targetScopeSortValueEnum);
        auto* grantableAbilityEnumType = grantableAbilityEnum.GetMonoType();
        if (!grantableAbilityEnumType) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: GrantableAbilityType MonoType FAILED");
            return false;
        }

        auto* targetScopeSortValueEnumType = targetScopeSortValueEnum.GetMonoType();
        if (!targetScopeSortValueEnumType) {
            LOG_ERR("CustomComponents/TargetScope: TargetScopeSortValue MonoType FAILED");
            return false;
        }

        bool enumRegistrationOk = true;
        AbilityRegistry::ForEach([&](const AbilityDescriptor& ability) {
            if (!EnumSerializationSupport::Register(
                    grantableAbilityEnumType,
                    ability.name,
                    ability.id)) {
                enumRegistrationOk = false;
            }
        });
        enumRegistrationOk =
            EnumSerializationSupport::Register(
                targetScopeSortValueEnumType,
                "ListPosition",
                3
            ) && enumRegistrationOk;

        if (!enumRegistrationOk) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: custom enum registration failed");
            return false;
        }

        LOG_INFO("CustomComponents/GrantAbility/Serialization: enum registrations complete");
        if (!EnumSerializationSupport::Initialize()) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: EnumSerializationSupport::Initialize failed");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: enum hook ready");

#undef SERIAL_CLASS_CHECK

        descriptorAbility =
            descriptorClass.GetField("GrantableAbilityType");

        auto populate =
            serializer.GetMethod("SetObjectProperties", 2);
        auto convert =
            descriptorClass.GetMethod("ToEffect", 2);
        auto descriptorCopy =
            descriptorClass.GetMethod("DeepCopyFrom", 2);
        auto effectCopy =
            effect.GetMethod("DeepCopyFrom", 2);
        auto descriptorPool =
            descriptorClass.GetMethod("HandleReturnToPool", 1);
        auto effectPool =
            effect.GetMethod("HandleReturnToPool", 1);

#define SERIAL_LOOKUP_CHECK(label, value) do { \
            if (!(value).IsValid()) { \
                LOG_ERR("CustomComponents/GrantAbility/Serialization: lookup FAILED: " label); \
                return false; \
            } \
        } while (0)

        SERIAL_LOOKUP_CHECK(
            "GrantAbilityEffectDescriptor.GrantableAbilityType",
            descriptorAbility
        );
        SERIAL_LOOKUP_CHECK(
            "ReferenceTypeDeserializer.SetObjectProperties(object, IDictionary)",
            populate
        );

        SERIAL_LOOKUP_CHECK(
            "GrantAbilityEffectDescriptor.ToEffect(InstanceManager, Entity)",
            convert
        );
        SERIAL_LOOKUP_CHECK(
            "GrantAbilityEffectDescriptor.DeepCopyFrom(object, InstanceManager)",
            descriptorCopy
        );
        SERIAL_LOOKUP_CHECK(
            "GrantAbilityEffect.DeepCopyFrom(object, InstanceManager)",
            effectCopy
        );
        SERIAL_LOOKUP_CHECK(
            "GrantAbilityEffectDescriptor.HandleReturnToPool(InstanceManager)",
            descriptorPool
        );
        SERIAL_LOOKUP_CHECK(
            "GrantAbilityEffect.HandleReturnToPool(InstanceManager)",
            effectPool
        );

#undef SERIAL_LOOKUP_CHECK

        // CustomFieldBridgeSystem already owns SetObjectProperties.
        // Do not install another BNM native hook at the same address.
        originalPopulate = CustomComponents::Systems::CustomFieldBridgeSystem::GetOriginalSetObjectProperties();
        if (!originalPopulate) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: shared SetObjectProperties owner unavailable");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: sharing SetObjectProperties hook with CustomFieldBridge");

        BNM::BasicHook(
            convert,
            ConvertHook,
            originalConvert
        );

        if (!originalConvert) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: HOOK FAILED ToEffect");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: HOOK OK ToEffect");

        BNM::BasicHook(
            descriptorCopy,
            DescriptorCopyHook,
            originalDescriptorCopy
        );

        if (!originalDescriptorCopy) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: HOOK FAILED Descriptor.DeepCopyFrom");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: HOOK OK Descriptor.DeepCopyFrom");

        BNM::BasicHook(
            effectCopy,
            EffectCopyHook,
            originalEffectCopy
        );

        if (!originalEffectCopy) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: HOOK FAILED Effect.DeepCopyFrom");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: HOOK OK Effect.DeepCopyFrom");

        BNM::BasicHook(
            descriptorPool,
            DescriptorPoolHook,
            originalDescriptorPool
        );

        if (!originalDescriptorPool) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: HOOK FAILED Descriptor.HandleReturnToPool");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: HOOK OK Descriptor.HandleReturnToPool");

        BNM::BasicHook(
            effectPool,
            EffectPoolHook,
            originalEffectPool
        );

        if (!originalEffectPool) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: HOOK FAILED Effect.HandleReturnToPool");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: HOOK OK Effect.HandleReturnToPool");

        if (!CustomComponents::Systems::CustomFieldBridgeSystem::RegisterSetObjectPropertiesDelegate(PopulateHook)) {
            LOG_ERR("CustomComponents/GrantAbility/Serialization: shared SetObjectProperties delegate registration FAILED");
            return false;
        }
        LOG_INFO("CustomComponents/GrantAbility/Serialization: shared SetObjectProperties delegate registered");
        LOG_INFO("CustomComponents/GrantAbility/Serialization: READY");
        return true;
    }
}
