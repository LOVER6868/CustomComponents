#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline unsigned trickIdentityEvents = 0;
// PlayOneTimeEffectSystem::SetupTargetsComponents (one PlayCardEffect).
// Read-only snapshot around the original call. No target mutation.
using ReviveSetupTargetsFn=void(*)(Obj*,Obj*,const void*);
inline ReviveSetupTargetsFn originalReviveSetupTargets=nullptr;
inline BNM::Field<Obj*> reviveSetupModelField;
inline BNM::Field<int> reviveSetupLocationField;
inline unsigned reviveSetupAuditCalls=0;
inline constexpr bool kReviveStripCastingPrimaryTrialEnabled=true; // scoped to verified PickFromThree revive PlayTrigger; removes only synthetic casting-zone PrimaryTargets
inline BNM::Field<int> reviveSetupSourceField;
inline BNM::Field<int> reviveEffectOwnerSourceField;
inline BNM::Method<Obj*> reviveSetupRemoveComponent;
inline BNM::Class reviveSetupPrimaryTargetsClass;
inline BNM::Field<int> reviveGenericPrimarySelectionType;
inline unsigned reviveSetupTrialEvents=0;
// Run after stock SetupTargetsComponents. Generic signature: Burst card, first
// PlayTrigger for that card, PickFromThree primary filter, revive descriptor.
// Other card effects and later PlayTriggers are left untouched.
inline void ReviveStripCastingPrimaryTrial(Obj* system,Obj* play,int location) {
    if(!kReviveStripCastingPrimaryTrialEnabled) return;
    const unsigned event=++reviveSetupTrialEvents;
    const bool trace=Diagnostics::kSelectionTrace && event<=500;
    if(!system || !play || location<0) {
        if(trace) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: REJECT event=%u reason=invalid-arguments system=%p play=%p location=%d",event,system,play,location);
        return;
    }
    Obj* model=reviveSetupModelField.IsValid()?reviveSetupModelField[system].Get():nullptr;
    auto* entities=(model && trickEntitiesField.IsValid())?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items)) {
        if(trace) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: REJECT event=%u reason=entities-unavailable model=%p entities=%p",event,model,entities);
        return;
    }
    const int sourceId=reviveSetupSourceField.IsValid()?reviveSetupSourceField[play].Get():-1;
    Obj* source=nullptr;
    for(int i=0;trace && i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];
        if(e && trickEntityIdField.IsValid() && trickEntityIdField[e].Get()==sourceId){source=e;break;}
    }
    Obj* card=nullptr;
    if(source && TargetBridge::getComponent.IsValid() && TargetBridge::cardClass.IsValid()) {
        const auto ex=BNM::TryInvoke([&]{card=TargetBridge::getComponent[source](TargetBridge::cardClass.GetMonoType());});
        if(ex.IsValid())card=nullptr;
    }
    const int guid=(card && TargetBridge::databaseCardGuid.IsValid())?TargetBridge::databaseCardGuid[card].Get():-1;
    if(trace) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: SOURCE event=%u location=%d sourceId=%d found=%d card=%d guid=%d sourceField=%d guidField=%d entityCount=%d",event,location,sourceId,source?1:0,card?1:0,guid,reviveSetupSourceField.IsValid()?1:0,TargetBridge::databaseCardGuid.IsValid()?1:0,entities->size);
    // PlayCardEffect.SourceId is -1 in observed setup calls; it is NOT a safe
    // ownership signal. Do not reject the whole call based on that field.
    // Verify the EffectEntity component on EACH candidate independently.
    if(trace) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: SOURCE_NOTE event=%u playSourceId=%d playGuid=%d ownership=per-effect-EffectEntity.SourceId",event,sourceId,guid);
    int changed=0, eligible=0, descriptorMatches=0, primaryMatches=0, playTriggerMatches=0;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i]; if(!e)continue;
        auto field=trickEntityComponentsField;
        auto* comps=field.IsValid()?field[e].Get():nullptr;
        if(!comps || comps->size<0 || comps->size>128 || (comps->size && !comps->items))continue;
        bool revive=false,primaryFilter=false,secondaryFilter=false;
        Obj* primary=nullptr; bool hasSecondaryTargets=false;
        for(int j=0;j<comps->size;++j){
            Obj* c=comps->items->m_Items[j];if(!c)continue;
            const auto* klass=BNM::Class(c).GetClass();const char* n=klass?klass->name:nullptr;
            if(!n)continue;
            if((!std::strcmp(n,"ReturnFromDiscardPileToLaneEffectDescriptor") || !std::strcmp(n,"PlayFromDeckToLaneEffectDescriptor")))revive=true;
            else if(!std::strcmp(n,"PrimaryTargetFilter"))primaryFilter=true;
            else if(!std::strcmp(n,"SecondaryTargetFilter"))secondaryFilter=true;
            else if(!std::strcmp(n,"PrimaryTargets"))primary=c;
            else if(!std::strcmp(n,"SecondaryTargets"))hasSecondaryTargets=true;
        }
        if(!revive || !primaryFilter || !secondaryFilter || !primary || hasSecondaryTargets)continue;
        ++descriptorMatches;
        const int effectId=trickEntityIdField.IsValid()?trickEntityIdField[e].Get():-1;
        Obj* ownerComponent=nullptr;
        for(int j=0;j<comps->size;++j) {
            Obj* c=comps->items->m_Items[j]; if(!c)continue;
            const auto* cls=BNM::Class(c).GetClass();
            if(cls && cls->name && !std::strcmp(cls->name,"EffectEntity")) {ownerComponent=c;break;}
        }
        const int ownerId=(ownerComponent && reviveEffectOwnerSourceField.IsValid())
            ?reviveEffectOwnerSourceField[ownerComponent].Get():-1;
        Obj* ownerEntity=nullptr;
        for(int k=0;k<entities->size && ownerId>=0;++k) {
            Obj* candidate=entities->items->m_Items[k];
            if(candidate && trickEntityIdField.IsValid() && trickEntityIdField[candidate].Get()==ownerId) {ownerEntity=candidate;break;}
        }
        Obj* ownerCard=nullptr;
        if(ownerEntity && TargetBridge::getComponent.IsValid() && TargetBridge::cardClass.IsValid()) {
            const auto ex=BNM::TryInvoke([&]{ownerCard=TargetBridge::getComponent[ownerEntity](TargetBridge::cardClass.GetMonoType());});
            if(ex.IsValid())ownerCard=nullptr;
        }
        const int ownerGuid=(ownerCard && TargetBridge::databaseCardGuid.IsValid())
            ?TargetBridge::databaseCardGuid[ownerCard].Get():-1;
        // Only actual burst/trick source cards qualify. Checking the source's
        // Burst marker excludes fighters, including those with an identical
        // PickFromThree + revive descriptor on one of their abilities.
        bool sourceIsBurst=false;
        if(ownerEntity) {
            auto sourceComponentsField=trickEntityComponentsField;
            auto* sourceComponents=sourceComponentsField.IsValid()?sourceComponentsField[ownerEntity].Get():nullptr;
            if(sourceComponents && sourceComponents->size>=0 && sourceComponents->size<=128 &&
               (!sourceComponents->size || sourceComponents->items)) {
                for(int k=0;k<sourceComponents->size;++k) {
                    Obj* component=sourceComponents->items->m_Items[k];if(!component)continue;
                    auto* cls=BNM::Class(component).GetClass();
                    if(cls && cls->name && !std::strcmp(cls->name,"Burst")) {sourceIsBurst=true;break;}
                }
            }
        }
        // Require the ORIGINAL first-stage primary selector to be PickFromThree,
        // not a stock/manual selector or an unrelated revival effect.
        Obj* primaryFilterComponent=nullptr;
        bool playTrigger=false;
        for(int j=0;j<comps->size;++j) {
            Obj* component=comps->items->m_Items[j];if(!component)continue;
            auto* cls=BNM::Class(component).GetClass();const char* n=cls?cls->name:nullptr;
            if(!n)continue;
            if(!std::strcmp(n,"PrimaryTargetFilter"))primaryFilterComponent=component;
            else if(!std::strcmp(n,"PlayTrigger"))playTrigger=true;
        }
        const int primarySelection=(primaryFilterComponent && reviveGenericPrimarySelectionType.IsValid())
            ?reviveGenericPrimarySelectionType[primaryFilterComponent].Get():-1;
        const bool pick3Primary=EnumModes::IsPickFromThree(primarySelection);
        // Entity IDs reflect the created-effect order in the observed game.
        // Fail closed whenever an earlier PlayTrigger exists for this card,
        // even if it has a different descriptor or targets.
        bool firstPlayTrigger=playTrigger && effectId>=0 && ownerId>=0;
        if(firstPlayTrigger) {
            for(int k=0;k<entities->size;++k) {
                Obj* other=entities->items->m_Items[k];if(!other || other==e)continue;
                const int otherId=trickEntityIdField.IsValid()?trickEntityIdField[other].Get():-1;
                if(otherId<0 || otherId>=effectId)continue;
                auto otherField=trickEntityComponentsField;
                auto* otherComps=otherField.IsValid()?otherField[other].Get():nullptr;
                if(!otherComps || otherComps->size<0 || otherComps->size>128 || (otherComps->size && !otherComps->items))continue;
                bool otherPlay=false;Obj* otherOwner=nullptr;
                for(int j=0;j<otherComps->size;++j) {
                    Obj* c=otherComps->items->m_Items[j];if(!c)continue;
                    auto* cl=BNM::Class(c).GetClass();const char* n=cl?cl->name:nullptr;
                    if(!n)continue;
                    if(!std::strcmp(n,"PlayTrigger"))otherPlay=true;
                    else if(!std::strcmp(n,"EffectEntity"))otherOwner=c;
                }
                if(otherPlay && otherOwner && reviveEffectOwnerSourceField.IsValid() &&
                   reviveEffectOwnerSourceField[otherOwner].Get()==ownerId) {firstPlayTrigger=false;break;}
            }
        }
        const bool signature=ownerGuid>=0 && sourceIsBurst && pick3Primary && firstPlayTrigger;
        if(trace && descriptorMatches<=24)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: EFFECT_OWNER event=%u effect=%d ownerSourceId=%d ownerGuid=%d burst=%d pick3=%d selection=%d firstPlay=%d scoped=%d",event,effectId,ownerId,ownerGuid,sourceIsBurst?1:0,pick3Primary?1:0,primarySelection,firstPlayTrigger?1:0,signature?1:0);
        if(!signature) {
            if(trace && descriptorMatches<=24) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: GUARD event=%u entity=%d reason=generic-signature-rejected guid=%d burst=%d pick3=%d firstPlay=%d",event,effectId,ownerGuid,sourceIsBurst?1:0,pick3Primary?1:0,firstPlayTrigger?1:0);
            continue;
        }
        if(!primary || hasSecondaryTargets) {
            if(trace && descriptorMatches<=16) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: GUARD event=%u entity=%d reason=target-components primary=%d secondary=%d",event,effectId,primary?1:0,hasSecondaryTargets?1:0);
            continue;
        }
        auto idsField=BNM::Class(primary).GetField("TargetEntityIds").cast<IntList*>();
        auto* ids=idsField.IsValid()?idsField[primary].Get():nullptr;
        const int count=ids?ids->size:-1;
        const int first=(ids && count>0 && ids->items)?ids->items->m_Items[0]:-1;
        if(!ids || count!=1 || !ids->items || first!=location) {
            if(trace && descriptorMatches<=16) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: GUARD event=%u entity=%d reason=primary-ids count=%d first=%d location=%d",event,effectId,count,first,location);
            continue;
        }
        ++primaryMatches;
        ++eligible;
        // Scope further: only the effect directly tied to the card's PlayTrigger.
        // No general removal for other revive entities from other cards.
        bool fromSource=false;
        for(int j=0;j<comps->size;++j){
            Obj* c=comps->items->m_Items[j];if(!c)continue;
            const auto* klass=BNM::Class(c).GetClass();
            if(klass && klass->name && !std::strcmp(klass->name,"PlayTrigger"))fromSource=true;
        }
        if(trace) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: GUARD event=%u entity=%d guid=%d primary=%d playTrigger=%d eligible=%d removeMethod=%d primaryType=%d",event,effectId,ownerGuid,first,fromSource?1:0,1,reviveSetupRemoveComponent.IsValid()?1:0,reviveSetupPrimaryTargetsClass.IsValid()?1:0);
        if(!fromSource || !reviveSetupRemoveComponent.IsValid() || !reviveSetupPrimaryTargetsClass.IsValid())continue;
        ++playTriggerMatches;
        Obj* removed=nullptr;
        const auto ex=BNM::TryInvoke([&]{removed=reviveSetupRemoveComponent[e](reviveSetupPrimaryTargetsClass.GetMonoType());});
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: entity=%d guid=%d source=%d location=%d attempted=1 exception=%d removed=%d",effectId,ownerGuid,ownerId,location,ex.IsValid()?1:0,removed?1:0);
        if(!ex.IsValid() && removed)++changed;
    }
    if(trace || changed>0)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: SUMMARY event=%u guid=%d source=%d location=%d descriptorMatches=%d primaryMatches=%d eligible=%d playTriggerMatches=%d removed=%d enabled=1",event,guid,sourceId,location,descriptorMatches,primaryMatches,eligible,playTriggerMatches,changed);
}

