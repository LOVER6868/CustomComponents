# PickFromThree: stock command lifecycle integration

Baseline: user-uploaded CustomComponents(20261023-172017).zip.

Change: `inc/pick3/PickFromThreeSubmission.hpp`.

- Preserve the original `SelectTargetCommand.Execute()` so the game registers stock success/failure callbacks and retains the command.
- Hook `SelectTargetCommand.SelectTarget()` to skip only the stock target action when the custom modal captures input AND the command's injected service equals the active service. Other commands execute normally.
- Do not call stock success / release manually or send an additional selected-target Play.
- Do not dispatch a synthetic game-state signal if the owning stock command is available for normal completion.
- Maintain original PvE/PvP transport, candidate eligibility, weighted unique-GUID sampler, UGUI, portraits and timer logic.

Validation performed: checked source metadata names, modification and archive integrity. NOT compiled or tested on Android. Native hook behavior and async service completion require device testing; stock command may retain until service callback and its ownership must be confirmed by logs. No guarantee all coded cards work: individual abilities/triggers need functional tests.

In device logs search COMMAND_LIFECYCLE: STOCK_EXECUTE_WITH_ACTION_DEFERRED and STOCK_ACTION_DEFERRED, then STOCK_COMMAND: SUCCESS_ENTER / RELEASE_ENTER. If the two deferred events never appear, or success still does not fire, do not assume the completion path is fixed.
