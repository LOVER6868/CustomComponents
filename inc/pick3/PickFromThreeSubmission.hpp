#include "core/HookRegistry.hpp"
#include "core/MatchGeneration.hpp"
#pragma once
#include "pick3/SelectionResolutionPolicy.hpp"
#include "pick3/Pick3ReleaseSafety.hpp"
#include "zones/RevivePairContract.hpp"
#include "diagnostics/Pf3RepairTrace.hpp"
#include "pick3/TargetButtonTrace.hpp"
#include "pick3/PhaseRecordTrace.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
// Experimental stock selection submission for an explicitly marked PickFromThree effect.
// Captures GameModel from PlayCardCommand.Execute and SelectTargetCommand.Execute.
// Captures SelectTargetService from the existing Step 3 factory GetService tracer
// or the already-injected field on SelectTargetCommand.Execute.
// NEVER hook generated property setters (8-byte methods adjacent to other functions).
// No direct effect execution, no local-only PvP service, no modified target lists.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Exceptions.hpp>
#include "core/SceneViewContext.hpp"
#include "presentation/TargetingPresentationRefreshSystem.hpp"
#include "pick3/PickFromThreeTargetBridge.hpp"
#include "pick3/PickedFromThreeSelection.hpp"
#include "pick3/CreationProvenance.hpp"
#include "gameplay/SelectionTransport.hpp"
#include "gameplay/SelectionCompletion.hpp"
#include "triggers/CustomTriggerSystem.hpp"
#include "diagnostics/SelectionNetworkTrace.hpp"
#include "pick3/SelectionQueryScope.hpp"
#include "pick3/SelectionQueryRouting.hpp"
#include "log.h"

// Submission feature facade. Implementation remains in one translation unit so
// BNM custom-class registration retains its established initialization behavior.
#include "pick3/submission/InputValidation.hpp"
#include "pick3/submission/TargetCreation.hpp"
#include "pick3/submission/InputTargeting.hpp"
#include "pick3/submission/ServiceState.hpp"
#include "pick3/submission/ReviveTargetSetup.hpp"
#include "pick3/submission/PairValidation.hpp"
#include "pick3/submission/ReviveRecognition.hpp"
#include "pick3/submission/ServiceBinding.hpp"
#include "pick3/submission/SelectionService.hpp"
#include "pick3/submission/InputCommands.hpp"
#include "pick3/submission/Session.hpp"
#include "pick3/submission/Dispatch.hpp"
#include "pick3/submission/ReviveCommit.hpp"
#include "pick3/submission/Completion.hpp"
#include "pick3/submission/Initialize.hpp"
