# PvP crash-boundary diagnostic (2026-10-29)

Starting baseline: StopDragIdentityCrashIsolation full ZIP. No PickFromThree gameplay, target filters, validation results, or forwarding changed.

Modified `inc/pick3/PickFromThreeSubmission.hpp`: bounded log-only breadcrumbs (`PVP_CRASH_TRACE`) around PlayCardCommand.Execute, the original stock invocation, the generic revive trick predicate, entity GUID lookup, targeting validity, and trick determination. The stop-drag identity diagnostic remains disabled.

The diagnostic counter is per-thread and limits to 1200 events. ENTER without matching EXIT indicates where the native execution died; compare `PLAY_COMMAND/STOCK_ENTER` with nested `GENERIC_BURST/ENTER` and `ENTITY_GUID/ENTER`.

Test one ordinary card in PvP first; capture new phone's full logcat and tombstone. To symbolicate exact source lines, preserve the unstripped .so from the *matching* build or linker map. The previously uploaded libsprig.so is from a different build and cannot symbolicate this new trace build.

This is an uncompiled diagnostic source patch, not a claimed crash fix.
