#pragma once
#include <cstdint>
namespace CustomComponents::Core {
// Frontend holder/binder identities are rooted by the caller. Simulation-model
// clones are deliberately not generation boundaries.
class MatchGeneration {
    const void* holder_=nullptr;
    const void* binder_=nullptr;
    std::uint64_t generation_=0;
public:
    bool Observe(const void* holder,const void* binder) {
        if(!holder || !binder)return false;
        if(holder_==holder && binder_==binder)return false;
        const bool retired=generation_!=0;
        holder_=holder;binder_=binder;++generation_;
        return retired;
    }
    std::uint64_t Value() const {return generation_;}
};
}
