# Source layout and native hooks

`main.cpp` starts the module and registers its features. Most gameplay code is header-based under `inc/`; the main folders are:

| Folder | Responsibility |
| --- | --- |
| `inc/abilities/` | Grantable abilities, active state and expiry |
| `inc/effects/` | Custom descriptors and runtime effect creation |
| `inc/engine/` | Shared model, queue, entity and native helpers |
| `inc/level/` | Level choices, modifiers and client presentation |
| `inc/queries/` | Entity predicates and targeting support |
| `inc/triggers/` | Event dispatch, listeners and trigger rules |
| `inc/variables/` | Stored values, expressions and multipliers |
| `inc/zones/` | Deck, discard and lane transfers |
| `inc/presentation/` | Card display, menus, frame and icons |

`bloom.ini` and `config.ini` describe module loading and runtime options. `patches/` contains the required dependency fix. `examples/` is card JSON, not C++ source.

## One owner per native hook

Native methods should be detoured in one place. Other features subscribe to that owner's event or call a shared helper instead of placing a second hook on the same method. `HookRegistry` keeps track of ownership. This is particularly important for the multiplier calculator, effect queue and frontend input/update paths.

The existing services separate game-state work from UI updates. Prediction models should not read live private-hand presentation state. When adding a feature, check its model-copy, pooling, serialization, and PvP-command behavior as well as its normal single-player path.

## Shared hook map

These are the existing owners or extension routes. If a feature needs one of these boundaries, subscribe to it instead of detouring the stock method again.

| Boundary | Current owner or route |
| --- | --- |
| UI EventSystem update | `FrontendTickDispatcher` |
| Input holder and targeting refresh | `TargetingPresentationRefreshSystem`, `FrontendServices` |
| Hand play cache | `HandPlayability` |
| Play commands and transport | `PlayServices`, `PlayTransport` |
| Trick eligibility and requirements | `PlayValidation.rules`, `PlayValidation.abilityRules` |
| Trigger conditions | `TriggerEligibility.rules` |
| Pre-payment effect preparation | `EffectGuards` |
| Effect and trigger insertion | `Engine::EffectQueue`, `Engine::TriggerQueue` |
| Serialized custom fields and model copies | `CustomFieldBridgeSystem` |
| Database subset finding and completion | `CreationProvenance`, `SubsetSelection` |
| Creating cards and placement traits | `SubsetCreationExtensions` |
| Ability grants, removals, expiry | `GrantAbilitySystem`, `AbilityRegistry` |
| Variable-based multiplier values | `inc/variables/Multiplier.hpp` |
| Card-local phase permissions | `PhasePermissions` |
| Level cost/stat presentation | `LevelFrameState`, `LevelStatColours` |
| Card-frame icons | `FrameIconRegistry` |
| Resolved effect notifications | `Engine::Events`, `SimulationChanges` |

For instance, Level mode changes prepare modifiers before payment, but the native play validator still owns the final legality check. `SimulationChanges` observes completed model updates without retaining pooled effect pointers.

### Ownership and copies

Keep managed objects rooted only while they're retained. Use model-generation keys for simulation state; don't hold a borrowed list beyond its owner's lifetime. Sidecar data on stock objects needs copy and pool-return handling rather than edits to the stock managed class layout. A picked GUID is not enough to recover a specific physical deck card: the selection transaction must carry verified model/entity identity.

## Adding a component

Follow the [new-component checklist](new-component-checklist.md). For a new effect, confirm that its descriptor converts to a runtime effect and that the processing hook actually receives that effect type. A syntactically valid `$type` is not evidence that the game executes the effect.

Development captures belong in local build/test folders, not the public `docs/` directory. See [contributing](contributing.md) for the repository layout.
