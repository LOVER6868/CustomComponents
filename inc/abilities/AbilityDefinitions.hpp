#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>

#include "abilities/Armor.hpp"
#include "abilities/MarkerGrantState.hpp"
#include "abilities/Withering.hpp"
#include "abilities/SelfTeleport.hpp"
#include "abilities/OverhealToHealth.hpp"
#include "abilities/Charge.hpp"
#include "abilities/Thrash.hpp"
#include "core/AbilityRegistry.hpp"
#include "log.h"

namespace CustomComponents::AbilityDefinitions {
    inline constexpr int Armor = 19;
    inline constexpr int Charge = 20;
    inline constexpr int Thrash = 21;
    inline constexpr int ArmorSpecialAbilityType = 8;

    inline BNM::Method<bool> hasComponent;
    inline BNM::Method<void*> getComponent;
    inline BNM::Method<void*> getOrAddComponent;
    inline BNM::MonoType* chargeType = nullptr;
    inline BNM::MonoType* thrashType = nullptr;

    inline void* GetComponent(void* entity, BNM::MonoType* type, bool create) {
        if (!entity || !type) return nullptr;
        auto has=hasComponent;auto get=getComponent;auto add=getOrAddComponent;
        if (has[entity](type)) return get[entity](type);
        return create ? add[entity](type) : nullptr;
    }

    inline void* GetCharge(void* entity, bool create) {
        return GetComponent(entity, chargeType, create);
    }

    inline void* GetThrash(void* entity, bool create) {
        return GetComponent(entity, thrashType, create);
    }

    inline bool Initialize() {
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto charge = BNM::Class("PvZCards.Engine.Components", "Charge");
        auto thrash = BNM::Class("PvZCards.Engine.Components", "Thrash");
        if (!entity.IsValid() || !charge.IsValid() || !thrash.IsValid()) return false;

        hasComponent = entity.GetMethod("HasComponent", 1);
        getComponent = entity.GetMethod("GetComponent", 1);
        getOrAddComponent = entity.GetMethod("GetOrAddComponent", 1);
        chargeType = charge.GetMonoType();
        thrashType = thrash.GetMonoType();
        if (!hasComponent.IsValid() || !getComponent.IsValid() ||
            !getOrAddComponent.IsValid() || !chargeType || !thrashType) return false;

        if (!Abilities::MarkerGrants::Initialize() || !Abilities::Withering::Initialize() || !Abilities::SelfTeleport::Initialize() || !Abilities::OverhealToHealth::Initialize()) return false;
        if (!AbilityRegistry::Register({Armor, "Armor", AbilityStorage::Numeric,
                nullptr, Armor::GetValue, ArmorSpecialAbilityType}) ||
            !AbilityRegistry::Register({Charge, "Charge", AbilityStorage::Grantable,
                GetCharge, nullptr}) ||
            !AbilityRegistry::Register({Thrash, "Thrash", AbilityStorage::Grantable,
                GetThrash, nullptr}) ||
            !AbilityRegistry::Register({23,"Withering",AbilityStorage::Grantable,Abilities::Withering::Get,nullptr}) ||
            !AbilityRegistry::Register({24,"Multishot",AbilityStorage::Marker,Abilities::MarkerGrants::Multishot,nullptr,-1,"GrantedMultishot"}) ||
            !AbilityRegistry::Register({25,"AttacksOnlyInAdjacentLanes",AbilityStorage::Marker,Abilities::MarkerGrants::Adjacent,nullptr,-1,"GrantedAdjacentAttacks"}) ||
            !AbilityRegistry::Register({Abilities::SelfTeleport::Id,"SelfTeleport",AbilityStorage::Grantable,Abilities::SelfTeleport::Get,nullptr}) ||
            !AbilityRegistry::Register({Abilities::OverhealToHealth::Id,"OverhealToHealth",AbilityStorage::Grantable,Abilities::OverhealToHealth::Get,nullptr})) {
            LOG_ERR("CustomComponents/Abilities: registry registration failed");
            return false;
        }

        CC_DIAG("CustomComponents/Abilities: registered Armor=19 Charge=20 Thrash=21");
        return true;
    }
}
