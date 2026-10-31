# Unearth effect ownership audit (2026-10-28)

Base: `CustomComponents_UnearthPrimarySequenceBoundary_20261028_FULL.zip`.

Scope: Read-only instrumentation in `inc/pick3/PickFromThreeSubmission.hpp`. Does not alter play acceptance, target lists, effect queues, filter values, or the stock creator's arguments and outputs. This is not a targeting fix.

## What to observe

`UNEARCH_OWNER: SETUP converter=1` confirms the converter hook installed.

`UNEARCH_OWNER: COMPONENT ...` lists each actual effect entity's component class name, bounded to the first 65 sampled audit events. This provides direct component evidence rather than attributing ownership from the mutable `trickSecondaryFocusGuid`.

`UNEARCH_OWNER: PRESELECT_ENTER / NORMAL_ENTER / METHOD_ENTER / PRIMARY_LIST_ENTER / SECONDARY_ENTER / CONVERTER_ENTER` and corresponding exits report entity ID, pointer, query context pointer, filter/descriptor presence, output pointer and output count (when a managed `get_Count` method is available).

`focusHint` is *only* a sampling hint; it is not the owning card GUID. For components absent from an effect entity, the output is zero. If `components=-1`, do not interpret zeros as proof of absence.

## Test

In PvE, play GUID 712 with a discarded plant; capture the log from initial cast through target selection and execution. Compare IDs and component flags for PRESELECT versus NORMAL and CONVERTER effects. Compare with GUID 698 if available. A hook that does not install or an inaccessible managed collection is reported as unknown; no fallback memory offsets are guessed.

## Limitations

No Android compilation or device execution was available. The existing experimental role-fix guard from the base remains unchanged; this change does not activate it or add a new gameplay mutation. The component list can expose ownership clues but the source card association may require a separate read-only ownership field probe after this test.
