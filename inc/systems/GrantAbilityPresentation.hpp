#pragma once

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>

#include "core/AbilityRegistry.hpp"
#include "systems/NumericAbility.hpp"
#include "systems/GrantableAbilityRuntime.hpp"
#include "log.h"

namespace CustomComponents::GrantAbilityPresentation {
    using AddSpecialAbilitiesFn = void (*)(void*, void*, void*, const void*);
    using GetSpriteTypeFn = int (*)(int, const void*);

    inline AddSpecialAbilitiesFn originalAddSpecialAbilities = nullptr;
    inline GetSpriteTypeFn originalGetSpriteType = nullptr;

    static void AddSpecialAbilitiesHook(void* self, void* abilities, void* entity, const void* method) {
        originalAddSpecialAbilities(self, abilities, entity, method);
        if (!abilities || !entity) return;

        BNM::Class listClass(static_cast<BNM::IL2CPP::Il2CppObject*>(abilities));
        auto contains = listClass.GetMethod("Contains", 1).cast<bool>();
        auto add = listClass.GetMethod("Add", 1).cast<void>();
        if (!contains.IsValid() || !add.IsValid()) return;

        AbilityRegistry::ForEach([&](const AbilityDescriptor& ability) {
            if (ability.specialAbilityType == AbilityDescriptor::NoSpecialAbilityType) return;
            bool active = false;
            if (ability.storage == AbilityStorage::Numeric) {
                auto* value = ability.numericValue(entity, false);
                active = value && NumericAbility::calculate(value) > 0;
            } else {
                auto* component = ability.component(entity, false);
                active = GrantableAbilityRuntime::Active(component);
            }
            if (!active) return;
            if (!contains[abilities](ability.specialAbilityType))
                add[abilities](ability.specialAbilityType);
        });
    }

    static int GetSpriteTypeHook(int abilityType, const void* method) {
        if (const auto* ability = AbilityRegistry::Find(abilityType);
            ability && ability->specialAbilityType != AbilityDescriptor::NoSpecialAbilityType)
            return ability->specialAbilityType;
        return originalGetSpriteType(abilityType, method);
    }

    inline bool Initialize() {
        auto checker = BNM::Class("PvZCards.Core.Systems.Utilities", "SpecialAbilityChecker");
        auto icons = BNM::Class("PvZCards.Game", "CardViewIconUtils");
        if (!checker.IsValid() || !icons.IsValid()) {
            LOG_ERR("CustomComponents/GrantAbility/Presentation: classes checker=%d icons=%d", (int)checker.IsValid(), (int)icons.IsValid());
            return false;
        }

        auto addSpecialAbilities = checker.GetMethod("AddSpecialAbilities", 2);
        auto getSpriteType = icons.GetMethod("GetSpriteTypeFromGrantableAbilityType", 1);
        if (!addSpecialAbilities.IsValid() || !getSpriteType.IsValid()) {
            LOG_ERR("CustomComponents/GrantAbility/Presentation: methods add=%d sprite=%d", (int)addSpecialAbilities.IsValid(), (int)getSpriteType.IsValid());
            return false;
        }

        BNM::BasicHook(addSpecialAbilities, AddSpecialAbilitiesHook, originalAddSpecialAbilities);
        BNM::BasicHook(getSpriteType, GetSpriteTypeHook, originalGetSpriteType);
        if (!originalAddSpecialAbilities || !originalGetSpriteType) {
            LOG_ERR("CustomComponents/GrantAbility/Presentation: hooks add=%d sprite=%d", (int)(originalAddSpecialAbilities != nullptr), (int)(originalGetSpriteType != nullptr));
            return false;
        }

        LOG_INFO("CustomComponents/GrantAbility/Presentation: registry-driven visual bridges ready");
        return true;
    }
}
