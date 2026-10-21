    // =====================================================================
    // 1. RemoveFromDiscardPile
    // =====================================================================

    inline bool HandContains(Obj* handEntity, int cardId) {
        auto* hand = FindComponent(handEntity, "Hand");
        if (!hand)
            return false;
        auto* ids = handCardIds[hand].Get();
        if (!ids || !ids->items)
            return false;
        for (int i = 0; i < ids->size; ++i) {
            if (ids->items->m_Items[i] == cardId)
                return true;
        }
        return false;
    }


    inline bool CardIsInAnyHand(Obj* model, int cardId) {
        if (!model || cardId < 0)
            return false;

        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && HandContains(entity, cardId))
                return true;
        }
        return false;
    }

