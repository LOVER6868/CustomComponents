    template<typename T>
    inline std::vector<T> Items(MonoList<T>* list) {
        std::vector<T> out;
        if (!list || !list->items)
            return out;
        out.reserve(list->size);
        for (int i = 0; i < list->size; ++i)
            out.push_back(list->items->m_Items[i]);
        return out;
    }

    inline std::string TypeName(Obj* object) {
        if (!object)
            return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "?";
    }

    inline Obj* FindComponent(Obj* entity, const char* name) {
        if (!entity)
            return nullptr;
        for (auto* component : Items(entityComponents[entity].Get())) {
            if (TypeName(component) == name)
                return component;
        }
        return nullptr;
    }

    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;
        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

    inline int GuidOf(Obj* entity) {
        auto* card = FindComponent(entity, "Card");
        return card ? cardGuid[card].Get() : -1;
    }

    inline bool IsCustomRuntime(Obj* effect) {
        if (!effect || !runtimeType)
            return false;
        auto customClass = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetInDeckEffect"
        );
        return customClass.IsValid() &&
            BNM::Class(effect).GetClass() == customClass.GetClass();
    }

    inline PendingDraw* FindPending(Obj* effect) {
        if (!effect)
            return nullptr;

        const int source = effectSource[effect].Get();
        const int player = effectPlayer[effect].Get();
        const int group = effectGroup[effect].Get();

        // Prefer exact EffectGroupId. Fall back to source/player because some
        // queue-building paths can assign the child group after CopyDefaultIds.
        for (auto& pending : pendingDraws) {
            if (pending.remaining > 0 &&
                pending.sourceId == source &&
                pending.playerId == player &&
                pending.effectGroupId == group) {
                return &pending;
            }
        }
        for (auto& pending : pendingDraws) {
            if (pending.remaining > 0 &&
                pending.sourceId == source &&
                pending.playerId == player) {
                return &pending;
            }
        }
        return nullptr;
    }

    inline void CleanupPending() {
        pendingDraws.erase(
            std::remove_if(
                pendingDraws.begin(),
                pendingDraws.end(),
                [](const PendingDraw& p) { return p.remaining <= 0; }
            ),
            pendingDraws.end()
        );
    }

    inline BNM::Method<void> ListMethod(MonoList<Obj*>* list, const char* name, int parameters) {
        if (!list)
            return {};
        auto method = BNM::Class(static_cast<Obj*>(list)).GetMethod(name, parameters).cast<void>();
        if (!method.IsValid())
            LOG_ERR("CustomComponents/DrawCardFromSubsetInDeck: List.%s not found", name);
        return method;
    }

    inline int FindEntry(
        const std::vector<Obj*>& entries,
        int guid,
        const std::set<Obj*>& skip
    ) {
        int index = -1;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            auto* entry = entries[i];
            if (!entry || skip.count(entry) || entryGuid[entry].Get() != guid)
                continue;
            if (index < 0)
                index = i;
            if (entryEntityId[entry].Get() <= 0)
                return i;
        }
        return index;
    }

    inline bool MoveToTop(MonoList<Obj*>* list, int index, Obj* entry) {
        auto removeAt = ListMethod(list, "RemoveAt", 1);
        auto insert = ListMethod(list, "Insert", 2);
        if (!removeAt.IsValid() || !insert.IsValid())
            return false;
        removeAt[static_cast<Obj*>(list)](index);
        insert[static_cast<Obj*>(list)](0, entry);
        return true;
    }

    inline std::set<Obj*> Reserved(
        Obj* model,
        int deck,
        const std::vector<Obj*>& entries
    ) {
        std::lock_guard lock(deckMapMutex);
        std::set<Obj*> present;
        auto it = reservedEntries.find({ model, deck });
        if (it == reservedEntries.end())
            return present;

        const std::set<Obj*> live(entries.begin(), entries.end());
        std::vector<Obj*> keep;
        for (auto* entry : it->second) {
            if (live.count(entry)) {
                keep.push_back(entry);
                present.insert(entry);
            }
        }
        if (keep.empty())
            reservedEntries.erase(it);
        else
            it->second.swap(keep);
        return present;
    }

    inline Obj* AbilitySystemFor(Obj* model) {
        if (!gcTarget)
            return nullptr;
        std::lock_guard lock(deckMapMutex);
        for (auto handle : abilitySystems) {
            Obj* system = gcTarget(handle);
            if (system && abilitySystemModel[system].Get() == model)
                return system;
        }
        return nullptr;
    }

