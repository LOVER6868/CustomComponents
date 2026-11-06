#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline thread_local unsigned pickFromThreeTargetAuditDepth = 0;
inline unsigned pickFromThreeTargetAuditSamples = 0;
inline BNM::Field<int> pickFromThreeAuditEffectPrimary;
inline BNM::Field<int> pickFromThreeAuditEffectSecondary;
inline BNM::Field<int> pickFromThreeAuditReturnOrder;
inline constexpr int kReturnToLaneCarrierMarker = 0x6F000003;

inline void PickFromThreeAuditIds(const char* stage, Obj* list) {
    if (!list) { CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: %s ids=null",stage); return; }
    // Both primary and secondary creator methods take System.Collections.Generic.List<int>.
    auto* ids=reinterpret_cast<IntList*>(list);
    const int count=ids->size;
    if(count<0 || count>128 || (count && !ids->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: %s ids=%p count=unreadable",stage,list);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: %s count=%d first=%d second=%d third=%d",stage,count,
             count>0?ids->items->m_Items[0]:-1,count>1?ids->items->m_Items[1]:-1,count>2?ids->items->m_Items[2]:-1);
}
// Read-only ownership inventory. Component identity is read from the actual
// effect entity; the shared focus GUID is only a sampling hint, never proof.
inline unsigned pickFromThreeOwnerSamples=0;
// Read-only observation of stock-owned context at a synchronous effect-creation
// boundary. No managed invokes, no pooled-list inspection, no retained pointers.
// Offsets match dump.cs: EntityQueryContext(Model 0x10, EffectContext 0x18,
// SelfId 0x20); EffectContext(Target 0x10, Source 0x14, EffectEntity 0x18,
// Springboard 0x1C, EffectData 0x20).
inline unsigned pick3StockContextAuditSamples=0;
inline void AuditPick3StockQueryContext(const char* stage, Obj* effectEntity, Obj* context) {
    if (!context || pick3StockContextAuditSamples >= 96) return;
    ++pick3StockContextAuditSamples;
    const auto* contextBytes = reinterpret_cast<const std::uint8_t*>(context);
    Obj* model = *reinterpret_cast<Obj* const*>(contextBytes + 0x10);
    Obj* effectContext = *reinterpret_cast<Obj* const*>(contextBytes + 0x18);
    const int selfId = *reinterpret_cast<const int*>(contextBytes + 0x20);
    if (!effectContext) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_STOCK_QUERY_CONTEXT: phase=%s focusGuid=%d effect=%p ctx=%p model=%p self=%d effectContext=null", stage, trickSecondaryFocusGuid, effectEntity, context, model, selfId);
        return;
    }
    const auto* effectBytes = reinterpret_cast<const std::uint8_t*>(effectContext);
    const int targetId = *reinterpret_cast<const int*>(effectBytes + 0x10);
    const int sourceId = *reinterpret_cast<const int*>(effectBytes + 0x14);
    const int effectEntityId = *reinterpret_cast<const int*>(effectBytes + 0x18);
    const int springboardId = *reinterpret_cast<const int*>(effectBytes + 0x1C);
    Obj* effectData = *reinterpret_cast<Obj* const*>(effectBytes + 0x20);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_STOCK_QUERY_CONTEXT: phase=%s focusGuid=%d effect=%p ctx=%p model=%p self=%d source=%d target=%d effectEntityId=%d springboard=%d effectData=%p validationModel=%p validationCard=%d modelMatch=%d", stage, trickSecondaryFocusGuid, effectEntity, context, model, selfId, sourceId, targetId, effectEntityId, springboardId, effectData, pick3CurrentValidationModel, pick3CurrentValidationCard, pick3CurrentValidationModel ? (model == pick3CurrentValidationModel ? 1 : 0) : -1);
}
inline void PickFromThreeOwnerAudit(const char* stage, Obj* effect, Obj* context, Obj* result=nullptr) {
    if(!effect || ++pickFromThreeOwnerSamples>220) return;
    const auto entityClass=BNM::Class(effect);
    auto componentsField=entityClass.GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
    auto idField=entityClass.GetField("<Id>k__BackingField").cast<int>();
    auto* components=componentsField.IsValid()?componentsField[effect].Get():nullptr;
    const int entityId=idField.IsValid()?idField[effect].Get():-1;
    bool primary=false, secondary=false, revive=false, move=false;
    int componentCount=-1;
    if(components && components->size>=0 && components->size<=128 &&
       (components->size==0 || components->items)) {
        componentCount=components->size;
        for(int i=0;i<components->size;++i) {
            Obj* component=components->items->m_Items[i];
            if(!component) continue;
            auto* cls=BNM::Class(component).GetClass();
            const char* name=(cls && cls->name)?cls->name:"?";
            if(!std::strcmp(name,"PrimaryTargetFilter")) primary=true;
            if(!std::strcmp(name,"SecondaryTargetFilter")) secondary=true;
            if(!std::strcmp(name,"ReturnFromDiscardPileToLaneEffectDescriptor")) revive=true;
            if(!std::strcmp(name,"MoveCardToLanesEffectDescriptor")) move=true;
            if(pickFromThreeOwnerSamples<=65)
                CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_OWNER: COMPONENT stage=%s entity=%d idx=%d type=%s",stage,entityId,i,name);
        }
    }
    // Unknown collection layouts are reported, never cast to List<> blindly.
    int resultCount=-1;
    if(result) {
        auto count=BNM::Class(result).GetMethod("get_Count",0).cast<int>();
        if(count.IsValid()) {
            auto ex=BNM::TryInvoke([&]{ resultCount=count[result](); });
            if(ex.IsValid() || resultCount<0 || resultCount>4096) resultCount=-2;
        }
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_OWNER: %s entity=%d ptr=%p ctx=%p focusHint=%d components=%d primaryFilter=%d secondaryFilter=%d revive=%d move=%d result=%p resultCount=%d",
             stage,entityId,effect,context,trickSecondaryFocusGuid,componentCount,
             primary?1:0,secondary?1:0,revive?1:0,move?1:0,result,resultCount);
}
// EffectEntityConverter uses an IList<int> (not necessarily a List<int>).
using PickFromThreeConverterFn=Obj*(*)(Obj*,Obj*,Obj*,Obj*,const void*);
inline PickFromThreeConverterFn originalPickFromThreeConverter=nullptr;
inline Obj* PickFromThreeConverterAudit(Obj* self,Obj* effect,Obj* targets,Obj* context,const void* method) {
    const bool scoped=trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || pickFromThreeTargetAuditDepth!=0;
    if(scoped) { AuditPick3StockQueryContext("CONVERTER_ENTER", effect, context); PickFromThreeOwnerAudit("CONVERTER_ENTER",effect,context); }
    Obj* result=originalPickFromThreeConverter?originalPickFromThreeConverter(self,effect,targets,context,method):nullptr;
    if(scoped) { AuditPick3StockQueryContext("CONVERTER_EXIT", effect, context); PickFromThreeOwnerAudit("CONVERTER_EXIT",effect,context,result); }
    return result;
}
inline void PickFromThreeAuditEffect(const char* stage, Obj* effect) {
    if (!effect) { CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: %s effect=null",stage); return; }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: %s effect=%p primary=%d secondary=%d",stage,effect,
             pickFromThreeAuditEffectPrimary.IsValid()?pickFromThreeAuditEffectPrimary[effect].Get():-999,
             pickFromThreeAuditEffectSecondary.IsValid()?pickFromThreeAuditEffectSecondary[effect].Get():-999);
}
// Uncapped, effect-identity-scoped stock primary creator audit. Read-only:
// it does not change the query context, lists, filter, or return value.
inline bool Pick3RevivePrimaryCreatorScope(Obj* effect, int& entityId) {
    entityId = -1;
    if (!effect) return false;
    static auto idField = BNM::Class("PvZCards.Engine", "Entity")
        .GetField("<Id>k__BackingField").cast<int>();
    if (!idField.IsValid()) return false;
    entityId = idField[effect].Get();
    return entityId >= 0 && IsGenericPick3ReviveEffect(entityId);
}
inline void Pick3RevivePrimaryCreatorContext(const char* stage, Obj* effect,
                                               Obj* context, int effectId) {
    if (!context) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d effect=%p context=null", stage, effectId, effect);
        return;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(context);
    Obj* model = *reinterpret_cast<Obj* const*>(bytes + 0x10);
    Obj* effectContext = *reinterpret_cast<Obj* const*>(bytes + 0x18);
    const int self = *reinterpret_cast<const int*>(bytes + 0x20);
    if (!effectContext) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d effect=%p model=%p self=%d effectContext=null",stage,effectId,effect,model,self);
        return;
    }
    const auto* ec = reinterpret_cast<const std::uint8_t*>(effectContext);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d effect=%p model=%p self=%d source=%d target=%d contextEffect=%d springboard=%d effectData=%p", stage,effectId,effect,model,self,
             *reinterpret_cast<const int*>(ec+0x14), *reinterpret_cast<const int*>(ec+0x10),
             *reinterpret_cast<const int*>(ec+0x18), *reinterpret_cast<const int*>(ec+0x1c),
             *reinterpret_cast<Obj* const*>(ec+0x20));
}
inline void Pick3RevivePrimaryCreatorIds(const char* stage, Obj* targets, int effectId) {
    if (!targets) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d targets=null",stage,effectId);
        return;
    }
    // This parameter is stock List<int>; read its existing layout, never retain it.
    const auto* ids = reinterpret_cast<const IntList*>(targets);
    const int count=ids->size;
    if (count<0 || count>4096 || (count>0 && !ids->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d targets=%p count=unreadable",stage,effectId,targets);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d targets=%p count=%d first=%d second=%d third=%d",stage,effectId,targets,count,
             count>0?ids->items->m_Items[0]:-1,count>1?ids->items->m_Items[1]:-1,count>2?ids->items->m_Items[2]:-1);
}
inline void CaptureStockAutomaticPrimary(Obj* effect,Obj* context,IntList* ids,Obj* result) {
    if(effect && context && ids && result) {
        static auto id=BNM::Class("PvZCards.Engine","Entity").GetField("<Id>k__BackingField").cast<int>();
        Obj* model=TransactionRuntime::ObjectAt(context,0x10);
        const int effectId=id.IsValid()?id[effect].Get():-1;
        const int mode=model?TargetBridge::ReadActiveSelectionMode(effectId,model):-1;
        if(EnumModes::IsLivePickFromThree(mode) && ids && ids->items && ids->size==1 && ids->items->capacity>=1) {
            const int target=ids->items->m_Items[0];
            Obj* card=target>0?TargetBridge::getEntityById[model](target):nullptr;
            const int guid=CustomComponents::Queries::InDeckCardGuidOf(card);
            auto* effects=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
            const int source=effects && effects->items && effects->size>0?TransactionRuntime::IntAt(effects->items->m_Items[0],0x24):-1;
            TransactionRuntime::AttachAutomaticEffects(model,effectId,source,target,guid,
                TransactionRuntime::ReadContext(TransactionRuntime::ObjectAt(context,0x18)),result);
        }
    }
 }
