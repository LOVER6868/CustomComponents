#pragma once
#include "core/CandidateRules.hpp"
#include <BNM/Il2CppHeaders.hpp>
namespace CustomComponents::Engine::TriggerEligibility {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Context {Obj* effectEntity;Obj* query;};
inline Core::CandidateRules<Context> rules;
inline bool Allows(Obj* effect,Obj* query){return Core::CandidateRules<Context>::Allows(rules.Read(),{effect,query},0);}
}
