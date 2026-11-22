#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "abilities/NumericAbility.hpp"
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

        CC_DIAG("CustomComponents/GrantAbility/Runtime: fields counters=%d entries=%d methods active=%d add=%d remove=%d removeDuration=%d",
            (int)countersField.IsValid(), (int)entriesField.IsValid(), (int)isActive.IsValid(),
            (int)addCounter.IsValid(), (int)removeCounter.IsValid(), (int)removeAllCountersForDuration.IsValid());
        return countersField.IsValid() && entriesField.IsValid() && isActive.IsValid() &&
            addCounter.IsValid() && removeCounter.IsValid() &&
            removeAllCountersForDuration.IsValid();
    }

    inline bool Active(void* component) {
        auto active=isActive;return component && active.IsValid() && active[component]();
    }

    inline bool HasDuration(void* component,int duration) {
        auto countersMetadata=countersField;auto entriesMetadata=entriesField;
        if(!component)return false;
        auto* counters=countersMetadata[component].Get();
        auto* list=counters?static_cast<BNM::Structures::Mono::List<CounterEntry>*>(entriesMetadata[counters].Get()):nullptr;
        if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return false;
        for(int i=0;i<list->size;++i)if(list->items->m_Items[i].duration==duration)return true;
        return false;
    }

    inline int SumValues(void* component) {
        if (!component || !countersField.IsValid() || !entriesField.IsValid()) return 0;
        auto countersMetadata=countersField;auto entriesMetadata=entriesField;
        auto* counters = countersMetadata[component].Get();
        if (!counters) return 0;
        auto* rawList = entriesMetadata[counters].Get();
        if (!rawList) return 0;
        auto* list = static_cast<BNM::Structures::Mono::List<CounterEntry>*>(rawList);
        if (!list->items) return 0;
        int total = 0;
        for (int i = 0; i < list->GetSize(); ++i) total += list->GetData()[i].value;
        return total;
    }

    inline bool Apply(void* component, int duration, int source, int value,
                      NumericAbility::Operation operation) {
        auto add=addCounter;auto remove=removeCounter;
        if (!component || !add.IsValid()) return false;
        if (operation == NumericAbility::Operation::Set && remove.IsValid()) {
            remove[component](duration, source);
            // Match the old ModifiableInt Set semantics: AbilityValue describes
            // the desired total ability value, not an additional counter.
            value -= SumValues(component);
        }
        add[component](duration, source, value);
        return true;
    }

    inline void Remove(void* component, int duration, int source, bool expiry) {
        auto remove=removeCounter;auto removeDuration=removeAllCountersForDuration;
        if (!component) return;
        if (expiry) {
            if (removeDuration.IsValid())
                removeDuration[component](duration);
        } else if (remove.IsValid()) {
            remove[component](duration, source);
        }
    }
}
