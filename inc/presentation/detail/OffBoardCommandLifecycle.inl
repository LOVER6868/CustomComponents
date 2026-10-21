    // Read-only command inventory. Uses the BNM List<T> layout; no activation,
    // mutation, or animation creation is performed by the tracer.
    inline void TraceEventCommands(void* container, const char* phase) {
        if (!container || !commandEventCommands.IsValid()) return;
        Obj* commandsObj = commandEventCommands[static_cast<Obj*>(container)].Get();
        if (!commandsObj) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s container=%p EventCommands=null", phase, container);
            return;
        }
        auto* commands = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(commandsObj);
        const int count = commands->GetSize();
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s container=%p count=%d", phase, container, count);
        if (count < 0 || count > 4096 || !commands->items) return;
        Obj** entries = commands->GetData();
        if (!entries) return;
        for (int i = 0; i < count && i < 64; ++i) {
            Obj* action = entries[i];
            if (!action) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s index=%d action=null", phase, i);
                continue;
            }
            auto klass = BNM::Class(action);
            auto activated = klass.GetMethod("get_HasBeenActivated", 0).cast<bool>();
            auto eventType = klass.GetMethod("get_EventType", 0).cast<int>();
            const int activeValue = activated.IsValid() ? static_cast<int>(activated[action]()) : -1;
            const int typeValue = eventType.IsValid() ? eventType[action]() : -1;
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: %s index=%d action=%p klass=%p activated=%d eventType=%d", phase, i, action, action->klass, activeValue, typeValue);
        }
    }

    static void ExecuteRemainingCommandsHook(
        void* self, const void* method
    ) {
        if (IsZoneNullTimelineActive()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: STOCK ENTER container=%p", self);
            TraceEventCommands(self, "BEFORE");
            originalExecuteRemainingCommands(self, method);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/ACTIONTRACE: STOCK EXIT container=%p", self);
            TraceEventCommands(self, "AFTER");
            return;
        }
        originalExecuteRemainingCommands(self, method);
    }

    static void HandleCommandLoadCompleteHook(void* self, const void* method) {
        Obj* animation = static_cast<Obj*>(self);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/WAITLOAD: HandleCommandLoadComplete ENTER animation=%p", animation);
        LogWaitForLoad(animation, "BEFORE load callback");
        PendingLoadScope pending{};
        if (ConsumeLoadScope(animation, pending)) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DELAYEDSCOPE: ENTER animation=%p mapper=%p source=%d", animation, pending.mapper, pending.sourceId);
            ZoneNullTimelineScope scope(pending.mapper, pending.sourceId);
            originalHandleCommandLoadComplete(self, method);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/DELAYEDSCOPE: EXIT animation=%p", animation);
        } else {
            originalHandleCommandLoadComplete(self, method);
        }
        LogWaitForLoad(animation, "AFTER load callback");
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/WAITLOAD: HandleCommandLoadComplete EXIT animation=%p", animation);
    }

    static void AddParticleEffectAnimationHook(
        void* self, Obj* action, Obj* data, const void* method
    ) {
        // Never suppress this entire registration stage: it can contain
        // legitimate Trick/target particles even within an off-board scope.
        if (IsZoneNullTimelineActive()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/PARTICLE: STOCK FORWARD self=%p action=%p data=%p",
                self, action, data);
        }
        originalAddParticleEffectAnimation(self, action, data, method);
    }


