#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline bool Initialize(bool request, bool pve, bool pvp, int thresholdMs,
                       bool useInjectionResolver = true, bool syncPveCompletion = true,
                       bool prePlayLifecycleTrace = false) {
    if (!request) return true;
    if (!Core::SceneViewContext::newGCHandle || !Core::SceneViewContext::getGCHandle || !Core::SceneViewContext::freeGCHandle) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: SETUP missing GC handle API");
        return false;
    }
    localClass = BNM::Class("PvZCards.Core.Services", "LocalSelectTargetService");
    remoteClass = BNM::Class("PvZCards.Multiplayer.Networking", "RemoteSelectTargetService");
    commandClass = BNM::Class("PvZCards.Game", "SelectTargetCommand");
    serviceFactoryClass = BNM::Class("PvZCards.Core.Utilities", "ServiceFactory");
    playCardCommandClass = BNM::Class("PvZCards.Game", "PlayCardCommand");
    gameStateUpdatedSignalClass = BNM::Class("PvZCards.Game", "GameStateUpdatedSignal");
    effectCollectionInterface=BNM::Class("PvZCards.Engine","EffectCollection");
    effectQueueConcrete=BNM::Class("PvZCards.Engine","EffectQueue");
    boundEntityModelClass=BNM::Class("PvZCards.Engine","EntityModel");
    chooseBlockingClass=BNM::Class("PvZCards.Engine.Effects","ChooseTargetBlockingEffect");
    boundQueueEffects=effectQueueConcrete.GetField("Effects").cast<BNM::Structures::Mono::List<Obj*>*>();
    boundQueueResponses=effectQueueConcrete.GetField("ResponseEffects").cast<BNM::Structures::Mono::List<Obj*>*>();
    boundQueueGroupResponses=effectQueueConcrete.GetField("GroupResponseEffects").cast<BNM::Structures::Mono::List<Obj*>*>();
    playCardStateUpdatedSignal = playCardCommandClass.GetField("<GameStateUpdatedSignal>k__BackingField");
    // Use the base-class Dispatch(), exactly like ARM64 bl 0x307C174 in
    // SelectTargetCommand.HandleServiceSuccess (0x1FBE424).
    dispatchGameStateUpdated = BNM::Class("strange.extensions.signal.impl", "Signal")
        .GetMethod("Dispatch", 0).cast<void>();
    serviceOnSuccess = BNM::Class("PvZCards.Engine", "BaseService")
        .GetField("<OnSuccess>k__BackingField").cast<Obj*>();
    stackClass = BNM::Class("PvZCards.Game", "TurnTimerStack");
    timerClass = BNM::Class("PvZCards.Game", "TurnTimerComponent");
    gameInputHolderClass = BNM::Class("PvZCards.Game", "GameInputStateHolder");
    gameCommandBinderClass = BNM::Class("strange.extensions.command.impl", "CommandBinder");
    selectServiceInterfaceClass = BNM::Class("PvZCards.Core.Services", "SelectTargetService");
    // Depending on Unity's metadata, the interface can be stored in the Core
    // namespace while its concrete instances live in Multiplayer.Networking.
    stateGameModel = gameInputHolderClass.GetField("<GameModel>k__BackingField");
    stateCommandBinder = gameInputHolderClass.GetField("<CommandBinder>k__BackingField");
    stateVisualTurnPhase = gameInputHolderClass.GetField("<VisualTurnPhaseModel>k__BackingField");
    holderInputState=gameInputHolderClass.GetField("GameInputState").cast<int>();
    highlightPlayableCardsClass=BNM::Class("PvZCards.Game","HighlightPlayableCardsCommand");
    executeHighlightPlayableCards=highlightPlayableCardsClass.GetMethod("Execute",0).cast<void>();
    // Observe stock's own command execution; never manually call it.
    const auto stockHighlightMethod = highlightPlayableCardsClass.GetMethod("Execute", 0);
    if (stockHighlightMethod.IsValid())
        CustomComponents::Core::Hooks::Install(stockHighlightMethod, StockHighlightExecuteObserved, originalStockHighlightExecute);
    CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_OBSERVER_SETUP installed=%d manualExecution=0",
             originalStockHighlightExecute ? 1 : 0);

    highlightCommandInputState=highlightPlayableCardsClass.GetField("<GameInputState>k__BackingField").cast<int>();
    highlightCommandPlayCache=highlightPlayableCardsClass.GetField("<PlayCache>k__BackingField").cast<Obj*>();
    highlightCommandModel=highlightPlayableCardsClass.GetField("<GameModel>k__BackingField").cast<Obj*>();
    highlightCommandFacade=highlightPlayableCardsClass.GetField("<GameFacadeDecider>k__BackingField").cast<Obj*>();
    highlightCommandPlayable=highlightPlayableCardsClass.GetField("<EntityBecamePlayableSignal>k__BackingField").cast<Obj*>();
    highlightCommandUnplayable=highlightPlayableCardsClass.GetField("<EntityBecameUnplayableSignal>k__BackingField").cast<Obj*>();
    CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: setup holderState=%d command=%d execute=%d cache=%d signals=%d (active by default)",
        holderInputState.IsValid()?1:0,highlightPlayableCardsClass.IsValid()?1:0,
        executeHighlightPlayableCards.IsValid()?1:0,highlightCommandPlayCache.IsValid()?1:0,
        highlightCommandPlayable.IsValid()&&highlightCommandUnplayable.IsValid()?1:0);
    visualSubTurn = BNM::Class("PvZCards.Game", "VisualTurnPhaseModel").GetField("SubTurn").cast<int>();
    getInjectionBinder = gameCommandBinderClass.GetMethod("get_injectionBinder", 0).cast<Obj*>();
    auto injectionBinder = BNM::Class("strange.extensions.injector.impl", "InjectionBinder");
    getInjectedInstance = injectionBinder.GetMethod("GetInstance", 2).cast<Obj*>();
    playClass = BNM::Class("PvZCards.Engine.Dtos", "Play");
    auto model = BNM::Class("PvZCards.Core.Utilities", "GameModel");
    localPlayerField = model.GetField("LocalPlayerId");
    opponentPlayerField = model.GetField("OpponentPlayerId");
    commandModel = commandClass.GetField("<GameModel>k__BackingField");
    commandService = commandClass.GetField("<SelectTargetService>k__BackingField");
    localServiceEffects = localClass.GetField("<Effects>k__BackingField");
    localServiceFinder=localClass.GetField("<Finder>k__BackingField").cast<Obj*>();
    peekLocalEffect = BNM::Class("PvZCards.Engine", "EffectQueue").GetMethod("Peek", 0).cast<Obj*>();
    auto gameEffect = BNM::Class("PvZCards.Engine", "Effect");
    liveEffectEntityId = gameEffect.GetField("EffectEntityId").cast<int>();
    liveEffectGroupId = gameEffect.GetField("EffectGroupId").cast<int>();
    liveBlockingEffectClass = BNM::Class("PvZCards.Engine", "BlockingEffect");
    liveEffectIsResolved = liveBlockingEffectClass.GetMethod("IsResolved", 0).cast<bool>();
    CC_SELECTION_DIAG("CustomComponents/Pick3/LIVE_EFFECT: setup effects=%d peek=%d entity=%d group=%d resolved=%d",
        localServiceEffects.IsValid()?1:0, peekLocalEffect.IsValid()?1:0,
        liveEffectEntityId.IsValid()?1:0, liveEffectGroupId.IsValid()?1:0,
        liveEffectIsResolved.IsValid()?1:0);

    targetCommandStateSignal = commandClass.GetField("<GameStateUpdatedSignal>k__BackingField");
    retainedCommandsTracker = BNM::Class("PvZCards.Application", "HeroesCommand")
        .GetField("<RetainedCommandsTracker>k__BackingField");
    playCardCommandModel = playCardCommandClass.GetField("<GameModel>k__BackingField");
    playType = playClass.GetField("Type"); playPlayer = playClass.GetField("PlayerId");
    playCard = playClass.GetField("CardId"); playOrder = playClass.GetField("OrderInTargetLocation");
    playTargets = playClass.GetField("Targets");
    timerRunning = timerClass.GetField("IsRunning");
    timerPaused = timerClass.GetField("IsPaused");
    timerParameters = timerClass.GetField("Parameters");
    auto playCardExecute = playCardCommandClass.GetMethod("Execute", 0);
    auto inputUpdate = gameInputHolderClass.GetMethod("Update", 0);
    auto execute = commandClass.GetMethod("Execute", 0);
    auto stockTargetAction = commandClass.GetMethod("SelectTarget", 0);
    auto boardTapExecute = BNM::Class("PvZCards.Game","HandleBoardTappedCommand").GetMethod("Execute",0);
    auto targetSuccess = commandClass.GetMethod("HandleServiceSuccess", 0);
    auto targetFailure = commandClass.GetMethod("HandleServiceFailure", 1);
    auto commandRelease = BNM::Class("PvZCards.Application", "HeroesCommand").GetMethod("Release", 0);
    auto push = stackClass.GetMethod("Push", 1);
    auto ctor = playClass.GetMethod(".ctor", 0);
    auto peek = stackClass.GetMethod("Peek", 0);
    const bool ok = localClass.IsValid() && remoteClass.IsValid() &&
        localPlayerField.IsValid() && commandModel.IsValid() && playType.IsValid() &&
        playPlayer.IsValid() && playCard.IsValid() && playOrder.IsValid() &&
        playTargets.IsValid() && commandService.IsValid() &&
        playCardExecute.IsValid() && playCardCommandModel.IsValid() &&
        execute.IsValid() && stockTargetAction.IsValid() && push.IsValid() && ctor.IsValid() &&
        inputUpdate.IsValid() && stateGameModel.IsValid() && stateCommandBinder.IsValid() &&
        peek.IsValid() && timerClass.IsValid() && timerParameters.IsValid() && timerRunning.IsValid() && timerPaused.IsValid();
    if (!ok || !SelectionCompletion::Initialize()) { LOG_ERR("CustomComponents/Pick3/SUBMIT: SETUP missing metadata"); return false; }
    allowPve = pve; allowPvp = pvp;
    pvpOwnerGateReady = localPlayerField.IsValid() && opponentPlayerField.IsValid() &&
                        TargetBridge::blockingPlayerId.IsValid();
    TargetBridge::resolveValidatedInitialCardTargets = &ValidatedInitialCardTargets;
    Systems::CustomTriggerSystem::RegisterInFlightSelectionProof(&ProveInFlightSelectedTarget);
    TargetBridge::resolveLocalPlayerId = &CapturedLocalPlayerId;
    TargetBridge::resolveRemoteSession = &Remote;
    if (allowPvp && !pvpOwnerGateReady) {
        LOG_ERR("CustomComponents/Pick3/PVP: OWNER_GATE_UNAVAILABLE remote selection disabled");
        allowPvp = false; // Do not compromise working PvE.
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRANSPORT: genericPlay=1 pve=%d pvpRequested=%d pvpEnabled=%d tracer=0 ownerGate=%d",
             allowPve ? 1 : 0, pvp ? 1 : 0, allowPvp ? 1 : 0, pvpOwnerGateReady ? 1 : 0);
    syncLocalCompletion = syncPveCompletion && (pve || pvp) &&
        gameStateUpdatedSignalClass.IsValid() && playCardStateUpdatedSignal.IsValid() &&
        dispatchGameStateUpdated.IsValid() && serviceOnSuccess.IsValid();
    CC_DIAG("CustomComponents/Pick3/SYNC410: SETUP requested=%d ready=%d signalType=%d field=%d dispatch=%d successField=%d",
        syncPveCompletion ? 1 : 0, syncLocalCompletion ? 1 : 0,
        gameStateUpdatedSignalClass.IsValid() ? 1 : 0,
        playCardStateUpdatedSignal.IsValid() ? 1 : 0,
        dispatchGameStateUpdated.IsValid() ? 1 : 0, serviceOnSuccess.IsValid() ? 1 : 0);
    resolveFromInjection = useInjectionResolver;
    TargetBridge::resolveMatchDatabase = useInjectionResolver
        ? &ResolveBoundCardDatabase : nullptr;
    CC_DIAG("CustomComponents/Pick3/SUBMIT: DI_META enabled=%d interface=%d getBinder=%d getService=%d",
        resolveFromInjection ? 1 : 0, selectServiceInterfaceClass.IsValid() ? 1 : 0,
        getInjectionBinder.IsValid() ? 1 : 0, getInjectedInstance.IsValid() ? 1 : 0);
    deadlineThresholdMs = std::clamp(thresholdMs, 100, 3000);
    // Enable before installing hooks: injection may occur on another thread.
    enabled = true;
    // Incoming stock targeting plays seed the same query cache as local picks.
    // Hook is independent of ADB tracing and forwards original exactly once.
    auto localProcess=localClass.GetMethod("Process",1);
    if (localProcess.IsValid()) CustomComponents::Core::Hooks::Install(localProcess,LocalSelectProcess,originalLocalSelectProcess);
    CC_SELECTION_DIAG("CustomComponents/Pick3/DEFAULT_REPAIR: incoming=%d serviceFinder=%d signalClass=%d queueMeta=%d modelClass=%d (active by default)",
        originalLocalSelectProcess?1:0,localServiceFinder.IsValid()?1:0,
        gameStateUpdatedSignalClass.IsValid()?1:0,
        boundQueueEffects.IsValid()&&boundQueueResponses.IsValid()&&boundQueueGroupResponses.IsValid()?1:0,
        TargetBridge::entityFinderModel.IsValid()?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/DB5_PVP: SETUP defaultOn=1 inbound=%d cardId=%d recipientRedirect=%d dbQuery=%d",
        originalLocalSelectProcess?1:0,playCard.IsValid()?1:0,
        TargetBridge::targetFilterQuery.IsValid()&&TargetBridge::additionalTargetQuery.IsValid()?1:0,
        TargetBridge::findDatabaseEntities.IsValid()?1:0);
    // Optional read-only diagnostics; no gameplay changes and no shared
    // ProcessEffects hook. Exact classes/method counts from dump.cs.
    const auto stopDragClass = BNM::Class("PvZCards.Game", "StopCardDragCommand");
    trickDragModelField = stopDragClass.GetField("<DragModel>k__BackingField").cast<Obj*>();
    const auto dragClass = BNM::Class("PvZCards.Game", "DragModel");
    trickDraggedCardField = dragClass.GetField("CardEntityId").cast<int>();
    trickInHandField = dragClass.GetField("InHand").cast<bool>();
    trickTargetIdsField = dragClass.GetField("Targetids").cast<IntList*>();
    trickEntitiesField = boundEntityModelClass.GetField("Entities").cast<BNM::Structures::Mono::List<Obj*>*>();
    const auto entityClassForRevive = BNM::Class("PvZCards.Engine", "Entity");
    trickEntityIdField = entityClassForRevive.GetField("<Id>k__BackingField").cast<int>();
    trickEntityComponentsField = entityClassForRevive.GetField("Components")
        .cast<BNM::Structures::Mono::List<Obj*>*>();
    trickCardComponentClass = BNM::Class("PvZCards.Engine.Components", "Card");
    const auto startMethod = BNM::Class("PvZCards.Game", "StartCardDragCommand").GetMethod("Execute", 0);
    if (startMethod.IsValid()) CustomComponents::Core::Hooks::Install(startMethod, TrickStartDragTrace, originalTrickStartDrag);
    const auto updateMethod = BNM::Class("PvZCards.Game", "UpdateCardDragPositionCommand").GetMethod("Execute", 0);
    if (updateMethod.IsValid()) CustomComponents::Core::Hooks::Install(updateMethod, TrickUpdateDragTrace, originalTrickUpdateDrag);
    const auto topDraggedMethod = BNM::Class("PvZCards.Game", "ViewEntityOverlapFinder").GetMethod("GetTopDraggedEntityId", 2);
    // Read-only cache and UI boundaries. Avoid manual command execution.
    const auto stockPlayCacheClass = BNM::Class("PvZCards.Game", "PlayCache");
    const auto cachePlaysMethod = stockPlayCacheClass.GetMethod("CachePlays", 1);
    const auto refreshPlayableMethod = stockPlayCacheClass.GetMethod("RefreshPlayableCardsList", 0);
    const auto updateHighlightMethod = highlightPlayableCardsClass.GetMethod("UpdateCardsHighlightState", 1);
    if (cachePlaysMethod.IsValid()) CustomComponents::Core::Hooks::Install(cachePlaysMethod, StockCachePlaysObserved, originalStockCachePlays);
    if (refreshPlayableMethod.IsValid()) CustomComponents::Core::Hooks::Install(refreshPlayableMethod, StockRefreshPlayableObserved, originalStockRefreshPlayable);
    if (updateHighlightMethod.IsValid()) CustomComponents::Core::Hooks::Install(updateHighlightMethod, StockHighlightUpdateObserved, originalStockHighlightUpdate);
    stockPlayableCardsField = stockPlayCacheClass.GetField("PlayableCards").cast<IntList*>();
    // Native GetPlayableCards() is two ARM64 instructions (8 bytes): do NOT
    // inline-hook it, because HasPlayableCards() begins at +8. Keep the
    // separate display-permission observer (40-byte method) intact.
    const auto shouldHighlightMethod = highlightPlayableCardsClass.GetMethod("ShouldHighlightPlayableCards", 0);
    if (shouldHighlightMethod.IsValid()) CustomComponents::Core::Hooks::Install(shouldHighlightMethod, StockShouldHighlightObserved, originalStockShouldHighlight);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=SETUP refreshField=%d displayPermission=%d tinyGetterHook=0",stockPlayableCardsField.IsValid()?1:0,originalStockShouldHighlight?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=SETUP cache=%d refresh=%d highlightUpdate=%d readOnly=1",
             originalStockCachePlays?1:0,originalStockRefreshPlayable?1:0,originalStockHighlightUpdate?1:0);
    // This call is virtual in stock and invokes GetPlaysForCard via slot 7.
    // The private static helper is only entered when the cache lookup is non-null.
    const auto cacheGetMethod = BNM::Class("PvZCards.Game", "PlayCache").GetMethod("GetPlaysForCard", 1);
    const auto firstValidMethod = BNM::Class("PvZCards.Game", "ViewEntityOverlapFinder").GetMethod("GetFirstValidTarget", 2);
    if (cacheGetMethod.IsValid()) CustomComponents::Core::Hooks::Install(cacheGetMethod, TrickCachedPlaysTrace, originalTrickCachedPlays);
    if (firstValidMethod.IsValid()) CustomComponents::Core::Hooks::Install(firstValidMethod, TrickFirstValidTrace, originalTrickFirstValid);
    if (topDraggedMethod.IsValid()) CustomComponents::Core::Hooks::Install(topDraggedMethod, TrickTopDraggedTrace, originalTrickTopDragged);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_CACHE: SETUP getPlays=%d firstValid=%d topDragged=%d readOnly=1 overlaps=opaque", originalTrickCachedPlays?1:0, originalTrickFirstValid?1:0, originalTrickTopDragged?1:0);

    trickGestureField = playCardCommandClass.GetField("<EntityGestureData>k__BackingField").cast<Obj*>();
    const auto stopDragMethod = stopDragClass.GetMethod("Execute", 0);
    if (stopDragMethod.IsValid())
        CustomComponents::Core::Hooks::Install(stopDragMethod, TrickStopDragTrace, originalTrickStopDrag);
    const auto targetMethod = BNM::Class("PvZCards.Core.Utilities", "GameStateUtils")
        .GetMethod("GetValidPlayTargetsForCard", 3);
    if (targetMethod.IsValid())
        CustomComponents::Core::Hooks::Install(targetMethod, TrickValidTargetsTrace, originalTrickValidTargets);
    // GameFacadeDecider is an interface (GetGameFacade only), not the
    // concrete gameplay generator. GameStateUtils implements both methods.
    // Targets are observed by TrickValidTargetsTrace above to avoid a double
    // hook on exactly the same GetValidPlayTargetsForCard entry point.
    const auto decider = BNM::Class("PvZCards.Core.Utilities", "GameStateUtils");
    gameStateUtilsEntityModelField = decider.GetField("<EntityModel>k__BackingField").cast<Obj*>();
    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: MODEL_FIELD_BIND decider=%d components=%d cardClass=%d",
        gameStateUtilsEntityModelField.IsValid()?1:0,trickEntityComponentsField.IsValid()?1:0,
        trickCardComponentClass.IsValid()?1:0);
    // Field identity confirmed against the game's OneTimeEffectValidator
    // declaration: <EntityModel>k__BackingField at object offset 0x28.
    // Binding it here does not change any validator result.
    pick3OneTimeValidatorModelField =
        BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator")
            .GetField("<EntityModel>k__BackingField").cast<Obj*>();
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_INITIAL_MODEL: phase=BIND validatorModelField=%d offsetExpected=0x28 mutation=0",
             pick3OneTimeValidatorModelField.IsValid() ? 1 : 0);
    const auto deciderPlays = decider.GetMethod("GetValidPlaysForCard", 3);
    if (deciderPlays.IsValid()) CustomComponents::Core::Hooks::Install(deciderPlays, TrickGeneratedPlays, originalDeciderValidPlays);
    // Both gates are observed without modifying their arguments or results.
    const auto orderMethod = decider.GetMethod("GetPossiblePlayOrdersInTargetForCard", 3);
    if (orderMethod.IsValid()) CustomComponents::Core::Hooks::Install(orderMethod, TrickOrderTrace, originalTrickOrder);
    const auto compositeMethod = BNM::Class("PvZCards.Core.Utilities", "CompositeValidator").GetMethod("Validate", 2);
    const auto oneTimeMethod = BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator").GetMethod("Validate", 2);
    const auto nullMethod = BNM::Class("PvZCards.Core.Utilities", "NullValidator").GetMethod("Validate", 2);
    const auto inHandMethod = BNM::Class("PvZCards.Core.Utilities", "InHandValidator").GetMethod("Validate", 2);
    if (compositeMethod.IsValid()) CustomComponents::Core::Hooks::Install(compositeMethod, TrickCompositeValidate, originalTrickCompositeValidate);
    if (oneTimeMethod.IsValid()) CustomComponents::Core::Hooks::Install(oneTimeMethod, TrickOneTimeValidate, originalTrickOneTimeValidate);
    if (nullMethod.IsValid()) CustomComponents::Core::Hooks::Install(nullMethod, TrickNullValidate, originalTrickNullValidate);
    if (inHandMethod.IsValid()) CustomComponents::Core::Hooks::Install(inHandMethod, TrickInHandValidate, originalTrickInHandValidate);
    const auto fighterValidateMethod = BNM::Class("PvZCards.Core.Utilities", "FighterValidator").GetMethod("Validate", 2);
    const auto openLaneValidateMethod = BNM::Class("PvZCards.Core.Utilities", "OpenLaneValidator").GetMethod("Validate", 2);
    if (fighterValidateMethod.IsValid()) CustomComponents::Core::Hooks::Install(fighterValidateMethod, TrickFighterValidate, originalTrickFighterValidate);
    if (openLaneValidateMethod.IsValid()) CustomComponents::Core::Hooks::Install(openLaneValidateMethod, TrickOpenLaneValidate, originalTrickOpenLaneValidate);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_GATE: phase=SETUP fighter=%d openLane=%d readOnly=1",
             originalTrickFighterValidate?1:0,originalTrickOpenLaneValidate?1:0);
    const auto anyTargetsMethod = BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator").GetMethod("HasAnyTargets", 2);
    if (anyTargetsMethod.IsValid()) CustomComponents::Core::Hooks::Install(anyTargetsMethod, TrickHasAnyTargetsObserved, originalTrickHasAnyTargets);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_ELIGIBILITY: phase=SETUP hasAnyTargets=%d observerOnly=1", originalTrickHasAnyTargets ? 1 : 0);
    // This stock method is 56 bytes long, unlike the unsafe 8-byte cache
    // getter. Preserve the precise PickFromThree metadata observer.
    const auto firstEffectMethod = BNM::Class("PvZCards.Core.Utilities", "EffectEntityUtils").GetMethod("GetFirstOnPlayTriggeredAndTargetedEffectEntity", 1);
    if (firstEffectMethod.IsValid()) CustomComponents::Core::Hooks::Install(firstEffectMethod, FirstTargetedEffectObserved, originalFirstTargetedEffect);
    CC_SELECTION_DIAG("CustomComponents/Pick3/RETURN_TO_LANE_METADATA: phase=SETUP hook=%d",originalFirstTargetedEffect?1:0);
    const auto targetingMethod = BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator").GetMethod("IsTargetingValid", 3);
    if (targetingMethod.IsValid()) CustomComponents::Core::Hooks::Install(targetingMethod, TrickIsTargetingValidTrace, originalTrickIsTargetingValid);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_VALID: SETUP helper=%d readOnly=1 focus=699,712,713,27 errors=counts-only collectionAccess=managed", originalTrickIsTargetingValid?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_VALIDATION: SETUP orders=%d composite=%d oneTime=%d null=%d inHand=%d readOnly=1 coverage=partial focus=699,712,713,27",
             originalTrickOrder?1:0, originalTrickCompositeValidate?1:0, originalTrickOneTimeValidate?1:0, originalTrickNullValidate?1:0, originalTrickInHandValidate?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: SETUP targets=%d plays=%d targetMethod=%d playsMethod=%d concrete=GameStateUtils readOnly=1 focus=699,712,713,27",
             originalTrickValidTargets?1:0, originalDeciderValidPlays?1:0,
             targetMethod.IsValid()?1:0,deciderPlays.IsValid()?1:0);
    // Stock trick-only placement/secondary transition observers. These hooks
    // preserve every original input and return value and are never a play override.
    const auto burstTraceMethod = BNM::Class("PvZCards.Core.Utilities", "GameStateUtils").GetMethod("GetPossiblePlayTargetsForBurst", 2);
    if (burstTraceMethod.IsValid()) CustomComponents::Core::Hooks::Install(burstTraceMethod, TrickBurstTargetsTrace, originalBurstTargetsTrace);
    RegisterPick3SecondaryFilterEvaluator(&EvaluatePick3SecondaryQueryWithStockFinder);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_COMPLEX_LANE_QUERY: stockEntityFinderRegistered=1 nestedQueryMatches=1 extraFilterModifiersFailClosed=1");
    const auto finderTraceMethod = BNM::Class("PvZCards.Core.Systems.Utilities", "TargetEntityFinder").GetMethod("FindValidSecondaryTargets", 2);
    // The target finder has two 2-argument overloads. Selecting by name and
    // arity alone is ambiguous, so we intentionally do NOT hook it here.
    (void)finderTraceMethod;
    // ProcessEffect is shared by both simulated PvP models. Observe the
    // committed phase without substituting stock continuation callbacks.
    const auto modelSelectMethod = BNM::Class("PvZCards.Core.Systems", "SelectTargetSystem")
        .GetMethod("ProcessEffect", 1);
    if (modelSelectMethod.IsValid()) {
        Acceptance::validateCards=&ValidateAcceptedSelectionCards;
        CustomComponents::Core::Hooks::Install(modelSelectMethod, Pick3ModelSelectHook, originalPick3ModelSelect);
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_MODEL_COMMIT: phase=SETUP installed=%d readOnly=1",originalPick3ModelSelect?1:0);
    const auto primaryCreatorClass=BNM::Class("PvZCards.Core.Systems.Utilities", "PrimaryTargetEffectCreator");
    const auto preselectedAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsForPreselectedTargets",3);
    const auto primaryListAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsWithPrimaryTargets",3);
    const auto primaryNormalAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsForPrimaryTargets",2);
    const auto primaryMethodAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsBasedOnTargetMethod",4);
    if(preselectedAuditMethod.IsValid()) CustomComponents::Core::Hooks::Install(preselectedAuditMethod, PickFromThreePrimaryPreselectedAudit, originalPrimaryPreselectedAudit);
    if(primaryListAuditMethod.IsValid()) CustomComponents::Core::Hooks::Install(primaryListAuditMethod, PickFromThreePrimaryListAudit, originalPrimaryTargetListAudit);
    if(primaryNormalAuditMethod.IsValid()) CustomComponents::Core::Hooks::Install(primaryNormalAuditMethod, PickFromThreePrimaryNormalAudit, originalPrimaryNormalAudit);
    if(primaryMethodAuditMethod.IsValid()) CustomComponents::Core::Hooks::Install(primaryMethodAuditMethod, PickFromThreePrimaryMethodAudit, originalPrimaryMethodAudit);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PRIMARY_BRANCH: SETUP normal=%d method=%d auditOnly=1 substitution=0", originalPrimaryNormalAudit?1:0, originalPrimaryMethodAudit?1:0);
    const auto contextAuditMethod=BNM::Class("PvZCards.Core.Systems.Utilities", "SecondaryTargetContextSetter").GetMethod("SetSecondaryTargetingContext",2);
    if(contextAuditMethod.IsValid()) CustomComponents::Core::Hooks::Install(contextAuditMethod, PickFromThreeSecondaryContextAudit, originalSecondaryContextAudit);
    const auto baseAuditEffect=BNM::Class("PvZCards.Engine", "Effect");
    pickFromThreeAuditEffectPrimary=baseAuditEffect.GetField("TargetId").cast<int>();
    pickFromThreeAuditEffectSecondary=baseAuditEffect.GetField("SecondaryTargetId").cast<int>();
    pickFromThreeAuditReturnOrder=BNM::Class("PvZCards.Engine.Effects", "MoveCardToLanesEffect").GetField("OrderInLane").cast<int>();
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_TARGET_FLOW: SETUP preselected=%d primaryList=%d contextSetter=%d primaryField=%d secondaryField=%d markerField=%d roleFix=1",originalPrimaryPreselectedAudit?1:0,originalPrimaryTargetListAudit?1:0,originalSecondaryContextAudit?1:0,pickFromThreeAuditEffectPrimary.IsValid()?1:0,pickFromThreeAuditEffectSecondary.IsValid()?1:0,pickFromThreeAuditReturnOrder.IsValid()?1:0);
    const auto secondaryCreatorAuditClass = BNM::Class("PvZCards.Core.Systems.Utilities", "SecondaryTargetEffectCreator");
    const auto addWithTargets = secondaryCreatorAuditClass.GetMethod("AddEffectsWithSecondaryTargets", 2);
    if (addWithTargets.IsValid()) CustomComponents::Core::Hooks::Install(addWithTargets,
        Pick3AddEffectsWithSecondary, originalPick3AddEffectsWithSecondary);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_SECONDARY_CREATOR: phase=SETUP installed=%d correctedOnlyWithVerifiedLaneList=1",
        originalPick3AddEffectsWithSecondary ? 1 : 0);

    const auto chooseCreatorAuditClass = BNM::Class("PvZCards.Core.Systems.Utilities", "ChooseTargetEffectCreator");
    const auto chooseDecision3 = chooseCreatorAuditClass.GetMethod("ShouldAddChooseTargetEffects", 3);
    const auto chooseDecision2 = chooseCreatorAuditClass.GetMethod("ShouldAddChooseTargetEffects", 2);
    const auto secondaryAdd = secondaryCreatorAuditClass.GetMethod("AddChooseSecondaryTargetEffects", 3);
    const auto targetChoice = chooseCreatorAuditClass.GetMethod("CreateChooseTargetEffects", 3);
    // TargetBridge owns both decisions; audit them through that owner.
    (void)chooseDecision3;(void)chooseDecision2;
    if(secondaryAdd.IsValid()) CustomComponents::Core::Hooks::Install(secondaryAdd, PickFromThreeAddSecondaryChoice, originalAddSecondaryChoice);
    if(targetChoice.IsValid()) CustomComponents::Core::Hooks::Install(targetChoice, PickFromThreeCreateTargetChoice, originalCreateTargetChoice);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_CHOOSE_AUDIT: SETUP decision3=%d decision2=%d addSecondary=%d createChoice=%d readOnly=1 unboundedReviveChoice=1", originalSecondaryChooseDecision3?1:0,originalSecondaryChooseDecision2?1:0,originalAddSecondaryChoice?1:0,originalCreateTargetChoice?1:0);
    const auto creatorTraceMethod = secondaryCreatorAuditClass.GetMethod("CreateEffectsForSecondaryTargets", 3);
    if (creatorTraceMethod.IsValid()) CustomComponents::Core::Hooks::Install(creatorTraceMethod, TrickCreateSecondaryTrace, originalCreateSecondaryTrace);
    // Exact name from dump.cs TypeDefIndex 12955 (RVA 0x220344C).
    const auto oneTimeClass=BNM::Class("PvZCards.Core.Systems","PlayOneTimeEffectSystem");
    reviveSetupModelField=oneTimeClass.GetField("<EntityModel>k__BackingField").cast<Obj*>();
    reviveSetupLocationField=BNM::Class("PvZCards.Engine.Effects","PlayCardEffect").GetField("LocationId").cast<int>();
    const auto setupMethod=oneTimeClass.GetMethod("SetupTargetsComponents",1);
    reviveSetupSourceField=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    reviveEffectOwnerSourceField=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
    reviveSetupPrimaryTargetsClass=BNM::Class("PvZCards.Engine.Components","PrimaryTargets");
    reviveGenericPrimarySelectionType=BNM::Class("PvZCards.Engine.Components","TargetEntityFilter").GetField("SelectionType").cast<int>();
    reviveSetupRemoveComponent=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STRIP_PRIMARY: SETUP enabled=%d sourceField=%d effectOwnerField=%d removeMethod=%d primaryType=%d pick3ModeField=%d signature=burst+pick3-primary+revive+first-play-trigger",kReviveStripCastingPrimaryTrialEnabled?1:0,reviveSetupSourceField.IsValid()?1:0,reviveEffectOwnerSourceField.IsValid()?1:0,reviveSetupRemoveComponent.IsValid()?1:0,reviveSetupPrimaryTargetsClass.IsValid()?1:0,reviveGenericPrimarySelectionType.IsValid()?1:0);
    if(setupMethod.IsValid())CustomComponents::Core::Hooks::Install(setupMethod,ReviveSetupTargetsAudit,originalReviveSetupTargets);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_SETUP_TARGETS: SETUP class=PlayOneTimeEffectSystem method=%d hook=%d modelField=%d locationField=%d readOnly=1",setupMethod.IsValid()?1:0,originalReviveSetupTargets?1:0,reviveSetupModelField.IsValid()?1:0,reviveSetupLocationField.IsValid()?1:0);
    const auto converterMethod=BNM::Class("PvZCards.Core.Systems.Utilities","EffectEntityConverter").GetMethod("CreateEffectsForEffectEntity",3);
    if(converterMethod.IsValid()) CustomComponents::Core::Hooks::Install(converterMethod,PickFromThreeConverterAudit,originalPickFromThreeConverter);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_OWNER: SETUP converter=%d identity=component-list readOnly=1",originalPickFromThreeConverter?1:0);
    const auto effectUtils = BNM::Class("PvZCards.Core.Utilities", "EffectEntityUtils");
    const auto isTargetedMethod = effectUtils.GetMethod("IsOnPlayTargetedEffect", 1);
    const auto validEffectTargetsMethod = effectUtils.GetMethod("GetValidTargetsForEffectEntity", 1);
    if (isTargetedMethod.IsValid()) CustomComponents::Core::Hooks::Install(isTargetedMethod, TrickEffectIsTargeted, originalEffectIsTargeted);
    if (validEffectTargetsMethod.IsValid()) CustomComponents::Core::Hooks::Install(validEffectTargetsMethod, TrickEffectValidTargets, originalEffectValidTargets);
    CC_SELECTION_DIAG("CustomComponents/Pick3/DEFERRED_TRIAL: SETUP enabled=%d scope=712,713 exactGeneratedZone=1 candidateScope=1 originalEffectFilterPreserved=1", kDeferredPick3TrialEnabled?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_EFFECT_PATH: SETUP burst=%d isTargeted=%d effectTargets=%d readOnly=1 initialZoneExperiment=%d scope=712,713 burstOnly=1", originalBurstTargetsTrace?1:0, originalEffectIsTargeted?1:0, originalEffectValidTargets?1:0, kDeferredPick3InitialZoneExperiment?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SECONDARY: SETUP burst=%d create=%d finder=skipped-ambiguous-overload readOnly=1", originalBurstTargetsTrace?1:0, originalCreateSecondaryTrace?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_IDENTITY: SETUP entitiesField=%d entityIdField=%d cardGuidField=%d readOnly=1", trickEntitiesField.IsValid()?1:0, trickEntityIdField.IsValid()?1:0, TargetBridge::databaseCardGuid.IsValid()?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_IDS: SETUP field=%d start=%d update=%d topDragged=%d readOnly=1", trickTargetIdsField.IsValid()?1:0, originalTrickStartDrag?1:0, originalTrickUpdateDrag?1:0, originalTrickTopDragged?1:0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_PLACE_DIAG: SETUP stopDrag=%d validTargets=%d gestureField=%d readOnly=1",
             originalTrickStopDrag ? 1 : 0, originalTrickValidTargets ? 1 : 0,
             trickGestureField.IsValid() ? 1 : 0);
    const auto trickSuccess = playCardCommandClass.GetMethod("HandleServiceSuccess", 0);
    const auto trickFailure = playCardCommandClass.GetMethod("HandleServiceFailure", 1);
    const auto playClass=BNM::Class("PvZCards.Engine.Dtos", "Play");
    trickPlayCardIdField=playClass.GetField("CardId").cast<int>();
    trickPlayPlayerIdField=playClass.GetField("PlayerId").cast<int>();
    trickPlayTypeField=playClass.GetField("Type").cast<int>();
    trickPlayOrderField=playClass.GetField("OrderInTargetLocation").cast<int>();
    trickPlayTargetsField=playClass.GetField("Targets").cast<IntList*>();
    const auto helperClass=BNM::Class("PvZCards.Core.Utilities", "PlayCardHelper");
    trickHelperFailModelField=helperClass.GetField("<FailReasonModel>k__BackingField").cast<Obj*>();
    trickFailureReasonsField=BNM::Class("PvZCards.Core.Utilities", "FailReasonModel").GetField("FailureReasons").cast<Obj*>();
    const auto determineMethod=helperClass.GetMethod("DetermineIfPlayIsValid",1);
    if(determineMethod.IsValid()) CustomComponents::Core::Hooks::Install(determineMethod,TrickDetermineTrace,originalTrickDetermine);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SERVICE: SETUP determine=%d card=%d player=%d type=%d order=%d targets=%d failModel=%d failureList=%d readOnly=1",originalTrickDetermine?1:0,trickPlayCardIdField.IsValid()?1:0,trickPlayPlayerIdField.IsValid()?1:0,trickPlayTypeField.IsValid()?1:0,trickPlayOrderField.IsValid()?1:0,trickPlayTargetsField.IsValid()?1:0,trickHelperFailModelField.IsValid()?1:0,trickFailureReasonsField.IsValid()?1:0);
    const auto localPlayMethod = BNM::Class("PvZCards.Core.Services", "LocalPlayCardService").GetMethod("Process", 1);
    if (trickSuccess.IsValid()) CustomComponents::Core::Hooks::Install(trickSuccess, TrickPlaySuccess, originalTrickCommandSuccess);
    if (trickFailure.IsValid()) CustomComponents::Core::Hooks::Install(trickFailure, TrickPlayFailure, originalTrickCommandFailure);
    if (localPlayMethod.IsValid()) CustomComponents::Core::Hooks::Install(localPlayMethod, TrickLocalPlayProcess, originalTrickLocalProcess);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_ACCEPT: SETUP commandSuccess=%d commandFailure=%d localProcess=%d readOnly=1 focus=699,712,713,27", originalTrickCommandSuccess?1:0, originalTrickCommandFailure?1:0, originalTrickLocalProcess?1:0);
    CustomComponents::Core::Hooks::Install(playCardExecute, PlayCardExecute, originalPlayCardExecute);
    // Install the action hook before allowing the original Execute to run.
    CustomComponents::Core::Hooks::Install(stockTargetAction, StockSelectTargetAction, originalSelectTargetAction);
    CustomComponents::Core::Hooks::Install(execute, SelectTargetExecute, originalExecute);
    // Read-only hooks. Missing optional metadata never disables submission.
    if (targetSuccess.IsValid())
        CustomComponents::Core::Hooks::Install(targetSuccess, StockTargetSuccess, originalStockTargetSuccess);
    if (targetFailure.IsValid())
        CustomComponents::Core::Hooks::Install(targetFailure, StockTargetFailure, originalStockTargetFailure);
    if (commandRelease.IsValid())
        CustomComponents::Core::Hooks::Install(commandRelease, HeroesRelease, originalHeroesCommandRelease);
    CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_COMMAND: HOOKS success=%d failure=%d release=%d "
             "signalField=%d trackerField=%d",
             originalStockTargetSuccess ? 1 : 0, originalStockTargetFailure ? 1 : 0,
             originalHeroesCommandRelease ? 1 : 0,
             targetCommandStateSignal.IsValid() ? 1 : 0, retainedCommandsTracker.IsValid() ? 1 : 0);
    if (boardTapExecute.IsValid())
        CustomComponents::Core::Hooks::Install(boardTapExecute, BoardTapExecute, originalBoardTapExecute);
    CustomComponents::Core::Hooks::Install(push, TimerPush, originalPush);
    const bool observedGameInput =
        Systems::TargetingPresentationRefreshSystem::RegisterAfterUpdateObserver(GameInputHolderUpdated);
    enabled = originalPlayCardExecute && originalExecute && originalPush &&
        observedGameInput;
    // Use TargetButtonTrace's existing state detours: no duplicate BNM hooks.
    PrePlayLifecycleTrace::Configure(prePlayLifecycleTrace);
    TargetButtonTrace::Setup();
    PhaseRecordTrace::Setup();
    // Passive diagnostics only, disabled unless ADB switch is nonzero.
    NetworkTrace::Initialize();
    QueryCaller::Initialize();
    SelectionCompletion::networkObserver = &NetworkTrace::OnServiceResult;
    CC_DIAG("CustomComponents/Pick3/SUBMIT: SETUP ready=%d pve=%d pvp=%d thresholdMs=%d hooks=playcard:%d execute:%d timer:%d gameInput:%d touchManager:0 setterHooks:0",
         enabled ? 1 : 0, pve ? 1 : 0, pvp ? 1 : 0, deadlineThresholdMs,
         originalPlayCardExecute ? 1 : 0, originalExecute ? 1 : 0,
         originalPush ? 1 : 0, observedGameInput ? 1 : 0);
    return enabled;
}
}
