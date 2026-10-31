# PlayCache / overlap resolution trace (read-only)

The hooks observe `PlayCache.GetPlaysForCard`, private static `ViewEntityOverlapFinder.GetFirstValidTarget`, and the existing `GetTopDraggedEntityId` wrapper. No stock arguments or results are modified. The `IEnumerable<int>` overlap input is deliberately left opaque.

Look for `TRICK_CACHE: SETUP getPlays=1 firstValid=1 topDragged=1`. Then compare `LOOKUP`, `FIRST_VALID`, and `RESOLVE` for one drag `seq`. `cacheCount=-1` means null; `-2` means unobserved/invalid list layout. `firstValid=-2` means that the helper was not observed; it is not a real target ID.

The logs alone cannot prove cache membership unless subsequent `TRICK_GENERATION` legal targets are correlated; do not interpret fallback as an acceptable play target. Test 699 and 712 after a discard candidate becomes eligible. Prior initial-zone experiment remains disabled.