inline Obj* PickFromThreePrimaryPreselectedAudit(Obj* self, Obj* effect, Obj* context, Obj* targets, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713) && ++pickFromThreeTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if (revive) { Pick3RevivePrimaryCreatorContext("PRESELECT_ENTER",effect,context,reviveId); Pick3RevivePrimaryCreatorIds("PRESELECT_TARGETS",targets,reviveId); }
    if(scoped) PickFromThreeOwnerAudit("PRESELECT_ENTER",effect,context);
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: PRIMARY_PRESELECT_ENTER effect=%p context=%p primaryTargets=%p",effect,context,targets);
    if(scoped) ++pickFromThreeTargetAuditDepth;
    Obj* result=originalPrimaryPreselectedAudit?originalPrimaryPreselectedAudit(self,effect,context,targets,method):nullptr;
    static auto autoIdsField=BNM::Class("PvZCards.Engine.Components","Targets").GetField("TargetEntityIds").cast<IntList*>();
    CaptureStockAutomaticPrimary(effect,context,targets && autoIdsField.IsValid()?autoIdsField[targets].Get():nullptr,result);
    if(revive) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=PRESELECT_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) PickFromThreeOwnerAudit("PRESELECT_EXIT",effect,context,result);
    if(scoped) { --pickFromThreeTargetAuditDepth; CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: PRIMARY_PRESELECT_EXIT effects=%p",result); }
    return result;
}
// Read-only audit: never mutate the preselected primary list. The previous
// substitution produced primary == secondary == selected discard entity, and
// the return-to-lane effect ran before any lane selection.
inline Obj* PickFromThreePrimaryListAudit(Obj* self, Obj* targets, Obj* effect, Obj* context, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || pickFromThreeTargetAuditDepth!=0) && ++pickFromThreeTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if(revive) { Pick3RevivePrimaryCreatorContext("PRIMARY_LIST_ENTER",effect,context,reviveId); Pick3RevivePrimaryCreatorIds("PRIMARY_LIST_TARGETS",targets,reviveId); }
    if(scoped) PickFromThreeOwnerAudit("PRIMARY_LIST_ENTER",effect,context);
    if(scoped) { CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: PRIMARY_LIST_ENTER effect=%p context=%p",effect,context); PickFromThreeAuditIds("PRIMARY_LIST_IDS",targets); ++pickFromThreeTargetAuditDepth; }
    Obj* result=originalPrimaryTargetListAudit?originalPrimaryTargetListAudit(self,targets,effect,context,method):nullptr;
    CaptureStockAutomaticPrimary(effect,context,reinterpret_cast<IntList*>(targets),result);
    if(revive) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=PRIMARY_LIST_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) PickFromThreeOwnerAudit("PRIMARY_LIST_EXIT",effect,context,result);
    if(scoped) { --pickFromThreeTargetAuditDepth; CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: PRIMARY_LIST_EXIT effects=%p",result); }
    return result;
}
// Trace the stock primary creator's *choice boundary*, including whether the
// Burst preselected path ever enters the normal primary selection creator.
using PrimaryNormalAuditFn = Obj*(*)(Obj*, Obj*, Obj*, const void*);
using PrimaryMethodAuditFn = Obj*(*)(Obj*, Obj*, Obj*, Obj*, Obj*, const void*);
inline PrimaryNormalAuditFn originalPrimaryNormalAudit=nullptr;
inline PrimaryMethodAuditFn originalPrimaryMethodAudit=nullptr;
inline Obj* PickFromThreePrimaryNormalAudit(Obj* self, Obj* effect, Obj* context, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || pickFromThreeTargetAuditDepth!=0) && ++pickFromThreeTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if(revive) Pick3RevivePrimaryCreatorContext("NORMAL_ENTER",effect,context,reviveId);
    if(scoped) PickFromThreeOwnerAudit("NORMAL_ENTER",effect,context);
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PRIMARY_BRANCH: NORMAL_ENTER effect=%p context=%p",effect,context);
    Obj* result=originalPrimaryNormalAudit?originalPrimaryNormalAudit(self,effect,context,method):nullptr;
    if(revive) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=NORMAL_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) PickFromThreeOwnerAudit("NORMAL_EXIT",effect,context,result);
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PRIMARY_BRANCH: NORMAL_EXIT effects=%p",result);
    return result;
}
inline Obj* PickFromThreePrimaryMethodAudit(Obj* self, Obj* effect, Obj* context, Obj* targets, Obj* filter, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || pickFromThreeTargetAuditDepth!=0) && ++pickFromThreeTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if(revive) {
        Pick3RevivePrimaryCreatorContext("METHOD_ENTER",effect,context,reviveId);
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=METHOD_INPUT entity=%d targets=%p filter=%p",reviveId,targets,filter);
    }
    if(scoped) PickFromThreeOwnerAudit("METHOD_ENTER",effect,context);
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PRIMARY_BRANCH: METHOD_ENTER effect=%p context=%p targets=%p filter=%p",effect,context,targets,filter);
    Obj* result=originalPrimaryMethodAudit?originalPrimaryMethodAudit(self,effect,context,targets,filter,method):nullptr;
    if(revive) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=METHOD_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) PickFromThreeOwnerAudit("METHOD_EXIT",effect,context,result);
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PRIMARY_BRANCH: METHOD_EXIT effects=%p",result);
    return result;
}
// Bind only a uniquely matching, still-unbound stock secondary choice.
// Called after the real selection service processes a validated primary.
inline void BindCommittedPick3RevivePrimary(Obj* service, const TargetBridge::ScopedValidationRecipient& recipient);
// Defined below, after the stock queue field bindings are available.
inline void CorrectPick3ReviveRolesBeforeStockSelect(Obj* service, int effectId,
    int groupId, int sourceCard, int selectedCard, int ownerPlayer, bool verified);


