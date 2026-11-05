# PickFromThree — Discover-style presentation (port)

This build uses the **presentation/UI portion** of the supplied `DiscoverSystem.hpp`.
The Discover gameplay descriptors/offer generator/hand-conjure code were **not** ported.

## Preserved game logic

- `PickFromThreeCore`, `PickFromThreeTargetBridge`, `PickFromThreeSubmission`
- `PickedFromThreeQuery`, candidate pools, mode 712/713, stock blocking choice
- PvE and PvP submission through existing stock service, including single-card auto-select
- `TargetingPresentationRefreshSystem`, custom card-specific targeting text
- `ZoneTriggeredAbilityPresentationSystem`, `detail/AnimationQueueObservation.inl`,
  `detail/Pick3VisualQueueGate.hpp`, `detail/Pick3VisualBarrierTracker.hpp`

## New UI behavior

- Three stock CardViewMapper CardViews in a vertical arrangement, a dark scrim,
  and Choose buttons based on the stock mulligan UI assets.
- Tapping a card previews its card inspection. Tapping its Choose button
  submits the exact candidate INDEX through `Submission::Submit`.
- Existing board tap hook in `DiscardPileViewer` is shared, not hooked twice.
- Exact effect/group and queue-front checks occur before publishing/consuming.
- One eligible live card auto-submits after the stock service is ready.
- Multi-select indices are sent together when requiredTargets choices are made.
- `SelectionCompletion` remains the authority for accepted/failed submissions.
- No synthetic `GameStateUpdated` signal or animation queue blocking is introduced.
- The animation barrier logs are read-only and keep ownership `UNKNOWN`.

## Config

`[PickFromThreePresentation] Enabled=true` enables the Discover-style UI.
`UseStockTargeting=true` bypasses the custom UI/tap interception entirely,
leaving the stock targeting path in control. This is a fallback/diagnostic option;
stock may not visually support the custom 712/713 selection type in every case.
`MaskStockTargetUI=true` (default) moves the stock prompt off-screen while displaying
our own localized PF3 targeting text. It returns when the UI closes.

`[Pick3TargetingText]` retains `BehindCards`, `PositionX`, `PositionY` and
`FontSizePercent`. Localization uses the activating card's exact prefab
`_PickFromThreeTargeting` key. A generic prompt appears when unavailable.

## Known limits and on-device validation

- This is a new UI port, not tested on Android here. Desktop clang++ C++20
  `-fsyntax-only` passed with the provided BNM/Sprig headers and Linux stubs.
- CardView callbacks, view-map ownership, stock input tap coordinates, and
  the custom-targeting localization must be verified on phone and emulator.
- The shared animation observer remains diagnostic; safe animation completion
  gating has **not** been proved and is intentionally disabled.
- `UseStockTargeting=true` is a diagnostic bypass, not a guarantee stock will
  interpret custom PF3 modes exactly like `Manual`.

## Current release

The user confirmed patch 3's PvP trigger repair works. Patch 4 retains that
model-scoped deferred-submission ledger and adds ReturnToLane fighter/environment
support. See the current [release notes](../CHANGELOG.md) and
[ReturnToLane guide](reference/return-to-lane.md); earlier investigation text in
this guide does not establish current two-device validation for patch 4.
