#pragma once
// Shared, passive post-stock-update observers for EffectAnimationQueue.Update.
// ZoneTriggeredAbilityPresentationSystem is the only native hook owner.
// Register additional presentation tasks here rather than hooking Update again.
#include "core/Observers.hpp"

namespace CustomComponents::Core::AnimationQueuePostUpdate {
    using Callback = void (*)(void*, const void*);
    inline Core::Observers<void*, const void*> callbacks;

    inline bool Register(Callback callback) {
        return callbacks.Add(callback);
    }

    inline void Notify(void* queue, const void* method) {
        callbacks.Notify(queue, method);
    }
}
