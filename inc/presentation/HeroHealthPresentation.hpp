#pragma once
#include <algorithm>
#include <limits>
#include "core/HookRegistry.hpp"
#include "engine/MatchAccess.hpp"
#include "presentation/CardStatState.hpp"

namespace CustomComponents::Presentation::HeroHealth {
using Obj=BNM::IL2CPP::Il2CppObject;
inline void(*originalHeal)(Obj*,Obj*,int,const void*)=nullptr;
static void Heal(Obj* life,Obj* player,int amount,const void* method){
    static auto health=BNM::Class("PvZCards.Game","LifeViewComponent").GetField("Health").cast<int>();
    static auto set=BNM::Class("PvZCards.Game","LifeViewComponent").GetMethod("SetHealth",1).cast<void>();
    const auto stats=CardStats::Read(player);
    if(!life || !player || !stats.maxHealth || !health.IsValid() || !set.IsValid()){
        originalHeal(life,player,amount,method);return;
    }
    const int before=Core::Metadata::Bind(health,life).Get();
    originalHeal(life,player,amount,method);
    // Native HealDamage clamps to MaxHealth.BaseValue, ignoring modifiers.
    // Keep its particles/animation, but use the same modified maximum as rules.
    const int expected=int(std::clamp<long long>(static_cast<long long>(before)+amount,0,std::max(0,*stats.maxHealth)));
    const int shown=Core::Metadata::Bind(health,life).Get();
    if(shown!=expected){
        Core::Metadata::Bind(set,life)(expected);
        LOG_INFO("CustomComponents/HeroHealth: player=%d shown=%d corrected=%d maximum=%d",Engine::Match::Id(player),shown,expected,*stats.maxHealth);
    }
}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Game","LifeViewComponent").GetMethod("HealDamage",2),Heal,originalHeal);}
}
