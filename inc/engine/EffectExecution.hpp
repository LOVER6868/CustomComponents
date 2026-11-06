#pragma once
namespace CustomComponents::Engine::EffectExecution {
// Functional, synchronous context supplied by the queue's single hook owner.
inline thread_local int group=-1;
struct Scope {
    int previous=group;
    explicit Scope(int value){group=value;}
    ~Scope(){group=previous;}
};
}
