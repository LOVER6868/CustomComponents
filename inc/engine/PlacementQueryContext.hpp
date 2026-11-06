#pragma once
namespace BNM::IL2CPP {struct Il2CppObject;}
namespace CustomComponents::Engine::PlacementQueryContext {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Value {Obj* model=nullptr;Obj* effect=nullptr;Obj* card=nullptr;int lane=-1;Obj* candidatesQuery=nullptr;bool live=false;bool provisional=false;};
inline thread_local Value active;
inline Obj* ProvisionalCard(){return active.provisional?active.card:nullptr;}
struct Scope {Value previous;explicit Scope(Value value):previous(active){if(value.model==previous.model && value.effect==previous.effect) {
        if(!value.card)value.card=previous.card;
        if(value.lane<0)value.lane=previous.lane;
        if(!value.candidatesQuery){value.candidatesQuery=previous.candidatesQuery;value.live=previous.live;}
    }active=value;}~Scope(){active=previous;}};
}
