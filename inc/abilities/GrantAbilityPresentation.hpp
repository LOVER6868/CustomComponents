#include "core/HookRegistry.hpp"
#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>

#include "core/AbilityRegistry.hpp"
#include "abilities/NumericAbility.hpp"
#include "abilities/GrantableAbilityRuntime.hpp"
#include "log.h"

namespace CustomComponents::GrantAbilityPresentation {
    using AddSpecialAbilitiesFn = void (*)(void*, void*, void*, const void*);
    using GetSpriteTypeFn = int (*)(int, const void*);

    inline AddSpecialAbilitiesFn originalAddSpecialAbilities = nullptr;
    inline GetSpriteTypeFn originalGetSpriteType = nullptr;
    // An AbilityGrantedRecord is legitimate simulation/history data even if
    // its target has no frontend view (for example a hand/deck card granted
    // Withering). The stock processor blindly resolves AbilityGrantableTarget
    // and throws a ViewMapException, aborting all later records/Pick3 submits.
    using ProcessRecordFn=void(*)(void*,BNM::IL2CPP::Il2CppObject*,const void*);
    inline ProcessRecordFn originalAbilityGrantedRecord=nullptr;
    static void AbilityGrantedRecordGuard(void* self,BNM::IL2CPP::Il2CppObject* record,const void* method) {
        using Obj=BNM::IL2CPP::Il2CppObject;
        static auto mapperField=BNM::Class("PvZCards.Game","AbilityGrantedRecordProcessor")
            .GetField("<EntityViewMapper>k__BackingField").cast<Obj*>();
        static auto targetId=BNM::Class("PvZCards.Engine","Record")
            .GetMethod("GetEntityId",1).cast<int>();
        static auto find=BNM::Class("PvZCards.Game","EntityViewMapper")
            .GetMethod("GetViewObject",{std::string_view("entityId")}).cast<Obj*>();
        static auto interface=BNM::Class("PvZCards.Game","AbilityGrantableTarget");
        Obj* mapper=mapperField.IsValid()?mapperField[static_cast<Obj*>(self)].Get():nullptr;
        int id=-1;
        Obj* view=nullptr;
        auto ex=BNM::TryInvoke([&]{
            if(record && targetId.IsValid())id=targetId[record](1);
            if(mapper && find.IsValid() && id>=0)view=find[mapper](id);
        });
        if(id<0 || !view || ex.IsValid() || (interface.IsValid() && !BNM::IsA(view,interface))){
            static std::atomic<unsigned> samples{0};
            if(samples.fetch_add(1)<60)
                LOG_INFO("CustomComponents/GrantAbility/FRONTEND_SKIP_UNMAPPED: target=%d view=%p mapper=%p lookupFailed=%d",id,view,mapper,int(ex.IsValid()));
            return; // Skip animation ONLY; gameplay state/history is preserved.
        }
        originalAbilityGrantedRecord(self,record,method);
    }

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

        CustomComponents::Core::Hooks::Install(addSpecialAbilities, AddSpecialAbilitiesHook, originalAddSpecialAbilities);
        CustomComponents::Core::Hooks::Install(getSpriteType, GetSpriteTypeHook, originalGetSpriteType);
        const bool recordReady=CustomComponents::Core::Hooks::Install(
            BNM::Class("PvZCards.Game","AbilityGrantedRecordProcessor").GetMethod("ProcessRecord",1),
            AbilityGrantedRecordGuard,originalAbilityGrantedRecord);
        if (!originalAddSpecialAbilities || !originalGetSpriteType || !recordReady) {
            LOG_ERR("CustomComponents/GrantAbility/Presentation: hooks add=%d sprite=%d", (int)(originalAddSpecialAbilities != nullptr), (int)(originalGetSpriteType != nullptr));
            return false;
        }

        CC_DIAG("CustomComponents/GrantAbility/Presentation: registry-driven visual bridges ready");
        return true;
    }
}
