    static void RegisterHandlersHook(
        void* self,
        Obj* router,
        const void* method
    ) {
        originalRegisterHandlers(self, router, method);
        if (!self || !gcNewWeak || !gcTarget || !gcFree)
            return;

        std::lock_guard lock(deckMapMutex);
        std::vector<uintptr_t> alive;
        for (auto handle : abilitySystems) {
            if (gcTarget(handle))
                alive.push_back(handle);
            else
                gcFree(handle);
        }
        alive.push_back(gcNewWeak(static_cast<Obj*>(self), false));
        abilitySystems.swap(alive);
    }

