#pragma once
#include <vector>
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
#include "core/WeakObjectSettings.hpp"
#include "core/AnimationQueuePostUpdate.hpp"
#include "core/SceneViewContext.hpp"
#include "engine/EntityAccess.hpp"

namespace CustomComponents::Presentation::CardReadiness {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Pending {std::shared_ptr<Core::ManagedRuntime::Root> view;bool playable=false;};
inline std::vector<Pending> pending;
inline std::uintptr_t scene=0;
inline BNM::Field<Obj*> dimmer;
inline BNM::Field<int> cardId;
inline void(*originalPlayable)(Obj*,bool,const void*)=nullptr;
// One native owner. Gameplay features may refine the requested highlight,
// but every path still passes through readiness and discard protection.
inline Core::Observers<Obj*,bool*> modifiers;
inline Core::WeakObjectSettings<bool> retiring;
inline bool Marked(Obj* view){
    Obj* model=reinterpret_cast<Obj*>(Core::SceneViewContext::ModelIdentity());
    const int id=view && cardId.IsValid()?Core::Metadata::Bind(cardId,view).Get():-1;
    return Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,id),"MarkedForDiscard")!=nullptr;
}
inline void Apply(Obj* view,bool playable,const void* method){
    modifiers.Notify(view,&playable);
    originalPlayable(view,playable && !Marked(view) && !retiring.Get(view),method);
}
static void Playable(Obj* view,bool playable,const void* method){
    if(scene!=Core::SceneViewContext::ModelIdentity()){pending.clear();scene=Core::SceneViewContext::ModelIdentity();}
    if(!view)return;
    if(!Core::Metadata::Bind(dimmer,view).Get()){
        for(auto& item:pending)if(item.view->Get()==view){item.playable=playable;return;}
        auto root=std::make_shared<Core::ManagedRuntime::Root>(view,true);
        if(root->Get())pending.push_back({root,playable});
        return;
    }
    Apply(view,playable,method);
}
inline void Dim(Obj* view){
    if(!view)return;
    // A retiring view stays dim even after the authoritative card has already
    // left the hand and lost its transient marker. Level may refine normal
    // highlights, but cannot relight this specific old view.
    retiring.Set(view,true);
    Playable(view,false,nullptr);
}
inline void Tick(void*,const void*){
    if(scene!=Core::SceneViewContext::ModelIdentity()){pending.clear();scene=Core::SceneViewContext::ModelIdentity();return;}
    for(auto it=pending.begin();it!=pending.end();){
        Obj* view=it->view->Get();
        if(!view){it=pending.erase(it);continue;}
        if(!Core::Metadata::Bind(dimmer,view).Get()){++it;continue;}
        const bool playable=it->playable;
        it=pending.erase(it);
        Apply(view,playable,nullptr);
    }
}
inline bool Initialize(){
    auto type=BNM::Class("PvZCards.Game","CardViewComponent");
    dimmer=type.GetField("Dimmer").cast<Obj*>();cardId=type.GetField("CardId").cast<int>();
    return dimmer.IsValid() && cardId.IsValid() && Core::Hooks::Install(type.GetMethod("SetPlayable",1),Playable,originalPlayable) && Core::AnimationQueuePostUpdate::Register(Tick);
}
}
