#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
static void LocalSelectProcess(Obj* self,Obj* play,const void* method) {
    // Local prediction must not erase the wire carrier before RemoteService
    // serializes the same DTO. Stock sees order zero only within this call.
    struct RestoreWireOrder {
        Obj* play;int order;bool encoded;
        ~RestoreWireOrder(){if(encoded && playOrder.IsValid())playOrder[play].Set(order);}
    } wire{play,play && playOrder.IsValid()?playOrder[play].Get():0,false};
    wire.encoded=AdditionalSelection::Decode(wire.order)>=0;
    TransactionRuntime::Submission receiverSubmission;
    ScopedReceiveReviveRecognition receiveRecognitionScope(self, play);
    TargetBridge::ScopedValidationRecipient bridgeContext;
    bool useValidationBridge=false;
    // Patch 1.7.7: receiver-side mode bridge, scoped strictly to the native
    // Process() of a valid incoming live-PF3 stock Play. Peer B's filter is
    // normally mode 3/4, while peer A's overlay has temporarily set mode 1.
    Obj* validatedLiveFilter=nullptr;
    int validatedLiveMode=-1;
    int validatedLiveEffect=-1, validatedLiveGroup=-1;
    int validatedLiveCount=0;
    Obj* blocking=nullptr;  // Queue front shared with pre-stock replica role correction.
    Obj* submissionModel=nullptr;
    if (enabled && self && play && playType.IsValid() && playTargets.IsValid() &&
        playType[play].Get()==6 && localServiceEffects.IsValid() && localServiceFinder.IsValid()) {
        Obj* queue=localServiceEffects[self].Get();
        // No managed Peek() in inbound simulation either. Read only the first
        // concrete queue entry, preserving stock Process() if inspection fails.
        if (queue && effectQueueConcrete.IsValid() && BNM::IsA(queue,effectQueueConcrete) &&
            boundQueueEffects.IsValid()) {
            auto* entries=boundQueueEffects[queue].Get();
            if (entries && entries->size>0 && entries->size<=4096 && entries->items)
                blocking=entries->items->m_Items[0];
            if (NetworkTrace::Mode())
                CC_SELECTION_DIAG("CustomComponents/Pick3/INBOUND_QUEUE: count=%d first=%p noPeek=1",
                    entries ? entries->size : -1,static_cast<void*>(blocking));
        } else if (queue) {
            LOG_ERR("CustomComponents/Pick3/INBOUND_QUEUE: concrete-type-unavailable queue=%p",static_cast<void*>(queue));
        }
        // Selection diagnostic: inspect the physical queue front without Peek()
        // and without changing any queue, filter, play, or completion state.
        if (play && playType.IsValid() && playType[play].Get()==6) {
            int frontEffect=-1, frontGroup=-1, frontOwner=-1;
            bool frontChoose=false;
            if (blocking && liveEffectEntityId.IsValid() && liveEffectGroupId.IsValid()) {
                frontEffect=liveEffectEntityId[blocking].Get();
                frontGroup=liveEffectGroupId[blocking].Get();
                frontChoose=chooseBlockingClass.IsValid() && BNM::IsA(blocking,chooseBlockingClass);
                if (TargetBridge::blockingPlayerId.IsValid()) frontOwner=TargetBridge::blockingPlayerId[blocking].Get();
            }
            const auto* ids=playTargets[play].Get();
            const int count=ids && ids->size>=0 && ids->size<=32 && (ids->size==0 || ids->items) ? ids->size:-1;
            CC_SELECTION_DIAG("CustomComponents/Pick3/VALIDATION_AUDIT: phase=QUEUE_BEFORE service=%p queue=%p front=%p frontEffect=%d frontGroup=%d frontOwner=%d frontChoose=%d pendingEffect=%d pendingGroup=%d selectedPlayer=%d count=%d target0=%d target1=%d target2=%d cardId=%d",
                (void*)self,(void*)queue,(void*)blocking,frontEffect,frontGroup,frontOwner,int(frontChoose),lastEffect,lastGroup,
                playPlayer.IsValid()?playPlayer[play].Get():-1,count,
                count>0?ids->items->m_Items[0]:-1,count>1?ids->items->m_Items[1]:-1,count>2?ids->items->m_Items[2]:-1,
                playCard.IsValid()?playCard[play].Get():-1);
        }
        // The local service's injected finder uses the *same simulation model*
        // (prediction/received play), which may differ from the UI model.
        Obj* model=nullptr;
        Obj* finder=localServiceFinder.IsValid()?localServiceFinder[self].Get():nullptr;
        Obj* entityFinder=finder && TargetBridge::finderEntityFinder.IsValid()
            ? TargetBridge::finderEntityFinder[finder].Get():nullptr;
        if (entityFinder && TargetBridge::entityFinderModel.IsValid())
            model=TargetBridge::entityFinderModel[entityFinder].Get();
        submissionModel=model;
        if (blocking && model && chooseBlockingClass.IsValid() && BNM::IsA(blocking,chooseBlockingClass) &&
            TargetBridge::getEntityById.IsValid() && TargetBridge::getComponent.IsValid() &&
            TargetBridge::cardGuid.IsValid() && TargetBridge::selectionType.IsValid() &&
            TargetBridge::primaryFilterClass.IsValid()) {
            const int effect=liveEffectEntityId[blocking].Get();
            const int group=liveEffectGroupId[blocking].Get();
            Obj* effectEntity=nullptr;
            auto e=BNM::TryInvoke([&] {effectEntity=TargetBridge::getEntityById[model](effect);});
            Obj* filter=nullptr;
            if (!e.IsValid() && effectEntity) {
                e=BNM::TryInvoke([&] {filter=TargetBridge::getComponent[effectEntity](TargetBridge::primaryFilterClass.GetMonoType());});
            }
            const bool ownerMatches=playPlayer.IsValid() && TargetBridge::blockingPlayerId.IsValid() &&
                playPlayer[play].Get()==TargetBridge::blockingPlayerId[blocking].Get();
            int originalMode=filter && !e.IsValid() ? TargetBridge::selectionType[filter].Get() : -1;
            // A local popup normalizes its filter (3/4/5 -> Manual=1).
            // Recover the original mode only for the EXACT filter/effect/group.
            if (ownerMatches && originalMode==TargetBridge::kManualValue) {
                std::lock_guard<std::mutex> lock(TargetBridge::snapshotMutex);
                for (const auto& item:TargetBridge::runtimeOverrides) {
                    if (item.effectId!=effect || item.groupId!=group) continue;
                    // Preserve the exact existing reconstruction behavior
                    // for live-entity modes 3/4. DB5 additionally requires
                    // the original filter object to match, since a GUID
                    // must never be inferred from an unrelated recipient.
                    if (EnumModes::IsLivePickFromThree(item.originalSelectionType) ||
                        (item.originalSelectionType==EnumModes::kPickFromThreeCardDatabase &&
                         Core::SceneViewContext::getGCHandle(item.filterHandle)==filter)) {
                        originalMode=item.originalSelectionType;
                        break;
                    }
                }
                if (originalMode==TargetBridge::kManualValue)
                    for (const auto& item:TargetBridge::databaseQueryOverrides)
                        if (item.effectId==effect && item.groupId==group &&
                            Core::SceneViewContext::getGCHandle(item.filterRoot)==filter) {
                            originalMode=EnumModes::kPickFromThreeCardDatabase;
                            break;
                        }
            }
            if (Diagnostics::kSelectionTrace && playType.IsValid() && playType[play].Get()==6) {
                const auto* ids=playTargets[play].Get();
                const int count=ids && ids->size>=0 && ids->size<=32 && (ids->size==0 || ids->items)?ids->size:-1;
                CC_SELECTION_DIAG("CustomComponents/Pick3/VALIDATION_AUDIT: phase=FILTER_CONTEXT effect=%d group=%d expectedEffect=%d expectedGroup=%d frontMatch=%d effectEntity=%p filter=%p rawMode=%d recoveredMode=%d ownerMatches=%d serviceModel=%p inputModel=%p finder=%p error=%d",
                    effect,group,lastEffect,lastGroup,int(effect==lastEffect && group==lastGroup),
                    (void*)effectEntity,(void*)filter,filter && !e.IsValid()?TargetBridge::selectionType[filter].Get():-1,
                    originalMode,int(ownerMatches),(void*)model,(void*)Target(modelRoot),(void*)finder,int(e.IsValid()));
                for(int i=0;i<count && i<3;++i) {
                    const int id=ids->items->m_Items[i];
                    Obj* target=nullptr; int guid=-1;
                    const auto ex=BNM::TryInvoke([&]{target=TargetBridge::getEntityById[model](id);});
                    bool guidException=false;
                    if(!ex.IsValid() && target) {
                        const auto g=BNM::TryInvoke([&]{guid=TargetBridge::cardGuid(target);});
                        guidException=g.IsValid();
                    }
                    CC_SELECTION_DIAG("CustomComponents/Pick3/VALIDATION_AUDIT: phase=TARGET_IN_SERVICE effect=%d group=%d index=%d id=%d entity=%p guid=%d lookupException=%d guidException=%d pendingSnapshot=%d pendingGuidMatch=%d",
                        effect,group,i,id,(void*)target,guid,int(ex.IsValid()),int(guidException),
                        int(TargetBridge::StillEligible(effect,group,id)),
                        int(guid>0 && TargetBridge::StillEligibleWithGuid(effect,group,id,guid)));
                }
            }
            const bool db5=ownerMatches && originalMode==EnumModes::kPickFromThreeCardDatabase;
            const bool customLive=ownerMatches && EnumModes::IsLivePickFromThree(originalMode);
            if(customLive && playOrder.IsValid()) {
                const int extra=AdditionalSelection::Decode(playOrder[play].Get());
                if(extra>=0) {
                    auto* targets=playTargets[play].Get();
                    const int primary=targets && targets->items && targets->size==1?targets->items->m_Items[0]:-1;
                    if(!AdditionalSelection::Authorize(model,effect,group,primary,extra))
                        LOG_ERR("CustomComponents/AdditionalSelection: received additional target rejected effect=%d group=%d",effect,group);
                    playOrder[play].Set(0);
                }
            }
            if (db5) {
                // The genuine Play target is a real recipient. The GUID is
                // conveyed separately via CardId; NEVER treat Targets[0] as
                // a definition GUID or create a fake match entity.
                IntList* ids=playTargets[play].Get();
                const int chosenGuid=playCard.IsValid() ? playCard[play].Get() : -1;
                const int recipient=ids && ids->items && ids->size==1
                    ? ids->items->m_Items[0] : kNoEntity;
                Obj* recipientEntity=nullptr;
                if (recipient>=0) {
                    auto ex=BNM::TryInvoke([&] {recipientEntity=TargetBridge::getEntityById[model](recipient);});
                    if (ex.IsValid()) recipientEntity=nullptr;
                }
                // Eligibility is checked using the original, rooted database
                // query, even when the receiver already redirected the live
                // primary target query to AdditionalTargetQuery.
                Obj* originalQuery=TargetBridge::OriginalDatabaseQuery(filter,effect,group);
                const auto eligible=TargetBridge::EnumerateCardDatabase(originalQuery,effect,model,recipient);
                const bool validGuid=chosenGuid>0 && std::any_of(eligible.begin(),eligible.end(),
                    [chosenGuid](const auto& item){return item.cardGuid==chosenGuid;});
                const bool shapeValid=TargetBridge::numberOfTargets.IsValid() &&
                    TargetBridge::numberOfTargets[filter].Get()==1 &&
                    TargetBridge::additionalTargetType.IsValid() &&
                    TargetBridge::additionalTargetType[filter].Get()==1 &&
                    TargetBridge::onlyApplyToAdditionalTargets.IsValid() &&
                    TargetBridge::onlyApplyToAdditionalTargets[filter].Get();
                Obj* queryEffect=Engine::EntityAccess::Find(model,effect);
                Engine::PlacementQueryContext::Scope placementQuery({model,queryEffect,validGuid?Engine::BoardPlacement::Definition(chosenGuid,queryEffect):nullptr,recipient});
                const bool recipientAllowed=!AdditionalSelection::Interactive(AdditionalSelection::Get(filter).mode) ||
                    (validGuid && AdditionalSelection::Authorize(model,effect,group,recipient,recipient,Engine::BoardPlacement::Definition(chosenGuid,queryEffect)));
                if (recipientEntity && shapeValid && validGuid && recipientAllowed &&
                    TargetBridge::RedirectDatabaseTargetQuery(filter,effect,group)) {
                    // Only normalize during the exact synchronous stock
                    // Process() call, preserving the custom enum afterward.
                    // The service still validates the recipient normally.
                    const int modeBefore=TargetBridge::selectionType[filter].Get();
                    if (modeBefore!=TargetBridge::kManualValue)
                        TargetBridge::selectionType[filter].Set(TargetBridge::kManualValue);
                    std::uint64_t receiverSequence=0;
                    if (Picked::Store(effect,group,{{kNoEntity,chosenGuid}},"RX_DB",&receiverSequence)) {
                        Picked::TraceLedger("RX_DB_STAGED", effect, group, chosenGuid);
                        ++remotePickRebuilds;
                        if (NetworkTrace::Mode())
                            CC_SELECTION_DIAG("CustomComponents/Pick3/DB5_PVP: RX_VALID effect=%d group=%d guid=%d recipient=%d eligible=%zu",effect,group,chosenGuid,recipient,eligible.size());
                    } else LOG_ERR("CustomComponents/Pick3/DB5_PVP: RX_CACHE_FAILED effect=%d group=%d",effect,group);
                    // Call the original while the filter is in stock Manual
                    // mode and the legal recipient query is active.
                    if (originalLocalSelectProcess) {
                        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_ENTER", self);
                        CustomComponents::Systems::CustomTriggerSystem::ArmPick3QueueAudit("DB_SELECTION", effect, group);
                        ++CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
                        TransactionRuntime::ScopedSubmission selectedSubmission({effect,group,receiverSequence,{recipient},{{kNoEntity,chosenGuid,-1}}});
                        if(!TransactionRuntime::StageSubmission(TransactionRuntime::ModelOf(model,true),TransactionRuntime::submitted))
                            LOG_ERR("CustomComponents/PickFromThree: selection intent rejected; stock processing continues without guessed provenance");
                        originalLocalSelectProcess(self,play,method);
                        --CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
                        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_EXIT", self);
                    }
                    if (modeBefore!=TargetBridge::kManualValue)
                        TargetBridge::selectionType[filter].Set(modeBefore);
                    // The owning popup retains its override until normal
                    // FinishTransaction; receiver-only query roots are
                    // released once stock processing has returned.
                    if (modeBefore!=TargetBridge::kManualValue)
                        TargetBridge::CancelDatabaseOverrideForFilter(filter,effect,group,"remote-process-complete");
                    return;
                }
                LOG_ERR("CustomComponents/Pick3/DB5_PVP: RX_INVALID effect=%d group=%d guid=%d recipient=%d exists=%d shape=%d eligible=%d (original service will decide)",
                        effect,group,chosenGuid,recipient,recipientEntity?1:0,shapeValid?1:0,validGuid?1:0);
                // Fail closed for custom choice reconstruction, but do not
                // bypass or modify the game's stock validation.
            } else if (customLive) {
                IntList* ids=playTargets[play].Get();
                std::vector<Picked::SelectedCard> selected;
                if (ids && ids->items && ids->size>=1 && ids->size<=3) {
                    for(int i=0;i<ids->size;i++) {
                        int id=ids->items->m_Items[i];
                        Obj* entity=nullptr;
                        if (id<0) {selected.clear();break;}
                        auto x=BNM::TryInvoke([&] {entity=TargetBridge::getEntityById[model](id);});
                        int guid=-1;
                        if (!x.IsValid() && entity) x=BNM::TryInvoke([&] {guid=TargetBridge::cardGuid(entity);});
                        if (x.IsValid() || guid<=0) {selected.clear();break;}
                        selected.push_back({id,guid});
                    }
                }
                // Stock's repeated FindValidTargetsFromEffect can yield board
                // lanes even though the original initial Play validator found
                // real discarded-card targets. Scope that validated list to
                // this exact stock service call, not to the popup lifetime.
                if (selected.size()==1 && TargetBridge::blockingSourceId.IsValid() &&
                    TargetBridge::blockingPlayerId.IsValid() && ownerMatches &&
                    originalMode==EnumModes::kPickFromThreeUniqueGuidTargets &&
                    TargetBridge::resolveValidatedInitialCardTargets) {
                    const int source=TargetBridge::blockingSourceId[blocking].Get();
                    Obj* sourceEntity=nullptr; int sourceGuid=-1;
                    auto sx=BNM::TryInvoke([&]{sourceEntity=TargetBridge::getEntityById[model](source);});
                    if (!sx.IsValid() && sourceEntity)
                        sx=BNM::TryInvoke([&]{sourceGuid=TargetBridge::cardGuid(sourceEntity);});
                    if (!sx.IsValid() && IsGenericPick3ReviveBurst(source)) {
                        auto validated=TargetBridge::resolveValidatedInitialCardTargets(model, source, effect);
                        if (std::find(validated.begin(),validated.end(),selected[0].entityId)!=validated.end() &&
                            validated.size()<=12) {
                            bridgeContext.effect=effect;
                            bridgeContext.group=group;
                            bridgeContext.owner=TargetBridge::blockingPlayerId[blocking].Get();
                            bridgeContext.sourceCard=source;
                            bridgeContext.chosen=selected[0].entityId;
                            bridgeContext.validated=std::move(validated);
                            useValidationBridge=true;
                        }
                    }
                    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_REVIVE_RECONSTRUCTION effect=%d group=%d source=%d chosen=%d serviceModel=%p recognitionModel=%p retainedModel=%p enabled=%d validated=%zu",
                        effect,group,source,selected[0].entityId,(void*)model,
                        (void*)ReviveRecognitionModel(),(void*)Target(boundEntityModelRoot),
                        int(useValidationBridge),bridgeContext.validated.size());
                    CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: PREPARE effect=%d group=%d source=%d guid=%d chosen=%d enabled=%d count=%zu",
                        effect,group,source,sourceGuid,selected[0].entityId,int(useValidationBridge),bridgeContext.validated.size());
                }
                // Full list has to decode against the RECEIVING service's
                // model before it can change a game-owned selection filter.
                const bool completeSelected=ids && ids->items && ids->size>=1 && ids->size<=3 &&
                    selected.size()==static_cast<std::size_t>(ids->size);
                std::uint64_t receiverSequence=0;
                const bool stored=completeSelected && Picked::Store(effect,group,selected,"RX_LIVE",&receiverSequence);
                if(stored) {
                    receiverSubmission={effect,group,receiverSequence,{},selected};
                    for(const auto& card:selected) receiverSubmission.targets.push_back(card.entityId);
                }
                if (stored) Picked::TraceLedger("RX_STAGED", effect, group, selected.empty() ? -1 : selected.front().entityId);
                if (stored) {
                    ++remotePickRebuilds;
                    CC_SELECTION_DIAG("CustomComponents/Pick3/DEFAULT_REPAIR: PICK_RECONSTRUCT effect=%d group=%d count=%zu total=%u",effect,group,selected.size(),remotePickRebuilds);
                    if (originalMode!=TargetBridge::kManualValue &&
                        EnumModes::IsLivePickFromThree(originalMode)) {
                        validatedLiveFilter=filter;
                        validatedLiveMode=originalMode;
                        validatedLiveEffect=effect;
                        validatedLiveGroup=group;
                        validatedLiveCount=ids->size;
                    }
                }
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RX_LIVE_PREFLIGHT effect=%d group=%d ownerMatches=%d mode=%d selected=%zu received=%d cached=%d normalizePlanned=%d model=%p",
                    effect,group,int(ownerMatches),originalMode,selected.size(),ids?ids->size:-1,int(stored),int(validatedLiveFilter!=nullptr),(void*)model);
#endif
            }
        }
    }
    if (originalLocalSelectProcess) {
        const bool traceSelection = play && playType.IsValid() && playCard.IsValid() &&
            playPlayer.IsValid() && playTargets.IsValid() && playType[play].Get() == 6;
        const auto* traceIds = traceSelection ? playTargets[play].Get() : nullptr;
        const int traceCount = traceIds && traceIds->items ? traceIds->size : -1;
        const int traceFirst = traceCount > 0 ? traceIds->items->m_Items[0] : -1;
        if (traceSelection) Picked::TraceLedger("STOCK_PROCESS_BEFORE", bridgeContext.effect, bridgeContext.group, traceFirst);
        if (traceSelection) CC_SELECTION_DIAG("CustomComponents/Pick3/HANDOFF_TRACE: phase=STOCK_PROCESS_ENTER service=%p play=%p cardId=%d player=%d type=6 targets=%d first=%d completion=%d reason=%d",
            (void*)self, (void*)play, playCard[play].Get(), playPlayer[play].Get(),
            traceCount, traceFirst, int(SelectionCompletion::Status()), SelectionCompletion::completion.Reason());
        if (traceSelection)
            CustomComponents::Systems::CustomTriggerSystem::ArmPick3QueueAudit("SELECT_SERVICE",
                bridgeContext.effect, bridgeContext.group);
        if (traceSelection) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=SERVICE_ENTER service=%p play=%p card=%d target=%d status=%d",
            self,play,playCard[play].Get(),traceFirst,int(SelectionCompletion::Status()));
        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_ENTER", self);
        // The reconstructed exact PickFromThree selection is already known at
        // this point. Correct the still-live marked runtime effect BEFORE
        // original Process() can retire it. Never use callback status here:
        // callback delivery is later than queue advancement on both peers.
        if (traceSelection && bridgeContext.effect >= 0) {
            CorrectPick3ReviveRolesBeforeStockSelect(self, bridgeContext.effect,
                bridgeContext.group, bridgeContext.sourceCard, bridgeContext.chosen,
                playPlayer.IsValid()?playPlayer[play].Get():-1, useValidationBridge);
        } else if (traceSelection && blocking && liveEffectEntityId.IsValid() &&
                   liveEffectGroupId.IsValid() && TargetBridge::blockingSourceId.IsValid() &&
                   playTargets.IsValid()) {
            // Replica path: no owner-only validation recipient. Require a
            // single exact transaction-bound Pick3 card already reconstructed
            // from the received stock Play, never the global latest choice.
            const int id = liveEffectEntityId[blocking].Get();
            const int group = liveEffectGroupId[blocking].Get();
            const int source = TargetBridge::blockingSourceId[blocking].Get();
            int chosen = -1, guid = -1;
            const auto* ids = playTargets[play].Get();
            if (ids && ids->items && ids->size == 1 &&
                Picked::TryGetSingleLiveEntity(id, group, chosen, guid) &&
                chosen == ids->items->m_Items[0])
                CorrectPick3ReviveRolesBeforeStockSelect(self, id, group,
                    source, chosen,playPlayer.IsValid()?playPlayer[play].Get():-1,true);
        }
        {
            TargetBridge::ScopedValidationRecipientGuard scoped(useValidationBridge ? &bridgeContext : nullptr);
            if (traceSelection) ++CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
            Pick3SecondaryCommitScope commit{};
            if (traceSelection && blocking && liveEffectEntityId.IsValid() &&
                liveEffectGroupId.IsValid() && TargetBridge::blockingSourceId.IsValid() &&
                playTargets.IsValid()) {
                const int effectId = liveEffectEntityId[blocking].Get();
                const int groupId = liveEffectGroupId[blocking].Get();
                const int source = TargetBridge::blockingSourceId[blocking].Get();
                const auto* ids = playTargets[play].Get();
                int chosen = -1, guid = -1;
                if (ids && ids->items && ids->size == 1 &&
                    Picked::TryGetSingleLiveEntity(effectId, groupId, chosen, guid) &&
                    chosen == ids->items->m_Items[0] &&
                    IsGenericPick3ReviveEffect(effectId)) {
                    Obj* finder = localServiceFinder.IsValid() ? localServiceFinder[self].Get() : nullptr;
                    Obj* entityFinder = finder && TargetBridge::finderEntityFinder.IsValid()
                        ? TargetBridge::finderEntityFinder[finder].Get() : nullptr;
                    Obj* currentModel = entityFinder && TargetBridge::entityFinderModel.IsValid()
                        ? TargetBridge::entityFinderModel[entityFinder].Get() : nullptr;
                    commit = {effectId, groupId, source, chosen, currentModel,
                              playPlayer.IsValid()?playPlayer[play].Get():-1};
                }
            }
            Pick3SecondaryCommitGuard commitGuard(commit);
            // Normalize only the exact, fully reconstructed live-PF3 RX filter
            // for the synchronous original stock Process() body. The outgoing
            // sender may already be in Manual mode; no extra writes there.
            const bool normalizeRx=validatedLiveFilter && validatedLiveMode>=0 &&
                TargetBridge::selectionType.IsValid() &&
                TargetBridge::selectionType[validatedLiveFilter].Get()==validatedLiveMode;
            if (normalizeRx) {
                TargetBridge::selectionType[validatedLiveFilter].Set(TargetBridge::kManualValue);
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RX_MODE_NORMALIZE effect=%d group=%d raw=%d stock=%d count=%d service=%p filter=%p",
                    validatedLiveEffect,validatedLiveGroup,validatedLiveMode,TargetBridge::kManualValue,validatedLiveCount,(void*)self,(void*)validatedLiveFilter);
            }
            // Restoration on every ordinary exit, including errors converted
            // to managed IL2CPP exceptions by the stock original method.
            struct RxModeRestore final {
                Obj* filter; int rawMode; bool armed;
                ~RxModeRestore() {
                    if (!armed || !filter || !TargetBridge::selectionType.IsValid()) return;
                    const int after=TargetBridge::selectionType[filter].Get();
                    if (after==TargetBridge::kManualValue)
                        TargetBridge::selectionType[filter].Set(rawMode);
                    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RX_MODE_RESTORE filter=%p raw=%d observed=%d restored=%d",
                        (void*)filter,rawMode,after,int(after==TargetBridge::kManualValue));
                }
            } rxRestore{validatedLiveFilter,validatedLiveMode,normalizeRx};
            TransactionRuntime::ScopedSubmission selectedSubmission(
                receiverSubmission.sequence?receiverSubmission:TransactionRuntime::submitted);
            if(!TransactionRuntime::StageSubmission(TransactionRuntime::ModelOf(submissionModel,true),TransactionRuntime::submitted))
                            LOG_ERR("CustomComponents/PickFromThree: selection intent rejected; stock processing continues without guessed provenance");
            originalLocalSelectProcess(self,play,method);
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
            // Read the *live* queue after stock Process returns, before its
            // surrounding scope is restored. No pointers survive this frame.
            static thread_local unsigned postSamples1724 = 0;
            if (traceSelection && postSamples1724++ < 48) {
                const int wantedEffect = commit.effect > 0 ? commit.effect : bridgeContext.effect;
                const int wantedGroup = commit.effect > 0 ? commit.group : bridgeContext.group;
                Obj* q = localServiceEffects.IsValid() ? localServiceEffects[self].Get() : nullptr;
                const bool validQueue = q && effectQueueConcrete.IsValid() &&
                    BNM::IsA(q, effectQueueConcrete);
                static auto postPrimary = BNM::Class("PvZCards.Engine", "Effect")
                    .GetField("TargetId").cast<int>();
                static auto postSecondary = BNM::Class("PvZCards.Engine", "Effect")
                    .GetField("SecondaryTargetId").cast<int>();
                static auto postPhase = BNM::Class("PvZCards.Engine.Effects", "ChooseTargetBlockingEffect")
                    .GetField("Phase").cast<int>();
                int matches = 0, examined = 0;
                if (validQueue && wantedEffect > 0 && wantedGroup >= 0 &&
                    liveEffectEntityId.IsValid() && liveEffectGroupId.IsValid()) {
                    struct Collection1724 {
                        const char* name;
                        BNM::Field<BNM::Structures::Mono::List<Obj*>*>* field;
                    };
                    // These fields represent separate stock continuation lists.
                    // Inspect only validated concrete lists; never call Peek().
                    const Collection1724 collections[] = {
                        {"Effects", &boundQueueEffects},
                        {"ResponseEffects", &boundQueueResponses},
                        {"GroupResponseEffects", &boundQueueGroupResponses}
                    };
                    for (const auto& collection : collections) {
                        if (!collection.field->IsValid()) continue;
                        auto* list = (*collection.field)[q].Get();
                        if (!list || list->size < 0 || list->size > 256 ||
                            (list->size > 0 && !list->items)) continue;
                        for (int i = 0; i < list->size && examined < 256; ++i) {
                            Obj* e = list->items->m_Items[i];
                            if (!e) continue;
                            ++examined;
                            if (liveEffectEntityId[e].Get() != wantedEffect ||
                                liveEffectGroupId[e].Get() != wantedGroup) continue;
                            ++matches;
                            const bool choose = chooseBlockingClass.IsValid() &&
                                BNM::IsA(e, chooseBlockingClass);
                            CC_SELECTION_DIAG("CustomComponents/Pick3/PREPLAY1724: stage=POST_PROCESS_EFFECT collection=%s effect=%d group=%d primary=%d secondary=%d phase=%d chosen=%d",
                                collection.name, wantedEffect, wantedGroup,
                                postPrimary.IsValid() ? postPrimary[e].Get() : -1,
                                postSecondary.IsValid() ? postSecondary[e].Get() : -1,
                                choose && postPhase.IsValid() ? postPhase[e].Get() : -1,
                                commit.chosen);
                        }
                    }
                }
                CC_SELECTION_DIAG("CustomComponents/Pick3/PREPLAY1724: stage=POST_PROCESS_SUMMARY wantedEffect=%d wantedGroup=%d scopeActive=%d queueAvailable=%d examined=%d matched=%d",
                    wantedEffect, wantedGroup, int(commit.effect > 0), int(validQueue),
                    examined, matches);
            }
