#pragma once
#include <limits>
#include <cstdint>
#include <string>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Utils.hpp>
#include "log.h"

namespace CustomComponents::NumericAbility {
    // Vanilla StatModOperation values. None/omitted means Add in this extension.
    enum class Operation : int { None = 0, Set = 1, Add = 2 };
    inline BNM::Class valueClass, modifierClass;
    inline BNM::Field<void*> instancesField, modifiersField;
    inline BNM::Field<int> originalAmount, additiveAmount, durationField, activationField;
    inline BNM::Field<BNM::Structures::Mono::String*> groupField;
    inline BNM::Method<void*> acquire;
    inline BNM::Method<void> addModifier, removeModifier;
    inline BNM::Method<int> calculate;

    inline bool Initialize() {
        valueClass = BNM::Class("PvZCards.Engine.Components.Data", "ModifiableInt");
        modifierClass = BNM::Class("PvZCards.Engine.Components.Data", "IntModifier");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto manager = BNM::Class("PvZCards.Engine", "InstanceManager");
        auto calculator = BNM::Class("PvZCards.Engine.Utilities", "ModifiableIntCalculator");
        if (!valueClass.IsValid() || !modifierClass.IsValid() || !entity.IsValid() ||
            !manager.IsValid() || !calculator.IsValid()) return false;
        instancesField = entity.GetField("InstanceManager");
        modifiersField = valueClass.GetField("Modifiers");
        originalAmount = modifierClass.GetField("OriginalAmount");
        additiveAmount = modifierClass.GetField("AdditiveAmount");
        durationField = modifierClass.GetField("Duration");
        activationField = modifierClass.GetField("ActivationTime");
        groupField = modifierClass.GetField("GroupId");
        acquire = manager.GetMethod("Acquire", 2);
        addModifier = valueClass.GetMethod("AddModifier", 1);
        removeModifier = valueClass.GetMethod("RemoveModifier", 2);
        calculate = calculator.GetMethod("CalculateValue", 1);
        return instancesField.IsValid() && modifiersField.IsValid() && originalAmount.IsValid() &&
            additiveAmount.IsValid() && durationField.IsValid() && activationField.IsValid() &&
            groupField.IsValid() && acquire.IsValid() && addModifier.IsValid() &&
            removeModifier.IsValid() && calculate.IsValid();
    }
    inline std::string Group(int ability, int source) {
        return "CustomComponents/GrantAbility/" + std::to_string(ability) + "/" + std::to_string(source);
    }
    inline void* NewValue() {
        auto ctor = valueClass.GetMethod(".ctor", 0).cast<void>();
        if (!ctor.IsValid()) return nullptr;
        auto* value = valueClass.CreateNewInstance();
        if (value) ctor[value]();
        return value;
    }
    inline int64_t Delta(int requested, int current, int baseOffset, Operation operation) {
        return operation == Operation::Set
            ? static_cast<int64_t>(requested) - current - baseOffset
            : static_cast<int64_t>(requested);
    }
    inline bool MatchesGroup(std::string_view text, int ability, int source, bool allSources) {
        if (!allSources) return text == Group(ability, source);
        return text.starts_with("CustomComponents/GrantAbility/" + std::to_string(ability) + "/");
    }
    inline bool Apply(void* entity, void* value, int ability, int source, int duration,
                      int requested, Operation operation, int baseOffset = 0) {
        if (!entity || !value) return false;
        auto* instances = instancesField[entity].Get();
        if (!instances) return false;
        const auto delta = Delta(requested, calculate(value), baseOffset, operation);
        if (delta < std::numeric_limits<int>::min() || delta > std::numeric_limits<int>::max()) {
            LOG_ERR("CustomComponents/GrantAbility: modifier overflow"); return false;
        }
        auto* modifier = acquire[instances](modifierClass.GetMonoType(), true);
        if (!modifier) return false;
        groupField[modifier].Set(BNM::CreateMonoString(Group(ability, source)));
        originalAmount[modifier].Set(requested);
        additiveAmount[modifier].Set(static_cast<int>(delta));
        durationField[modifier].Set(duration);
        activationField[modifier].Set(0);
        addModifier[value](modifier);
        return true;
    }
    // Reverse iteration allows native RemoveModifier to remove AND pool each entry.
    // GroupId is managed state: native deep copies preserve source identity.
    inline void Remove(void* entity, void* value, int ability, int duration,
                       int source = 0, bool allSources = false) {
        if (!entity || !value) return;
        auto* instances = instancesField[entity].Get();
        auto* list = modifiersField[value].Get();
        if (!instances || !list) return;
        BNM::Class listClass(static_cast<BNM::IL2CPP::Il2CppObject*>(list));
        auto count = listClass.GetMethod("get_Count", 0).cast<int>();
        auto item = listClass.GetMethod("get_Item", 1).cast<void*>();
        if (!count.IsValid() || !item.IsValid()) return;
        for (int i = count[list]() - 1; i >= 0; --i) {
            auto* modifier = item[list](i);
            if (!modifier || durationField[modifier].Get() != duration) continue;
            auto* group = groupField[modifier].Get();
            if (!group) continue;
            const auto text = group->str();
            if (MatchesGroup(text, ability, source, allSources))
                removeModifier[value](modifier, instances);
        }
    }
}
