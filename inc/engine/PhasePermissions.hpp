#pragma once
#include "variables/Reactive.hpp"
namespace CustomComponents::Engine::PhasePermissions {
using Obj=BNM::IL2CPP::Il2CppObject;
inline bool(*originalFighter)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline bool(*originalWhite)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline bool(*originalBlack)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline Obj* ModelFromTypes(Obj* types){auto field=BNM::Class("PvZCards.Core.Utilities","CardTypeUtils").GetField("<EntityModel>k__BackingField").cast<Obj*>();return types?field[types].Get():nullptr;}
inline bool LocalPermission(Obj* model,Obj* play){
    if(!model || Variables::Field(play,"Type")!=3)return false;int player=Variables::Field(play,"PlayerId");Obj* card=EntityAccess::Find(model,Variables::Field(play,"CardId"));Obj* owner=EntityAccess::Find(model,player);
    return card && EntityAccess::Component(card,"HandElement") && BoardPlacement::Owner(card)==player && EntityAccess::Component(owner,"Zombies") && Variables::Reactive::Active(model,card);
}
static bool Fighter(Obj* self,Obj* play,Obj* errors,const void* method){
    static auto metadata=BNM::Class("PvZCards.Core.Utilities","CanPlayFighterInSurprisePhaseValidator").GetField("<EntityModel>k__BackingField").cast<Obj*>();auto field=metadata;Obj* model=field[self].Get();Obj* card=EntityAccess::Find(model,Variables::Field(play,"CardId"));
    Obj* queue=Match::Provide(Match::Instances(card),BNM::Class("PvZCards.Engine","EffectCollection").GetMonoType());Obj* phase=nullptr;if(queue){auto peek=BNM::Class(queue).GetMethod("Peek",0).cast<Obj*>();if(peek.IsValid())phase=peek[queue]();}
    if(phase && phase->klass && std::string_view(phase->klass->name)=="SurprisePhaseBlockingEffect" && Variables::Field(phase,"PlayerId")==Variables::Field(play,"PlayerId") && card && CardTypes::Fighter(card) && LocalPermission(model,play))return true;
    return originalFighter(self,play,errors,method);
}
inline bool Relevant(Obj* self,bool white){
    static std::mutex lock;static std::map<std::pair<void*,bool>,bool> cache;auto key=std::make_pair(static_cast<void*>(self->klass),white);
    {std::lock_guard<std::mutex> guard(lock);if(auto it=cache.find(key);it!=cache.end())return it->second;}
    bool result=false;auto error=BNM::TryInvoke([&]{Obj* type=reinterpret_cast<Obj*>(BNM::Class(self).GetMonoType());auto get=BNM::Class(type).GetMethod("GetGenericArguments",0).cast<Obj*>();auto* args=reinterpret_cast<BNM::Structures::Mono::Array<Obj*>*>(get[type]());if(!args || args->capacity!=2)return;auto name0=BNM::Class(args->m_Items[0]).GetMethod("get_Name",0).cast<BNM::Structures::Mono::String*>();auto name1=BNM::Class(args->m_Items[1]).GetMethod("get_Name",0).cast<BNM::Structures::Mono::String*>();auto* first=name0[args->m_Items[0]]();auto* second=name1[args->m_Items[1]]();result=first && second && first->str()=="Surprise" && second->str()==(white?"SurprisePhaseBlockingEffect":"MainPhaseBlockingEffect");});
    result=result && !error.IsValid();{std::lock_guard<std::mutex> guard(lock);cache[key]=result;}return result;
}
inline bool MainException(Obj* self,Obj* play,bool white){
    if(!Relevant(self,white))return false;auto type=BNM::Class(self);auto types=type.GetField("<CardTypeUtils>k__BackingField").cast<Obj*>();auto effects=type.GetField("<Effects>k__BackingField").cast<Obj*>();Obj* model=ModelFromTypes(types[self].Get());
    if(!LocalPermission(model,play))return false;Obj* card=EntityAccess::Find(model,Variables::Field(play,"CardId"));if(!EntityAccess::Component(card,"Surprise"))return false;
    Obj* queue=effects[self].Get();if(!queue)return false;auto peek=BNM::Class(queue).GetMethod("Peek",0).cast<Obj*>();Obj* phase=peek.IsValid()?peek[queue]():nullptr;
    return phase && phase->klass && std::string_view(phase->klass->name)=="MainPhaseBlockingEffect" && Variables::Field(phase,"PlayerId")==Variables::Field(play,"PlayerId");
}
static bool White(Obj* self,Obj* play,Obj* errors,const void* method){return MainException(self,play,true)?true:originalWhite(self,play,errors,method);}
static bool Black(Obj* self,Obj* play,Obj* errors,const void* method){return MainException(self,play,false)?true:originalBlack(self,play,errors,method);}
inline bool Initialize(){
    auto surprise=BNM::Class("PvZCards.Engine.Components","Surprise").GetCompileTimeClass();
    auto white=BNM::Class("PvZCards.Core.Utilities","CardPhaseWhiteListValidator`2").GetGeneric({surprise,BNM::Class("PvZCards.Engine.Effects","SurprisePhaseBlockingEffect").GetCompileTimeClass()});
    auto black=BNM::Class("PvZCards.Core.Utilities","CardPhaseBlackListValidator`2").GetGeneric({surprise,BNM::Class("PvZCards.Engine.Effects","MainPhaseBlockingEffect").GetCompileTimeClass()});
    return Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","CanPlayFighterInSurprisePhaseValidator").GetMethod("Validate",2),Fighter,originalFighter) && Core::Hooks::Install(white.GetMethod("Validate",2),White,originalWhite) && Core::Hooks::Install(black.GetMethod("Validate",2),Black,originalBlack);
}
}