#endif
            if (traceSelection) --CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
        }
        if (useValidationBridge) {
            // A returned failure is not a committed choice. Never alter the
            // queue in that case; this check is symmetric on both clients.
            const auto outcome = SelectionCompletion::Status();
            const int reason = SelectionCompletion::completion.Reason();
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_COMMIT: effect=%d group=%d chosen=%d status=%d reason=%d success=%d pending=%d failure=%d",
                bridgeContext.effect, bridgeContext.group, bridgeContext.chosen,
                int(outcome), reason, int(outcome == SelectionCompletion::Result::Success && reason == 0),
                int(outcome == SelectionCompletion::Result::Pending),
                int(outcome == SelectionCompletion::Result::Failure));
            if (outcome == SelectionCompletion::Result::Success && reason == 0)
                BindCommittedPick3RevivePrimary(self, bridgeContext);
            else
                CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=SKIP_NOT_SUCCESS effect=%d group=%d chosen=%d status=%d reason=%d",
                    bridgeContext.effect, bridgeContext.group, bridgeContext.chosen, int(outcome), reason);
        }
        if (useValidationBridge) CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: PROCESS_EXIT effect=%d group=%d chosen=%d status=%d reason=%d",bridgeContext.effect,bridgeContext.group,bridgeContext.chosen,int(SelectionCompletion::Status()),SelectionCompletion::completion.Reason());
        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_EXIT", self);
        if (traceSelection) CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=SERVICE_EXIT service=%p play=%p target=%d status=%d reason=%d",
            self,play,traceFirst,int(SelectionCompletion::Status()),SelectionCompletion::completion.Reason());
        if (traceSelection) Picked::TraceLedger("STOCK_PROCESS_AFTER", bridgeContext.effect, bridgeContext.group, traceFirst);
        if (traceSelection) CC_SELECTION_DIAG("CustomComponents/Pick3/HANDOFF_TRACE: phase=STOCK_PROCESS_EXIT service=%p play=%p completion=%d reason=%d",
            (void*)self, (void*)play, int(SelectionCompletion::Status()), SelectionCompletion::completion.Reason());
        if (traceSelection) {
            Obj* queue=localServiceEffects.IsValid()?localServiceEffects[self].Get():nullptr;
            Obj* front=nullptr;int queueCount=-1;
            if(queue && effectQueueConcrete.IsValid() && BNM::IsA(queue,effectQueueConcrete) && boundQueueEffects.IsValid()) {
                auto* entries=boundQueueEffects[queue].Get();
                if(entries && entries->size>=0 && entries->size<=4096 && (!entries->size || entries->items)) {
                    queueCount=entries->size;
                    if(queueCount) front=entries->items->m_Items[0];
                }
            }
            CC_SELECTION_DIAG("CustomComponents/Pick3/VALIDATION_AUDIT: phase=QUEUE_AFTER service=%p play=%p queue=%p queueCount=%d front=%p frontEffect=%d frontGroup=%d completion=%d reason=%d activeTicket=%llu",
                (void*)self,(void*)play,(void*)queue,queueCount,(void*)front,
                front && chooseBlockingClass.IsValid() && BNM::IsA(front,chooseBlockingClass) && liveEffectEntityId.IsValid()?liveEffectEntityId[front].Get():-1,
                front && chooseBlockingClass.IsValid() && BNM::IsA(front,chooseBlockingClass) && liveEffectGroupId.IsValid()?liveEffectGroupId[front].Get():-1,
                int(SelectionCompletion::Status()),SelectionCompletion::completion.Reason(),
                (unsigned long long)SelectionCompletion::activeTicket);
        }
    }
}
// The matching game-context binder has already been injected into the game's
// CommandBinder. Query its existing SelectTargetService binding (not a new
// service instance we construct). Limit attempts and catch managed failures.
}
