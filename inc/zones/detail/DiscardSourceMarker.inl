#include "diagnostics/DiagnosticLogging.hpp"
    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0 || !modelEntities.IsValid()) return nullptr;
        auto* list = modelEntities[model].Get();
        if (!list || !list->items) return nullptr;
        for (int i = 0; i < list->size; ++i) {
            Obj* e = list->items->m_Items[i];
            if (e && entityId[e].Get() == id) return e;
        }
        return nullptr;
    }

    inline Obj* NamedComponent(Obj* entity, const char* name) {
        if (!entity || !name) return nullptr;
        auto* list = entityComponents[entity].Get();
        if (!list || !list->items) return nullptr;
        for (int i = 0; i < list->size; ++i) {
            Obj* item = list->items->m_Items[i];
            if (!item) continue;
            auto* klass = BNM::Class(item).GetClass();
            if (klass && klass->name && std::strcmp(klass->name, name) == 0)
                return item;
        }
        return nullptr;
    }

    inline bool InDiscard(Obj* model, int cardId) {
        if (!ready || !model || cardId < 0) return false;
        Obj* card = FindEntity(model, cardId);
        if (!card) return false;
        const int playerId = playerIdForCard[static_cast<Obj*>(nullptr)](card);
        Obj* player = FindEntity(model, playerId);
        Obj* assoc = NamedComponent(player, "DiscardPileAssociation");
        if (!assoc) return false;
        Obj* pile = FindEntity(model, discardPileAssociationId[assoc].Get());
        Obj* component = NamedComponent(pile, "DiscardPile");
        if (!component) return false;
        auto* ids = discardPileIds[component].Get();
        if (!ids || !ids->items) return false;
        for (int i = 0; i < ids->size; ++i)
            if (ids->items->m_Items[i] == cardId) return true;
        return false;
    }

    inline int GetSource(Obj* model, int cardId) {
        if (!ready) return 0;
        Obj* card = FindEntity(model, cardId);
        if (!card || !entityHas[card](discardedType)) return 0;
        Obj* marker = entityGet[card](discardedType);
        return marker ? discardedSource[marker].Get() : 0;
    }

    // Only tag a SUCCESSFUL transition into a real discard pile.
    inline bool Mark(Obj* model, int cardId, Source source) {
        if ((source != Source::Hand && source != Source::Deck) ||
            !ready || !InDiscard(model, cardId)) return false;
        Obj* card = FindEntity(model, cardId);
        Obj* marker = card ? entityGetOrAdd[card](discardedType) : nullptr;
        if (!marker) {
            LOG_ERR("CustomComponents/Discarded: add failed model=%p card=%d", model, cardId);
            return false;
        }
        discardedSource[marker] = static_cast<int>(source);
        CC_DIAG("CustomComponents/Discarded: SET model=%p card=%d Source=%d", model, cardId, static_cast<int>(source));
        return true;
    }

