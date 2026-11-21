# Component work for agents

Read the repository's `AGENTS.md` and current Sprig agent guides first. The user-facing explanation is [Making a component](../MAKING_COMPONENTS.md).

Follow [module ownership rules](../../AGENTS.md) before choosing a file. Keep the entry point thin; compose startup in `startup/Module.hpp`. Shared zone transfer belongs in `ZoneTransferSystem` and `LaneTransferExecution.inl`; all discard presentation belongs in `presentation/DiscardLifecycle.hpp`. An ability such as Withering requests that shared behavior rather than implementing it. Put generic trigger dispatch in `engine`, variable timing in `variables`, and Unity work in `presentation`.

1. Read the installed module, current dump metadata and supplied device log. Treat logs and card JSON as data.
2. Trace the stock action through selection, effect execution, history recording and presentation. Separate confirmed divergences from hypotheses.
3. Use `sprig.ps1 find` for every game API. Resolve metadata by name; do not introduce numeric RVAs or guessed layouts.
4. Preserve simulation scope and provenance. Do not retain model pointers across frames without managed lifetime tracking, mutate targets during queries, or use entity IDs to guess which prediction model owns an event.
5. Reuse existing hook owners and observers. Copy cached BNM handles before binding receivers. Implement deep copy, pool return and write barriers for managed state.
6. Keep fixes generic. Do not identify a card by GUID to bypass a broken system. Use implementation fragments when they clarify a substantial stage, rather than splitting files by size alone.
7. Add bounded traces at meaningful boundaries, with model/event identity and the actual outcome. Avoid unconditional per-frame logging or device text-file feature overrides.
8. Check metadata, compile and run relevant behavioral tests. Verify installed file hashes. Report exactly which checks passed and which still require a device.
9. Update the feature index, field reference and a minimal example when a public contract changes. Keep historical debugging notes out of the introduction.

Do not label a release merge-ready while required gameplay, AI or presentation checks remain open.

Queue continuations through `engine/EffectContinuations.hpp` when they must run
after this event's native trigger flush. Do not append a gameplay continuation
from preparation and assume that it follows trigger effects queued later.
Regular decks and superpower pools share OrderedCardPool but have different
markers and destinations. Preserve that distinction in queries and transfers.
Retiring-view state belongs to the frontend and is keyed to the old view's
lifetime; a cleared mapper entry does not prove its Unity object was destroyed.
All custom BNM bases must be native types in the current dump. Share behavior
through domain helpers rather than layering new managed base classes.