// Native ARM64 ABI: SecondaryTargetingData is a 0x20-byte value type passed
// indirectly in x1. x2 is ICollection<Effect>. Do not treat the data as an
// Il2CppObject or assume its IEnumerable<int> is always a List<int>.
inline int Pick3CastingHandOnModel(Obj* model, int player);
struct Pick3SecondaryTargetingData {
    Obj* effectEntity;
    std::int32_t primaryId;
    std::int32_t padding;
    Obj* secondaryIds;
    std::int32_t context;
    std::int32_t tailPadding;
};
static_assert(sizeof(Pick3SecondaryTargetingData) == 0x20);
using Pick3AddEffectsWithSecondaryFn = void(*)(Obj*, Pick3SecondaryTargetingData*, Obj*, const void*);
inline Pick3AddEffectsWithSecondaryFn originalPick3AddEffectsWithSecondary = nullptr;
struct Pick3SecondaryCommitScope {
    int effect = -1, group = -1, source = -1, chosen = -1;
    Obj* model = nullptr;
    int player = -1;
};
inline thread_local Pick3SecondaryCommitScope pick3SecondaryCommitScope{};
struct Pick3SecondaryCommitGuard {
    Pick3SecondaryCommitScope old;
    explicit Pick3SecondaryCommitGuard(const Pick3SecondaryCommitScope& next)
        : old(pick3SecondaryCommitScope) { pick3SecondaryCommitScope = next; }
    ~Pick3SecondaryCommitGuard() { pick3SecondaryCommitScope = old; }
};
// The stock effect creator copies data.primaryId directly into Effect.TargetId
// (libil2cpp RVA 0x2216F78). Correct only when a single exact selection is
// verified AND the incoming secondary IDs are demonstrably lanes, not cards.
// If stock is still supplying the discarded card as a secondary target, do
// not produce a self-targeted revive; log the missing lane handoff instead.
inline void Pick3AddEffectsWithSecondary(Obj* self, Pick3SecondaryTargetingData* data,
                                         Obj* effects, const void* method) {
    const auto scope = pick3SecondaryCommitScope;
    // 1.7.24: observe the existing hook *before* any commit-scope filtering.
    // Do not dereference the effect entity without the exact scope and a valid
    // metadata field. No new detour, command ownership, or gameplay changes.
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
    static thread_local unsigned creatorSamples1724 = 0;
    const bool traceCreator1724 = creatorSamples1724 < 64;
    if (traceCreator1724) {
        ++creatorSamples1724;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PREPLAY1724: stage=HOOK_ENTER scopeEffect=%d scopeGroup=%d scopeSource=%d chosen=%d dataPresent=%d entityPresent=%d originalPrimary=%d secondaryPresent=%d",
            scope.effect, scope.group, scope.source, scope.chosen,
            int(data != nullptr), int(data && data->effectEntity != nullptr),
            data ? data->primaryId : -1, int(data && data->secondaryIds != nullptr));
    }
    const char* creatorReason1724 = "NO_COMMIT_SCOPE";
    int creatorDataEffect1724 = -1;
    if (scope.effect > 0)
        creatorReason1724 = (!data || !data->effectEntity) ? "NO_EFFECT_DATA" : "EFFECT_UNAVAILABLE";
