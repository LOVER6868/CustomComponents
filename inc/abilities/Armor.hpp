#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
namespace CustomComponents::Armor {
    inline BNM::MonoType* monoType = nullptr;
    inline BNM::Field<void*> amount;
    inline BNM::Method<void*> getOrAdd, get;
    inline BNM::Method<bool> has;
    inline bool Initialize() {
        auto armor = BNM::Class("PvZCards.Engine.Components", "Armor");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        if (!armor.IsValid() || !entity.IsValid()) return false;
        monoType = armor.GetMonoType();
        amount = armor.GetField("ArmorAmount");
        getOrAdd = entity.GetMethod("GetOrAddComponent", 1);
        get = entity.GetMethod("GetComponent", 1);
        has = entity.GetMethod("HasComponent", 1);
        return monoType && amount.IsValid() && getOrAdd.IsValid() && get.IsValid() && has.IsValid();
    }
    inline void* GetValue(void* entity, bool create) {
        if (!entity || (!create && !has[entity](monoType))) return nullptr;
        auto* component = create ? getOrAdd[entity](monoType) : get[entity](monoType);
        return component ? amount[component].Get() : nullptr;
    }
}
