#pragma once
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include "core/AtomicSnapshot.hpp"
namespace CustomComponents::Core {
// Rules only narrow an existing engine-approved pool. They cannot invent a
// target, choose a GUID, consume RNG, or override another rule's rejection.
template<class Context> class CandidateRules {
public:
    using Rule=bool(*)(const Context&,int);
    using Snapshot=std::shared_ptr<const std::vector<Rule>>;
    bool Add(Rule rule) {
        if(!rule) return false;
        std::lock_guard<std::mutex> guard(lock_);
        auto prior=rules_.load();
        if(std::find(prior->begin(),prior->end(),rule)!=prior->end()) return true;
        auto next=std::make_shared<std::vector<Rule>>(*prior);next->push_back(rule);
        rules_.store(Snapshot(next));return true;
    }
    Snapshot Read() const {return rules_.load();}
    static bool Allows(const Snapshot& rules,const Context& context,int guid) {
        for(auto rule:*rules) if(!rule(context,guid)) return false;
        return true;
    }
private:
    std::mutex lock_;
    AtomicSnapshot<std::vector<Rule>> rules_{std::make_shared<const std::vector<Rule>>()};
};
}
