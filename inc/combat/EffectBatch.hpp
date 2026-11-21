#pragma once
#include <vector>
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::ExtraCombat {
// Generated effects must remain rooted across subsequent native allocations
// until EffectCollection owns the completed group.
struct EffectBatch:std::vector<Core::ManagedRuntime::Obj*> {
    std::vector<std::shared_ptr<Core::ManagedRuntime::Root>> roots;
    void push_back(Core::ManagedRuntime::Obj* effect){
        if(!effect)return;
        roots.push_back(std::make_shared<Core::ManagedRuntime::Root>(effect));
        std::vector<Core::ManagedRuntime::Obj*>::push_back(effect);
    }
};
}
