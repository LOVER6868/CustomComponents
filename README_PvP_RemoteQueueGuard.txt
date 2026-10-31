PvP remote queue guard / service collection diagnostics isolation

1. CaptureSelectionTriggerContext now refuses remote service instances before reading localServiceEffects.
2. TrickIsTargetingValidTrace no longer enumerates service valid/selected target collections for diagnostics.

No change to gameplay selection request, primary source capture, stock validation, or local PvE selection.
Experimental source patch; compile and runtime unverified.
Test both phones for manual 2+ choices and single option, and capture fresh logs if crashing.
