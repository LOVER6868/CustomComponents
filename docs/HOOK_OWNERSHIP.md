# Hook ownership

Use `HookRegistry` for native detours. Each stock method has one owner; other
consumers use that owner's callbacks or capabilities.

| Boundary | Owner / extension route |
| --- | --- |
| Unity EventSystem update | `FrontendTickDispatcher` |
| Game input-holder update | `TargetingPresentationRefreshSystem`; submission before picker observation |
| Native hand play-cache generation | `HandPlayability`; observes CachePlaysCommand and its actual GameFacade |
| Local play service and control routing | `PlayServices`; submission diagnostics observe the shared owner |
| Level stat dimming colours | `LevelStatColours`; scoped Dimmer.ApplyDim baseline cache |
| Level stat frame updates | `LevelFrameState`; native cost/attack/health update methods |
| Live UI service access | `FrontendServices`; observes the existing input-holder update |
| Entity serialization/copy | `CustomFieldBridgeSystem`; schemas and before/after observers |
| Queue insertion | `engine/TriggerQueue.hpp`, `engine/EffectQueue.hpp` |
| Effect routing | Existing Thrash owner or transaction fallback; shared transaction scope |
| Trick validation | Existing submission owner; `PlayValidation.rules` |
| On-play targeted ability selection | `PlayValidation.abilityRules`; stock targets remain authoritative |
| Trigger eligibility | Existing BoardAbility validator owner; `TriggerEligibility.rules` |
| Before-payment preparation | `EffectGuards`; ordered preparation callbacks |
| Both public CardFromSubsetFinder overloads | `CreationProvenance`; shared `SubsetSelection` context/capabilities |
| Draw/created-card completion | `CreationProvenance`; verify GUID/entity/model before binding |
| Created-card placement | `SubsetCreationExtensions`; grants before legality checking |
| Resolved effect completion | Existing discard/return owner via `Engine::Events`; `SimulationChanges` subscribes for revisions |
| Native multiplier-specific value | `variables/Multiplier.hpp`; custom multiplier only, stock delegation |
| Card-local phase exceptions | `PhasePermissions`; relevant native phase validators |
| Grant/remove/expiry | `GrantAbilitySystem`; registry dispatch |
| Revive eligibility | `ReturnToLaneTargeting.inl`; shared physical pair checker |
| Board taps | Existing discard-viewer owner; picker/additional callbacks |
| Card data and icons | Shared metadata observers and `FrameIconRegistry` |

Timing attaches to existing owners. A diagnostic switch must not disable a
functional binding or gameplay adapter. Keep frontend refresh separate from
simulation and submission.
