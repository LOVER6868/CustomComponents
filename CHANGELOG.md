# Changelog

## Unreleased

Compared with published [`main` at e6b7823](https://github.com/LOVER6868/CustomComponents/tree/e6b7823e140e34918accd22ad8c4d2f1daa89db9),
verified on 2026-11-06. These are the changes in this working version, not released
GitHub version numbers.

### Added

- Per-ability PlayRequirement queries with pre-payment eligibility snapshots.

- Level options with ordered extra costs, signed/X buffs, synchronized hand modifiers, a shake gesture and pulse.
- Leveled state and comparison queries, with owned modifier cleanup on departure.
- A versioned Level command extension preserved through prediction copies and pooling.
- Explicit removal of stock/custom abilities, including innate or source-scoped modes.

- Withering grants with hand/deck discard and board-destruction modes.
- Grantable native Multishot and AttacksOnlyInAdjacentLanes markers.
- Physical deck-to-lane deployment with PF3 targeting and play-trigger semantics.

- RandomKeyword grants with configurable pools and source/duration-aware removal.
- Destroyable hidden BoardAbility support.
- Configurable StatIcons and a reusable frame-icon provider registry.
- CreateCardInDeckFromSubset for integrated subset-based deck creation/shuffling.
- ListPositionQuery for deck/discard positions from either end.
- OpenLaneForCardQuery and CanBePlacedQuery for placement-aware targeting.
- Independent additional-target selection and additional-first ordering for PF3.
- Team-Up, Aquatic and front/back options for stock subset creation.
- A current feature catalog, authoring guides and categorized example indexes.

### Changed

- Level stat colours survive the native card dimmer without changing its fade behavior.
- Final trick validation combines each ability's requirement and primary-target eligibility.

- Hand-mode submissions provide valid native completion delegates, allowing immediate preview, tint and legal-play cache refresh.

- Level hand modes use real queryable modifiers and stock stat colours without emitting buff triggers.
- Full-cost/X modes follow current resources and reset when no extra payment remains.
- Physical PF3 eligibility verifies its own primary query instead of counting only a casting zone.

- Play requirements combine with each ability's native primary-target eligibility and preserve final stock validation.
- Level changes rebuild native playability and refresh the current card, lane or casting-zone targets.
- Selected Level cost text overrides its font face, cached colour, gradient and colour tags, restoring owned settings on return to base.

- Unaffordable Level options also hide drag-time placement and target markers.
- Level cost-number tint follows native colour updates and refreshes its text mesh.

- Selected Level costs control hand affordability highlights and use a configurable cost tint.
- Level changes use a larger, longer pulse that can finish after returning a card to hand.
- PF3 board-light suppression preserves stock target discovery for consecutive menus.

- Custom ability expiry skips absent components and counters with no matching duration.
- Level tables read boxed JSON numbers directly and report the failed field when invalid.
- Leveled queries see private hand choices and command-scoped trick validation, resetting at base.

- PF3 selection, submission and listener routing retain model/effect/group/generation
  identity and exact physical representatives, including automatic unique-GUID choices.
- Verified created-card binding participates in normal operation independently of
  tracing. Provisional target queries remain separate from committed picked queries.
- Consecutive menus observe the frontend frame tick and refresh the real local
  holder without submitting synthetic board actions.
- Return-to-lane preserves card identity, supports fighters/environments, checks
  fighter capacity and Aquatic water placement, and excludes return-origin moves.
- Custom discards provide their own herald origin and cooperate with optional
  HeraldModifiers. Inspection stat reconciliation is confined to discard viewing.
- Deck subset drawing supports physical-entity queries and delivered-card heralds.
- Subset creation options survive deserialization/copying; granted Aquatic affects
  the stock subset finder before its innate-trait lane restriction.
- Shared services centralize native hook ownership, model lifetime, queues, managed
  settings and subset selection.

### Performance and distribution

- Reuse generation metadata instead of copying the full selection history.
- Reuse verified router model lookups and cache tag/keyword metadata.
- Avoid copying every tag for a single-prefix lookup.
- Add bounded nested timing for turn/phase, copy, selection and ability expiry work.
- Separate local tests, maintenance tools and investigation notes from the GitHub
  distribution. Include the required dependency installer with the production patch.
- Replace scattered private iteration notes with the current public manual.

### Validation

Local host, schema and native build checks accompany development. Device gameplay
and performance measurements remain separate evidence; the manual documents current
constraints rather than claiming universal test coverage.
