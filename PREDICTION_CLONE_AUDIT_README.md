# PickFromThree prediction-clone audit (diagnostic only)

Based on the uploaded CustomComponents(20261024-013724).zip snapshot. No CardDatabase/InDeckGuid source edits.

Enable temporarily in Bloom's config.ini:

```ini
[Pick3Diagnostics]
PredictionCloneAudit=true
```

The default is **false**. Observe `CustomComponents/Pick3/CLONE_AUDIT` in `adb logcat -s Sprig:I`. The hook wraps **stock EntityModel.DeepCopyFrom(object, InstanceManager)**, invokes the original exactly once, then reads source/destination entity counts and `NextEntityId`; max 12 observations per process. It performs NO card creation, pool mutation, RNG operation, or effect submission. `distinct=1 countMatch=1 nextMatch=1` demonstrates separate model objects with equal baseline counts/counters, NOT proof of factory/RNG isolation or fully equal card pools. It does NOT fix shrinking or PvP desync. If no copy happens, there may be no observations, especially on client-authoritative paths.

Caution: the provided source snapshot retains older deck-materialization behavior; this diagnostic patch does not revert or change it. Merge these two source edits into the actual latest local source if it differs from the uploaded snapshot.
