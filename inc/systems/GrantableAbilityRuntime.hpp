#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "systems/NumericAbility.hpp"
#include "log.h"

namespace CustomComponents::GrantableAbilityRuntime {
    struct CounterEntry {
        int duration{};
        int sourceId{};
        int value{};
    };

    inline BNM::Field<void*> countersField;
    inline BNM::Field<void*> entriesField;
    inline BNM::Method<bool> isActive;
    inline BNM::Method<void> addCounter;
    inline BNM::Method<void> removeCounter;
    inline BNM::Method<void> removeAllCountersForDuration;

    inline bool Initialize() {
        auto ability = BNM::Class("PvZCards.Engine.Components", "GrantableAbility");
        auto group = BNM::Class("PvZCards.Engine.Components.Data", "ModifiableCounterGroup");
        if (!ability.IsValid() || !group.IsValid()) {
            LOG_ERR("CustomComponents/GrantAbility/Runtime: class missing ability=%d counterGroup=%d", (int)ability.IsValid(), (int)group.IsValid());
            return false;
        }

        countersField = ability.GetField("Counters");
        entriesField = group.GetField("Entries");
        isActive = ability.GetMethod("IsActive", 0);
        addCounter = ability.GetMethod("AddCounter", 3);
        removeCounter = ability.GetMethod("RemoveCounter", 2);
        removeAllCountersForDuration = ability.GetMethod("RemoveAllCountersForDuration", 1);

        LOG_INFO("CustomComponents/GrantAbility/Runtime: fields counters=%d entries=%d methods active=%d add=%d remove=%d removeDuration=%d",
            (int)countersField.IsValid(), (int)entriesField.IsValid(), (int)isActive.IsValid(),
            (int)addCounter.IsValid(), (int)removeCounter.IsValid(), (int)removeAllCountersForDuration.IsValid());
        return countersField.IsValid() && entriesField.IsValid() && isActive.IsValid() &&
            addCounter.IsValid() && removeCounter.IsValid() &&
            removeAllCountersForDuration.IsValid();
    }

    inline bool Active(void* component) {
        return component && isActive.IsValid() && isActive[component]();
    }

    inline int SumValues(void* component) {
        if (!component || !countersField.IsValid() || !entriesField.IsValid()) return 0;
        auto* counters = countersField[component].Get();
        if (!counters) return 0;
        auto* rawList = entriesField[counters].Get();
        if (!rawList) return 0;
        auto* list = static_cast<BNM::Structures::Mono::List<CounterEntry>*>(rawList);
        if (!list->items) return 0;
        int total = 0;
        for (int i = 0; i < list->GetSize(); ++i) total += list->GetData()[i].value;
        return total;
    }

    inline bool Apply(void* component, int duration, int source, int value,
                      NumericAbility::Operation operation) {
        if (!component || !addCounter.IsValid()) return false;
        if (operation == NumericAbility::Operation::Set && removeCounter.IsValid()) {
            removeCounter[component](duration, source);
            // Match the old ModifiableInt Set semantics: AbilityValue describes
            // the desired total ability value, not an additional counter.
            value -= SumValues(component);
        }
        addCounter[component](duration, source, value);
        return true;
    }

    inline void Remove(void* component, int duration, int source, bool expiry) {
        if (!component) return;
        if (expiry) {
            if (removeAllCountersForDuration.IsValid())
                removeAllCountersForDuration[component](duration);
        } else if (removeCounter.IsValid()) {
            removeCounter[component](duration, source);
        }
    }
}