#endif
    if (scope.effect > 0 && data && data->effectEntity) {
        static auto entityIdField = BNM::Class("PvZCards.Engine", "Entity")
            .GetField("<Id>k__BackingField").cast<int>();
        const int dataEffect = entityIdField.IsValid() ? entityIdField[data->effectEntity].Get() : -1;
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
        creatorDataEffect1724 = dataEffect;
        creatorReason1724 = !entityIdField.IsValid() ? "EFFECT_ID_UNAVAILABLE" :
            (dataEffect != scope.effect ? "EFFECT_MISMATCH" :
            (!IsGenericPick3ReviveEffect(dataEffect) ? "NON_REVIVE_EFFECT" : "MATCHED"));
#endif
        if (dataEffect == scope.effect && IsGenericPick3ReviveEffect(dataEffect)) {
            int selected = -1, guid = -1;
            const bool verified = Picked::TryGetSingleLiveEntity(scope.effect, scope.group,
                selected, guid) && selected == scope.chosen;
            int listCount = -1;
            bool laneIds = false;
            bool listsChosenCard = false;
            // IEnumerable<int> can be an iterator: inspect only a confirmed
            // concrete List<int>. Never reinterpret arbitrary enumerable data.
            if (data->secondaryIds) {
                const auto* klass = BNM::Class(data->secondaryIds).GetClass();
                if (klass && klass->name && !std::strcmp(klass->name, "List`1")) {
                    auto* ids = reinterpret_cast<const IntList*>(data->secondaryIds);
                    if (ids->size >= 0 && ids->size <= 64 &&
                        (ids->size == 0 || ids->items)) {
                        listCount = ids->size;
                        laneIds = listCount > 0 && scope.model &&
                            TargetBridge::getEntityById.IsValid() && TargetBridge::getComponent.IsValid();
                        static auto laneClass = BNM::Class("PvZCards.Engine.Components", "Lane");
                        for (int i = 0; i < listCount; ++i) {
                            const int id = ids->items->m_Items[i];
                            if (id == scope.chosen) listsChosenCard = true;
                            Obj* entity = nullptr;
                            Obj* lane = nullptr;
                            if (laneIds) {
                                auto err = BNM::TryInvoke([&]{
                                    entity = TargetBridge::getEntityById[scope.model](id);
                                });
                                if (!err.IsValid() && entity && laneClass.IsValid()) {
                                    err = BNM::TryInvoke([&]{
                                        lane = TargetBridge::getComponent[entity](laneClass.GetMonoType());
                                    });
                                }
                                if (err.IsValid() || !lane) laneIds = false;
                            }
                        }
                    }
                }
            }
            const int oldPrimary = data->primaryId;
            const int initialZone = Pick3VerifiedCastTargetFor(scope.model,scope.source,scope.effect);
            const bool zoneVerified = initialZone >= 0 && oldPrimary == initialZone;
            const auto provenance = RevivePairContract::Classify(
                oldPrimary, selected, initialZone);
            // Read-only provenance: scoped to the already existing stock creator
            // detour. No extra hooks, no pooled-object retention, no ID rewrite.
            // Never interpret an ALREADY_CHOSEN primary as a casting zone.
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
            static thread_local unsigned provenanceSamples = 0;
            if (provenanceSamples < 48) {
                ++provenanceSamples;
                CC_SELECTION_DIAG("CustomComponents/Pick3/PREPLAY1723: stage=SECONDARY_PRIMARY_PROVENANCE effect=%d group=%d source=%d player=%d original=%d chosen=%d verifiedCast=%d origin=%s selectedVerified=%d secondaryCount=%d lanesVerified=%d includesChosen=%d model=%p",
                    scope.effect, scope.group, scope.source, scope.player,
                    oldPrimary, scope.chosen, initialZone,
                    RevivePairContract::Name(provenance), int(verified),
                    listCount, int(laneIds), int(listsChosenCard), (void*)scope.model);
            }
#endif
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: SECONDARY_CREATOR_CONTEXT effect=%d group=%d source=%d player=%d model=%p validatedCast=%d original=%d chosen=%d verified=%d lanes=%d",
                scope.effect,scope.group,scope.source,scope.player,(void*)scope.model,initialZone,oldPrimary,selected,int(verified),int(laneIds));
            const bool canCorrect = verified && zoneVerified && laneIds &&
                !listsChosenCard && oldPrimary != selected;
            if (canCorrect) data->primaryId = selected;
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_SECONDARY_CREATOR: action=%s effect=%d group=%d source=%d chosen=%d dataPrimaryBefore=%d dataPrimaryAfter=%d zone=%d verified=%d laneList=%d secondaryCount=%d includesChosen=%d collection=%p",
                canCorrect ? "CORRECTED" : "SKIP", dataEffect, scope.group,
                scope.source, scope.chosen, oldPrimary, data->primaryId, initialZone,
                int(verified), int(laneIds), listCount, int(listsChosenCard), effects);
        }
    }
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
    if (traceCreator1724)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PREPLAY1724: stage=HOOK_CLASSIFY reason=%s scopeEffect=%d dataEffect=%d scopeGroup=%d beforeStockPrimary=%d",
            creatorReason1724, scope.effect, creatorDataEffect1724, scope.group,
            data ? data->primaryId : -1);
