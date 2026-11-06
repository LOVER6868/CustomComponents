#pragma once
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Engine::CardTypes {
using Obj=BNM::IL2CPP::Il2CppObject;
// Fighter is a game predicate, not a component. Keep the stateless stock query
// rooted; no match entity or model is retained by this adapter.
inline bool Fighter(Obj* entity) {
    if(!entity)return false;
    static auto type=BNM::Class("PvZCards.Engine.Queries","FighterQuery");
    static auto matches=type.GetMethod("Matches",2).cast<bool>();
    static auto query=std::make_shared<Core::ManagedRuntime::Root>(
        type.IsValid()?type.CreateNewObjectParameters():nullptr);
    auto method=matches;
    return query->Get() && method.IsValid() && method[query->Get()](entity,static_cast<Obj*>(nullptr));
}
}
