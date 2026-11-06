#pragma once
namespace CustomComponents::Systems::ReturnToLaneTargeting {
struct Context {void* finder=nullptr;void* model=nullptr;int player=-1;};
inline thread_local Context active;
class Scope {
    Context previous=active;
public:
    explicit Scope(Context context) {active=context;}
    ~Scope(){active=previous;}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
inline bool Owns(void* finder) {
    return finder && active.finder==finder && active.model && active.player>=0;
}
}
