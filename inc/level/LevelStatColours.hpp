#pragma once
#include "level/LevelRuntime.hpp"
#include "level/LevelCostTint.hpp"
namespace CustomComponents::Level::StatColours {
using Color=VisualStyle::Color;
struct Binding {std::shared_ptr<Core::ManagedRuntime::Root> frame,view;Core::ModelRegistry::Token model=0,scene=0;int card=-1;};
inline Core::WeakObjectSettings<Binding> dimmers;
inline void(*originalApply)(Obj*,float,const void*)=nullptr;
inline void Bind(Obj* frame,Obj* view,Obj* model,int card){
    static auto metadata=BNM::Class("PvZCards.Game","CardViewComponent").GetField("Dimmer").cast<Obj*>();auto field=metadata;
    Obj* dimmer=view && field.IsValid()?field[view].Get():nullptr;if(!dimmer)return;
    auto previous=dimmers.Get(dimmer);if(previous.frame && previous.frame->Get()==frame && previous.view && previous.view->Get()==view && previous.card==card && Core::ModelRegistry::Get(previous.model)==model)return;
    dimmers.Set(dimmer,{std::make_shared<Core::ManagedRuntime::Root>(frame,true),std::make_shared<Core::ManagedRuntime::Root>(view,true),Core::ModelRegistry::Identify(model,true),Core::ModelRegistry::Identify(Engine::FrontendServices::Game(),true),card});
}
inline void UpdateCache(Obj* dimmer){
    auto binding=dimmers.Get(dimmer);Obj* frame=binding.frame?binding.frame->Get():nullptr;Obj* view=binding.view?binding.view->Get():nullptr;
    if(!CostTint::Alive(frame) || !CostTint::Alive(view))return;
    static auto type=BNM::Class("PvZCards.Game","CardFrameViewComponent");auto api=Presentation::CardStats::Metadata();if(!api.Valid())return;
    static auto cardMetadata=BNM::Class("PvZCards.Game","CardViewComponent").GetField("CardId").cast<int>();auto cardField=cardMetadata;
    const bool identity=cardField[view].Get()==binding.card && binding.scene==Core::ModelRegistry::Identify(Engine::FrontendServices::Game());
    Obj* card=identity?Engine::EntityAccess::Find(Core::ModelRegistry::Get(binding.model),binding.card):nullptr;
    const bool hand=card && Engine::EntityAccess::Component(card,"HandElement");
    static auto attackMetadata=type.GetMethod("PickAttackColor",2).cast<Color>();static auto healthMetadata=type.GetMethod("PickHealthColor",3).cast<Color>();auto attack=attackMetadata;auto health=healthMetadata;
    const Color attackColor=attack[frame](api.initialAttack[frame].Get(),api.attack[frame].Get());
    const Color healthColor=health[frame](api.initialHealth[frame].Get(),api.maximumHealth[frame].Get(),api.health[frame].Get());
    const Color costColor=hand && SelectedEntry(card)>=0?VisualStyle::CostTint:CostTint::originalPick(frame,api.initialCost[frame].Get(),api.cost[frame].Get(),nullptr);
    static auto attackTextMetadata=type.GetField("AttackUI").cast<Obj*>();static auto healthTextMetadata=type.GetField("HealthUI").cast<Obj*>();static auto costTextMetadata=type.GetField("SunCostUI").cast<Obj*>();
    auto attackField=attackTextMetadata;auto healthField=healthTextMetadata;auto costField=costTextMetadata;Obj* attackText=attackField[frame].Get();Obj* healthText=healthField[frame].Get();Obj* costText=costField[frame].Get();
    static auto dimmerType=BNM::Class("PvZCards.Game","Dimmer");
    static auto labelsMetadata=dimmerType.GetField("TextMeshPros").cast<BNM::Structures::Mono::List<Obj*>*>();static auto colorsMetadata=dimmerType.GetField("TextMeshProColors").cast<BNM::Structures::Mono::List<Color>*>();auto labelsField=labelsMetadata;auto colorsField=colorsMetadata;
    auto* labels=labelsField[dimmer].Get();auto* colors=colorsField[dimmer].Get();
    if(!Engine::EntityAccess::Valid(labels) || !colors || !colors->items || colors->size<0 || colors->size>int(colors->items->capacity))return;
    // Replace the undimmed baseline, never an already-dimmed colour. Native
    // ApplyDim still owns the interpolation, alpha and every unrelated label.
    for(int i=0;i<std::min(labels->size,colors->size);++i){Obj* label=labels->items->m_Items[i];if(label==attackText)colors->items->m_Items[i]=attackColor;else if(label==healthText)colors->items->m_Items[i]=healthColor;else if(label==costText)colors->items->m_Items[i]=costColor;}
    if(!identity || !hand)dimmers.Set(dimmer,{},false);
}
static void Apply(Obj* self,float amount,const void* method){UpdateCache(self);originalApply(self,amount,method);}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Game","Dimmer").GetMethod("ApplyDim",1),Apply,originalApply);}
}
