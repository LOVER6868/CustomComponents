# Hook ownership and consolidation map

**This page documents native hook ownership and remaining consolidation candidates; it does not assert that every static hook call site installs during every match.** Enum deserialization, the frontend tick and CardView mapping now have shared hook owners. The current scene mapper also has a single GC-rooted context. Animation hook ownership remains unchanged.

| Stock interception or shared facility | Current module / callback owner | Maintenance issue or proposed home |
| --- | --- | --- |
| `DefaultDeserializer.GetValue(Type, object)` enum fallback | `core/EnumSerializationSupport.hpp`, initialized once from `main.cpp` | **Consolidated.** `TargetScopeSupport` registers `ListPosition`, `GrantAbilitySerialization::RegisterAbilityEnums` registers abilities. Both execute before the shared hook is installed. The old `systems/EnumSerializationSupport.hpp` forwarding include was retired. |
| `ReferenceTypeDeserializer` and custom-field cloning hooks | `CustomFieldBridgeSystem`, with feature callbacks | Retain central ownership; move feature-specific handlers behind registrations later. |
| `TriggeredEffectTargetScoper.CalculateSortedValueFromEntityId` | `TargetScopeSupport` | Keep ListPosition calculation here; avoid global changes for stock sort modes. |
| Current-scene `EntityViewMapper` reference (not a hook) | `core/SceneViewContext.hpp` | **Step 4B consolidated.** Owns the scene GC root and numeric model identity. Captured from existing registration/lane hooks; released by the existing scene-reset callback. Consumers must use `RootedMapper` and model identity verification, not store raw pointers across frames. |
| `CardViewMapper.CreateCardAsync` and retirement observations | `core/CardViewLifecycleCoordinator.hpp` | **Step 4D reorganized.** Production owner of deferred creation, return markers, death/delete observations, and retirement. The old `systems/CardViewLifecycleTracer.hpp` forwarding include was retired; the canonical coordinator remains the single hook owner. |
| `CardViewMapper.MapCardView` | `core/CardViewMappingCoordinator.hpp`, installed at the existing `DiscardFromHandPresentationSystem::Initialize` point | **Step 4C consolidated.** A feature-owned discard-ticket policy decides the replacement flag; the coordinator then invokes `Core::CardViewLifecycleCoordinator::MapBefore`, the stock method once, and `MapAfter`, in that order. No second native hook is installed. |
| `HandViewComponent.FindCardInHand`, `RepositionCards` | `presentation/DiscardFromHandPresentationSystem.hpp` | Existing hooks observe opponent-hand view; do not add opponent `Awake` interception. |
| `EventSystem.Update` frontend reconciliation tick | `core/FrontendTickDispatcher.hpp`, installed once from `DiscardFromHandPresentationSystem::Initialize` | **Step 4A consolidated.** Fixed order: hand-discard reconciliation, deferred CardView creation, stock `EventSystem.Update`. Both callbacks remain feature-owned. |
| UI-only `PickFromThreePreview` callback | `presentation/PickFromThreePreviewSystem.hpp`, optionally registered in `core/FrontendTickDispatcher.hpp` | **Diagnostic, disabled by default.** Runs after stock `EventSystem.Update`; introduces no new native hook; UI-only mock cards and pointer polling. See [prototype guide](PICK_FROM_THREE_UI_PROTOTYPE.md). |
| `BuffSystem.ProcessEffect` | `zones/DiscardProvenanceSystem.hpp` (`DiscardedSystem` namespace), with observers | Keep a single installed hook. `ZoneTriggeredAbilityPresentationSystem` should observe through that owner, not independently rehook. |
| Grant/remove ability processors | `GrantAbilitySystem`, with observer callbacks | Shared read-only presentation observations should remain callbacks, not duplicate hooks. |
| `EffectAnimationQueue` processing, presentation guards and lane stat refresh | `presentation/ZoneTriggeredAbilityPresentationSystem.hpp` | **Steps 4E.1/4E.2 extracted private implementation fragments** for frame sync, lane entry, queue observation, effect audit, off-board source context, view routing, timeline hooks, visual policies, and asynchronous command completion. The single hook owner, original callbacks, initialization, order, and state are unchanged. The off-board safety code remains under the same logical owner, now organized into private fragments. See [PRESENTATION_LAYOUT](PRESENTATION_LAYOUT.md). |
| Hand removal diagnostic suite | `diagnostics/HandRemovalDiagnostics.hpp` (`HandRemovalTraceSystem` namespace), initialized in `main.cpp` | Included via `DiscardPileZoneEffects.hpp`, disabled by default in `main.cpp`; enable with `[Diagnostics] HandRemovalTrace=true` in `config.ini`. This is diagnostic-only and does not affect the production lifecycle coordinator. |
| Deck materialization and discard inspection UI hooks | `zones/DeckMaterializer.hpp`, `presentation/DiscardPileViewer.hpp` | Refiled in Step 5; their existing namespaces and hooks remain unchanged. |

## Hook audit interpretation

The source currently contains **99 `BNM::BasicHook` call sites** across headers and private fragments. This is a regression checksum, **not** 99 confirmed runtime-installed hooks: some call sites are conditional, and others appear inside helper loops that can install several hooks. The optional hand-removal suite uses one generic call site to install eight additional hooks only when configured. See [maintainability audit](MAINTAINABILITY_AUDIT.md).

## Principles for future hook refactors

1. **One installed native hook per stock entry point.** Feature observers register with that owner.
2. Do not move code solely because a feature uses a shared callback; distinguish hook ownership from callback registration.
3. Preserve original function pointers and calling conventions, particularly for short ARM64 methods.
4. Keep gameplay hooks on their actual execution thread; Unity/CardView mutation belongs on frontend paths.
5. Test any relocation on emulator and physical device, with stock Trick reveal, repeated lane returns, and both viewpoints.

Enum registration, shared frontend tick, scene mapper, CardView mapping, and lifecycle coordinator naming moves are complete. Presentation source extraction now includes the off-board guards and timeline callbacks, with no hook-ownership changes; independently initialized presentation submodules remain deferred. Protect lifecycle and queue behavior with dedicated two-device regression tests.

## Step 5 file locations

Step 5 moved 42 implementation headers without adding or moving native hooks. Use the canonical file paths listed in [SOURCE_LAYOUT](SOURCE_LAYOUT.md). Former paths were compatibility-only forwarders. The public C++ namespaces were deliberately not renamed.
