# Hook ownership

Use `HookRegistry` for native detours. Each stock method has one owner; other
consumers use that owner's callbacks or capabilities.

| Boundary | Owner / extension route |
| --- | --- |
| Unity EventSystem update | `FrontendTickDispatcher` |
| Game input-holder update | `TargetingPresentationRefreshSystem`; submission before picker observation |
| Entity serialization/copy | `CustomFieldBridgeSystem`; schemas and before/after observers |
| Queue insertion | `engine/TriggerQueue.hpp`, `engine/EffectQueue.hpp` |
| Effect routing | Existing Thrash owner or transaction fallback; shared transaction scope |
| Both public CardFromSubsetFinder overloads | `CreationProvenance`; shared `SubsetSelection` context/capabilities |
| Draw/created-card completion | `CreationProvenance`; verify GUID/entity/model before binding |
| Created-card placement | `SubsetCreationExtensions`; grants before legality checking |
| Grant/remove/expiry | `GrantAbilitySystem`; registry dispatch |
| Revive eligibility | `ReturnToLaneTargeting.inl`; shared physical pair checker |
| Board taps | Existing discard-viewer owner; picker/additional callbacks |
| Card data and icons | Shared metadata observers and `FrameIconRegistry` |

Timing attaches to existing owners. A diagnostic switch must not disable a
functional binding or gameplay adapter. Keep frontend refresh separate from
simulation and submission.
