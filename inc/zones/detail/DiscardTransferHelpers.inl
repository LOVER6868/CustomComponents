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

    // EntityId 0 is valid (notably the local player).
    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;
        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

    inline bool DiscardContains(Obj* discardPileEntity, int cardId) {
        auto* discard = FindComponent(discardPileEntity, "DiscardPile");
        if (!discard)
            return false;
        auto* ids = discardCardIds[discard].Get();
        if (!ids || !ids->items)
            return false;
        for (int i = 0; i < ids->size; ++i) {
            if (ids->items->m_Items[i] == cardId)
                return true;
        }
        return false;
    }

    inline Obj* ScratchDiscardModifier(Obj* model) {
        if (!model || !discardModifierClass.IsValid() || !discardModifierEntities.IsValid())
            return nullptr;

        // Never retain a raw IL2CPP pointer across matches or GC cycles.
        // The newly allocated modifier is used synchronously by the caller.
        Obj* modifier = discardModifierClass.CreateNewInstance();
        if (modifier)
            discardModifierEntities[modifier] = model;
        return modifier;
    }

    inline Obj* ScratchHandModifier(Obj* model) {
        if (!model || !handModifierClass.IsValid() || !handModifierEntities.IsValid())
            return nullptr;
        // Fresh temporary per operation; no unrooted process-global managed pointer.
        Obj* modifier = handModifierClass.CreateNewInstance();
        if (modifier)
            handModifierEntities[modifier] = model;
        return modifier;
    }

    struct DiscardContext {
        Obj* card = nullptr;
        Obj* player = nullptr;
        Obj* discardPile = nullptr;
        int cardId = -1;
        int playerId = -1;
        int discardPileId = -1;
    };

    inline bool ResolveDiscardContext(Obj* model, int cardId, DiscardContext& out) {
        out = {};
        out.cardId = cardId;

        Obj* card = FindEntity(model, cardId);
        if (!card)
            return false;

        const int playerId = getPlayerIdFromCard[static_cast<Obj*>(nullptr)](card);
        Obj* player = FindEntity(model, playerId);
        if (!player)
            return false;

        Obj* discardAssociation = FindComponent(player, "DiscardPileAssociation");
        if (!discardAssociation)
            return false;

        const int discardPileId = discardAssociationId[discardAssociation].Get();
        Obj* discardPile = FindEntity(model, discardPileId);
        if (!discardPile || !DiscardContains(discardPile, cardId))
            return false;

        out.card = card;
        out.player = player;
        out.discardPile = discardPile;
        out.playerId = playerId;
        out.discardPileId = discardPileId;
        return true;
    }