#endif
    if (originalPick3AddEffectsWithSecondary)
        originalPick3AddEffectsWithSecondary(self, data, effects, method);
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
    if (traceCreator1724)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PREPLAY1724: stage=CREATOR_RETURN reason=%s scopeEffect=%d dataEffect=%d primaryAfter=%d stockCalled=%d",
            creatorReason1724, scope.effect, creatorDataEffect1724,
            data ? data->primaryId : -1, int(originalPick3AddEffectsWithSecondary != nullptr));
#endif
}

inline void PickFromThreeSecondaryContextAudit(Obj* self, Obj* data, Obj* effect, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || pickFromThreeTargetAuditDepth!=0) && ++pickFromThreeTargetAuditSamples<=160;
    if(scoped) { CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: CONTEXT_SET_ENTER data=%p",data); PickFromThreeAuditEffect("CONTEXT_BEFORE",effect); }
    if(originalSecondaryContextAudit) originalSecondaryContextAudit(self,data,effect,method);
    // Read-only at this boundary. The target-role correction now happens
    // before the stock selection service is allowed to advance its queue.
    // Mutating here was unreliable: the selection can have been consumed.
    if(scoped) PickFromThreeAuditEffect("CONTEXT_AFTER",effect);
}

// Stock secondary-choice decision audit for the 712/713 PickFromThree handoff.
// The two ShouldAdd overloads have different ABIs. No game state is changed.
using SecondaryChooseDecision3Fn = bool(*)(Obj*, int, Obj*, Obj*, const void*);
using SecondaryChooseDecision2Fn = bool(*)(Obj*, Obj*, Obj*, const void*);
using AddSecondaryChoiceFn = void(*)(Obj*, Obj*, Obj*, Obj*, const void*);
using CreateTargetChoiceFn = Obj*(*)(Obj*, Obj*, int, Obj*, const void*);
inline SecondaryChooseDecision3Fn originalSecondaryChooseDecision3 = nullptr;
inline SecondaryChooseDecision2Fn originalSecondaryChooseDecision2 = nullptr;
inline AddSecondaryChoiceFn originalAddSecondaryChoice = nullptr;
inline CreateTargetChoiceFn originalCreateTargetChoice = nullptr;
inline thread_local unsigned pickFromThreeSecondaryDecisionDepth = 0;
inline unsigned pickFromThreeSecondaryDecisionSamples = 0;
inline bool PickFromThreeChoiceAuditEnabled() {
    return trickSecondaryFocusGuid == 712 || trickSecondaryFocusGuid == 713 || pickFromThreeTargetAuditDepth != 0;
}
inline int PickFromThreeAuditListCount(Obj* list) {
    if (!list) return -1;
    const auto* ids = reinterpret_cast<const IntList*>(list);
    return (ids->size >= 0 && ids->size <= 4096) ? ids->size : -2;
}
inline bool PickFromThreeChooseDecision3(Obj* self, int primaryId, Obj* filter, Obj* targets, const void* method) {
    const bool scoped = PickFromThreeChoiceAuditEnabled() && ++pickFromThreeSecondaryDecisionSamples <= 240;
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_CHOOSE_DECISION: ENTER overload=3 primary=%d filter=%p secondaryCount=%d", primaryId, filter, PickFromThreeAuditListCount(targets));
    if(scoped) ++pickFromThreeSecondaryDecisionDepth;
    const bool result=originalSecondaryChooseDecision3 ? originalSecondaryChooseDecision3(self,primaryId,filter,targets,method) : false;
    if(scoped) { --pickFromThreeSecondaryDecisionDepth; CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_CHOOSE_DECISION: EXIT overload=3 primary=%d secondaryCount=%d choose=%d",primaryId,PickFromThreeAuditListCount(targets),result?1:0); }
    return result;
}
inline bool PickFromThreeChooseDecision2(Obj* self, Obj* filter, Obj* targets, const void* method) {
    const bool scoped = (PickFromThreeChoiceAuditEnabled() || pickFromThreeSecondaryDecisionDepth!=0) && ++pickFromThreeSecondaryDecisionSamples <= 240;
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_CHOOSE_DECISION: ENTER overload=2 filter=%p targetCount=%d",filter,PickFromThreeAuditListCount(targets));
    const bool result=originalSecondaryChooseDecision2 ? originalSecondaryChooseDecision2(self,filter,targets,method) : false;
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_CHOOSE_DECISION: EXIT overload=2 targetCount=%d choose=%d",PickFromThreeAuditListCount(targets),result?1:0);
    return result;
}
// Unbounded, structurally scoped secondary-choice boundary audit.
// Deliberately do NOT defer stock effects here: there is no verified resume
// operation for the original queue or guaranteed matching target context yet.
// This diagnostic never retains objects or dereferences pooled lists.
inline bool Pick3IsReviveChoiceEffect(Obj* effectEntity) {
    if (!effectEntity) return false;
    auto id = BNM::Class("PvZCards.Engine", "Entity")
        .GetField("<Id>k__BackingField").cast<int>();
    if (!id.IsValid()) return false;
    const int effectId = id[effectEntity].Get();
    return effectId >= 0 && IsGenericPick3ReviveEffect(effectId);
}
inline thread_local unsigned pick3SecondaryChoiceDepth = 0;
inline void PickFromThreeAddSecondaryChoice(Obj* self, Obj* effectEntity, Obj* allEffects, Obj* context, const void* method) {
    const bool scoped = Pick3IsReviveChoiceEffect(effectEntity);
    if (scoped) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=ADD_ENTER effect=%p context=%p collection=%p primaryCommit=unknown continuation=stock", effectEntity, context, allEffects);
        ++pick3SecondaryChoiceDepth;
    }
    if (originalAddSecondaryChoice) originalAddSecondaryChoice(self,effectEntity,allEffects,context,method);
    if (scoped) {
        --pick3SecondaryChoiceDepth;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=ADD_EXIT effect=%p context=%p collection=%p continuation=stock", effectEntity,context,allEffects);
    }
}
inline Obj* PickFromThreeCreateTargetChoice(Obj* self, Obj* effectEntity, int phase, Obj* context, const void* method) {
    const bool scoped = pick3SecondaryChoiceDepth != 0 || Pick3IsReviveChoiceEffect(effectEntity);
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=CREATE_ENTER effect=%p choicePhase=%d context=%p nested=%u",effectEntity,phase,context,pick3SecondaryChoiceDepth);
    Obj* result=originalCreateTargetChoice ? originalCreateTargetChoice(self,effectEntity,phase,context,method) : nullptr;
    if(scoped) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=CREATE_EXIT effect=%p choicePhase=%d result=%p",effectEntity,phase,result);
    return result;
}