inline void ReviveSetupSnapshot(const char* stage,Obj* system,Obj* play,int location) {
    Obj* model=reviveSetupModelField.IsValid()?reviveSetupModelField[system].Get():nullptr;
    auto* entities=(model && trickEntitiesField.IsValid())?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_SETUP_TARGETS: %s location=%d model=%p entities=unavailable",stage,location,model);
        return;
    }
    int matched=0;
    for(int i=0;i<entities->size && matched<24;++i) {
        Obj* entity=entities->items->m_Items[i];
        if(!entity) continue;
        auto field=trickEntityComponentsField;
        auto* comps=field.IsValid()?field[entity].Get():nullptr;
        if(!comps || comps->size<0 || comps->size>128 || (comps->size && !comps->items)) continue;
        bool primary=false,secondary=false,revive=false;
        int firstPrimary=-1,firstSecondary=-1;
        for(int j=0;j<comps->size;++j) {
            Obj* c=comps->items->m_Items[j]; if(!c)continue;
            auto* cl=BNM::Class(c).GetClass();
            const char* name=cl?cl->name:nullptr; if(!name)continue;
            if(!std::strcmp(name,"PrimaryTargetFilter"))primary=true;
            if(!std::strcmp(name,"SecondaryTargetFilter"))secondary=true;
            if((!std::strcmp(name,"ReturnFromDiscardPileToLaneEffectDescriptor") || !std::strcmp(name,"PlayFromDeckToLaneEffectDescriptor")))revive=true;
            if(!std::strcmp(name,"PrimaryTargets") || !std::strcmp(name,"SecondaryTargets")) {
                auto idsField=BNM::Class(c).GetField("TargetEntityIds").cast<IntList*>();
                auto* ids=idsField.IsValid()?idsField[c].Get():nullptr;
                int first=(ids && ids->size>0 && ids->size<100 && ids->items)?ids->items->m_Items[0]:-1;
                if(!std::strcmp(name,"PrimaryTargets"))firstPrimary=first;
                else firstSecondary=first;
            }
        }
        if(!revive)continue;
        ++matched;
        int id=trickEntityIdField.IsValid()?trickEntityIdField[entity].Get():-1;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_SETUP_TARGETS: %s entity=%d ptr=%p location=%d primaryFilter=%d secondaryFilter=%d revive=%d firstPrimary=%d firstSecondary=%d",stage,id,entity,location,primary,secondary,revive,firstPrimary,firstSecondary);
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_SETUP_TARGETS: %s location=%d entities=%d reviveEntities=%d",stage,location,entities->size,matched);
}
inline void ReviveSetupTargetsAudit(Obj* system,Obj* play,const void* method) {
    const int location=(play && reviveSetupLocationField.IsValid())?reviveSetupLocationField[play].Get():-1;
    const bool audit=Diagnostics::kSelectionTrace && play && ++reviveSetupAuditCalls<=90;
    // PvP boundary trace: no extra managed calls, no target mutation.
    // Each thread has its own counter so nested/remote callbacks remain distinguishable.
    static thread_local unsigned traceCount = 0;
    const unsigned traceSequence = ++traceCount;
    const bool traceBoundary = Diagnostics::kSelectionTrace && traceSequence <= 96;
    if (traceBoundary) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_BOUNDARY: phase=SETUP_ENTER seq=%u system=%p play=%p location=%d stripEnabled=%d original=%d",
        traceSequence, system, play, location, kReviveStripCastingPrimaryTrialEnabled ? 1 : 0,
        originalReviveSetupTargets ? 1 : 0);
    if(audit) ReviveSetupSnapshot("BEFORE",system,play,location);
    if (traceBoundary) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_BOUNDARY: phase=STOCK_ENTER seq=%u", traceSequence);
    if(originalReviveSetupTargets)originalReviveSetupTargets(system,play,method);
    if (traceBoundary) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_BOUNDARY: phase=STOCK_EXIT seq=%u", traceSequence);
    if(audit) ReviveSetupSnapshot("AFTER",system,play,location);
    if (traceBoundary) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_BOUNDARY: phase=STRIP_ENTER seq=%u enabled=%d", traceSequence,
        kReviveStripCastingPrimaryTrialEnabled ? 1 : 0);
    ReviveStripCastingPrimaryTrial(system,play,location);
    if (traceBoundary) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_BOUNDARY: phase=STRIP_EXIT seq=%u", traceSequence);
}
}
