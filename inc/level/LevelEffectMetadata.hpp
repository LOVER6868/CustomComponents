#pragma once
#include "level/LevelPlayMetadata.hpp"
#include "engine/EffectQueue.hpp"
namespace CustomComponents::Level::Plays {
inline thread_local Obj* activePlay=nullptr;
inline void(*originalAddPlay)(Obj*,Obj*,const void*)=nullptr;
static void AddPlay(Obj* self,Obj* play,const void* method){struct Restore{Obj* previous;~Restore(){activePlay=previous;}} restore{activePlay};activePlay=play;originalAddPlay(self,play,method);}
inline void QueuePlay(Obj*,Obj* effect,int,Obj*){if(!activePlay || !effect || !effect->klass || std::string_view(effect->klass->name)!="PlayCardEffect")return;auto playType=BNM::Class("PvZCards.Engine.Dtos","Play");auto effectType=BNM::Class("PvZCards.Engine","Effect");
    if(playType.GetField("CardId").cast<int>()[activePlay].Get()!=effectType.GetField("TargetId").cast<int>()[effect].Get() || playType.GetField("PlayerId").cast<int>()[activePlay].Get()!=effectType.GetField("PlayerId").cast<int>()[effect].Get())return;
    auto choice=choices.Get(activePlay);effects.Set(effect,choice,choice.present);}
inline bool InitializeEffectMetadata(){return Engine::EffectQueue::beforePush.Add(QueuePlay) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","PlayCardHelper").GetMethod("AddPlayCardEffect",1),AddPlay,originalAddPlay);}
}