inline Obj* TrickFindSecondaryTrace(Obj* self, Obj* effect, Obj* context, const void* method) {
    const bool logging = trickSecondaryFocusGuid == 699 || trickSecondaryFocusGuid == 712 || trickSecondaryFocusGuid == 713 || trickSecondaryFocusGuid == 27;
    if (logging) CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SECONDARY: boundary=FIND_ENTER focusGuid=%d effect=%p context=%p stock=1", trickSecondaryFocusGuid, effect, context);
    Obj* result = originalFindSecondaryTrace ? originalFindSecondaryTrace(self, effect, context, method) : nullptr;
    if (logging) CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SECONDARY: boundary=FIND_EXIT focusGuid=%d effect=%p list=%p count=opaque stockReturned=1", trickSecondaryFocusGuid, effect, result);
    return result;
}
inline Obj* TrickCreateSecondaryTrace(Obj* self, Obj* targets, Obj* effect, Obj* context, const void* method) {
    const bool logging = trickSecondaryFocusGuid == 699 || trickSecondaryFocusGuid == 712 || trickSecondaryFocusGuid == 713 || trickSecondaryFocusGuid == 27;
    if(logging) { AuditPick3StockQueryContext("SECONDARY_ENTER", effect, context); PickFromThreeOwnerAudit("SECONDARY_ENTER",effect,context); }
    if (logging) { CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SECONDARY: boundary=CREATE_ENTER focusGuid=%d targets=%p effect=%p context=%p stock=1", trickSecondaryFocusGuid, targets, effect, context); PickFromThreeAuditIds("SECONDARY_CREATOR_PRIMARY_IDS",targets); }
    Obj* result = originalCreateSecondaryTrace ? originalCreateSecondaryTrace(self, targets, effect, context, method) : nullptr;
    if(logging) { AuditPick3StockQueryContext("SECONDARY_EXIT", effect, context); PickFromThreeOwnerAudit("SECONDARY_EXIT",effect,context,result); }
    if (logging) CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SECONDARY: boundary=CREATE_EXIT focusGuid=%d effects=%p count=opaque stockReturned=1", trickSecondaryFocusGuid, result);
    return result;
}
}
