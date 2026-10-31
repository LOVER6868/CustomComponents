# PickFromThree stock command boundary, 2026-10-23

## Scope
This is a **limited, unverified correction**, not a complete selection-lifecycle fix.

- An intercepted SelectTargetCommand whose injected service differs from the service owned by our modal is now forwarded through its original Execute rather than swallowed. The normal engine command is allowed to register its own callbacks.
- The custom completion handler no longer dispatches a cached PlayCardCommand GameStateUpdatedSignal as a substitute for a matching SelectTargetCommand signal.
- Stock commands with the **same** service as our modal still remain suppressed to avoid double-selecting a target; as a result, the underlying command completion gap is **not resolved**.

## What the game binary confirms
`SelectTargetCommand.Execute` RVA 0x1FBDC70 performs its command setup and registers service handlers before flowing into `SelectTarget` RVA 0x1FBDF7C. Passing the entire original Execute during an active custom modal may submit a second target. The present patch does **not** do that.

## Validation
No Android build or gameplay run was available in this environment. The source must be rebuilt and tested in both PvE and PvP. This patch does not address short candidate pools, Fisherman activation timing, or missing matching stock-command success callbacks. In particular, the absence of the native command callback still needs a real integration with game command lifecycle; this cannot be guaranteed by a UI signal.
