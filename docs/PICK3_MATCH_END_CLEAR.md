# PickedFromThree match-scoped lifetime

`Picked::Store()` overwrites the latest selection for the same originating effect entity. Closing the popup or consuming `PickedFromThreeQuery` does not clear it.

`Picked::ResetMatch()` now clears all stored selections under the cache mutex. The preview's match-exit watchdog invokes `Submission::ResetMatch()` (which calls `Picked::ResetMatch()`) both when an open popup loses the match and when an idle popup observes the match has ended. The latter is no longer conditional on `externalOnly`.

This prevents IDs/results from a prior match being reused at the start of a later match. The submitted Play native-object retention policy remains unchanged because it has a different asynchronous lifetime.

Source-level change only: Android compilation/device behavior not verified.
