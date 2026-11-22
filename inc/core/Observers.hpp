#pragma once
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include "core/AtomicSnapshot.hpp"

namespace CustomComponents::Core {
// Registration is copy-on-write; dispatch never holds a lock across engine or
// feature callbacks. A repeated initialization cannot register a callback twice.
template<class... Args> class Observers {
public:
    using Callback=void(*)(Args...);
    bool Add(Callback callback) {
        if(!callback) return false;
        std::lock_guard<std::mutex> guard(lock_);
        auto prior=callbacks_.load();
        if(std::find(prior->begin(),prior->end(),callback)!=prior->end()) return true;
        auto next=std::make_shared<std::vector<Callback>>(*prior);
        next->push_back(callback);
        callbacks_.store(std::shared_ptr<const std::vector<Callback>>(next));
        return true;
    }
    void Notify(Args... args) const {
        const auto snapshot=callbacks_.load();
        for(auto callback:*snapshot) callback(args...);
    }
private:
    std::mutex lock_;
    AtomicSnapshot<std::vector<Callback>> callbacks_{std::make_shared<const std::vector<Callback>>()};
};
}
