# CustomComponents ownership and organization

Preserve edits in the installed Bloom module before starting work. Compare each
file against the baseline again before installation; merge concurrent changes.

- `main.cpp` contains the Bloom entry point and exported cross-module adapters.
  It includes the module startup header, not individual gameplay features.
- `startup/Module.hpp` composes initialization. `CustomComponents.hpp` is the
  module umbrella. Keep implementation and runtime state in their owning domain.
- Abilities request ordinary engine actions. They do not own zone transfer,
  discard presentation, card-view disposal, or another ability's bookkeeping.
- Shared hand/deck/discard/board movement belongs to `zones`. Use neutral names
  for shared transfer code. Deck fallback must not live in a discard-only file.
- Presentation owns Unity objects and animation completion. Gameplay sends
  recorded engine actions; it must not call Unity or reach into presentation state.
- Variables use the normal effect DSL: stock trigger validation, conditions,
  source/target filters, target selection, and recipient queries. Do not implement
  a second partial condition language for one card or a hard-coded GUID counter.
- Shared hooks have one owner. Add observers/adapters rather than a second
  detour. Resolve game methods and field names with `sprig.ps1 find`.
- Required custom presentation stops stock consumption at its history boundary
  and resumes after completion/cancellation. Never return null as a stock record,
  rewind the stock cursor, or reorder native animation lists to force timing.
- A new BNM constructor must call its native base constructor explicitly via
  `Core::NativeConstruction::Base(this)`. `CallCustomMethodOrigin` only finds
  virtual overrides on new classes; using it for `.ctor` silently does nothing.
- Combat selection and validation use the same live-fighter pair policy. Faction
  restrictions belong in authored queries, never in generic combat targeting.
- A runtime effect's selected target differs from its original event target.
  Use `EffectEventContext` for subset queries that refer to the trigger event.
- Never retain an EntityViewMapper entry after stock removes its Unity view.
  Animation lifetime and mapper registration are separate responsibilities.
- Use `.inl` for a cohesive implementation section owned by its including header.
  Do not put unrelated systems in a convenient `.inl` or rely on an accidental
  transitive include. Cross-domain dependencies should point to small interfaces.
- Refactors must preserve authored data and existing public types. Document
  migrations, keep backups, update architecture tests, and run source layout,
  native regressions, module lint and the native build. Report device limitations.

New work must follow these rules instead of adding another direct feature include
to the entry point or another special case to a neighboring system.

- Custom mutations must be represented by stock records or typed CustomHistory
  facts. Read `docs/history/COVERAGE.md` before adding a writer. Emit at the owning
  mutation site, preserve source/duration and before/after state, and skip AI
  probes. Rule/presentation provenance must never apply stock stat changes twice.
  Add contract, serializer and recorder-viewer tests for new payloads. Keep kind
  IDs stable and name snapshot encodings explicitly.

- A native generic registration may bypass its nongeneric overload. Do not rely
  on a type alias observer alone to prove a derived action has a processor.
  Prefer the registered native action for ordinary zone work.
- Lane layout orders are relative to the native owner's group. A flat lane-pool
  index is not a team-up slot. Keep loading hierarchies active until callbacks
  complete; hide their visuals without suppressing loader lifecycle methods.
