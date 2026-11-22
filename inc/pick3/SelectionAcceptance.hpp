#pragma once
#include "core/HookRegistry.hpp"
#include "pick3/SelectionTransactionRuntime.hpp"
#include "engine/SelectionLayout.hpp"

namespace CustomComponents::Pick3::Acceptance {
using Obj=BNM::IL2CPP::Il2CppObject;
using IntList=BNM::Structures::Mono::List<int>;
inline bool(*validateCards)(Obj*,const std::vector<Picked::SelectedCard>&)=nullptr;
inline void(*originalCreate)(Obj*,Obj*,Obj*,const void*)=nullptr;
static void Create(Obj* self,Obj* effect,Obj* entity,const void* method) {
    namespace RT=TransactionRuntime;
    namespace TX=Transaction;
    namespace Layout=Engine::SelectionLayout;
    const int id=RT::IntAt(effect,Layout::effectId),group=RT::IntAt(effect,Layout::group);
    const auto context=TX::Context{id,RT::IntAt(effect,Layout::source),RT::IntAt(effect,Layout::target),RT::IntAt(effect,Layout::springboard)};
    Obj* model=RT::ObjectAt(self,Layout::model);
    const auto token=RT::ModelOf(model);
    const auto intent=RT::PendingFor(token,id,group);
    auto* targets=reinterpret_cast<IntList*>(RT::ObjectAt(effect,Layout::selectedTargets));
    bool matches=targets && targets->items && targets->size>0 && targets->size<=3 &&
        targets->size<=int(targets->items->capacity) && std::size_t(targets->size)==intent.targets.size();
    if(matches) for(int i=0;i<targets->size;++i) if(targets->items->m_Items[i]!=intent.targets[i]) matches=false;
    const auto cancelled=[&]{return effect && *reinterpret_cast<const bool*>(reinterpret_cast<const std::uint8_t*>(effect)+Layout::cancelled);};
    const bool accepted=RT::pipelineReady && !cancelled() && matches && intent.sequence &&
        RT::IntAt(effect,Layout::phase)==0 && validateCards && validateCards(model,intent.cards);
    Picked::SelectionSnapshot snapshot;
    snapshot.sequence=intent.sequence;snapshot.selection={id,group,intent.cards};
    auto value=accepted?TX::Make(token,context.source,snapshot):TX::Ref{};
    TX::Scoped scope(value,id);
    originalCreate(self,effect,entity,method);
    if(value && cancelled()) TX::Reject(value);
    if(value && !cancelled() && RT::onStockAccepted) {
        auto completed=RT::Capture(model,id,group,context.source,value->sequence);
        if(completed) RT::onStockAccepted(model,effect,completed,context,group);
    }
    if(value) { RT::CompleteSubmission(token,id,group,intent.sequence);TX::Complete(value); }
}
inline bool Initialize() {
    if(!Engine::SelectionLayout::Validate()) return false;
    return Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems","SelectTargetSystem")
        .GetMethod("CreateEffectsForTargets",2),Create,originalCreate);
}
}
