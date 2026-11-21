# Release validation

The module contains shared fixes and bounded traces for damage blocking, swap presentation, variable snapshots, deck fallback, and AI choices. Combat is always available. Device text-file switches are unsupported.

Run `sprig.ps1 lint CustomComponents`, build, then capture a complete match. A passing build verifies C++ and metadata, not animation timing or game decisions.

Exercise these cases in one capture:

- Deal ability damage to a hero with NonCombatTruestrike. Check the actual block value and its animation, then repeat against a fighter and with ordinary combat.
- Swap two single fighters, swap with a team-up lane, and swap two partners in one lane. Both moving views and remaining partners must agree with board order and targeting.
- Discard from each hand while a trigger animation is running. The view must survive until its presentation finishes, then disappear once.
- Queue several plays and non-play triggers. Each variable comparison must use its own event value, including after a target choice pauses resolution. Repeat next turn and next match.
- Exhaust legal physical deck choices with failsafe enabled. Confirm routing reaches execution and a new card is recorded, while the original deck and placeholder stay unchanged. Repeat with failsafe disabled and an occupied lane.
- Let the AI play leveled cards and resolve PickFromThree. Check costs, legal targets, chosen GUIDs and resumed execution. Compare ordinary AI turns for regressions.
- Capture card-play, level-change and AI stalls with performance tracing enabled. Compare the same scenario before and after; do not infer a speedup from compilation.

Run `tools/Read-ReleaseCapture.ps1 -LogPath <capture>` to summarize observed boundaries. Missing observations require another exercised case or investigation. Review error lines separately.

Merge acceptance requires these device cases to pass. Native builds and dump checks alone do not establish stability or release readiness.

For the 220629 capture repair, also exercise:

- PF3 cold and cached loads: assets remain hidden during loading, then the complete choice opens. Closing during a load must release late callbacks.
- Level changes in the same hand: affordability glow and placement highlights refresh without an unrelated card action. There must be no SetPlayable/Dimmer null references.
- Hero health buffs above base maximum: verify actual rules health and displayed health, then damage and ordinary healing.
- Withering in each supported zone: observe the ordinary native transfer, trigger and presentation boundaries.
- Opponent hand discard: dim while waiting, popup with the native hand settings, shrink, then retire once. Hidden cards must stay hidden.
- Same-lane custom combat: no facing rotation. Before-combat buffs must settle continuous modifiers before attack values are generated.
- Halfbread: correlate TriggerSnapshots/DRAW_CREATED with Draw/DELIVERY. The last capture does not prove why its draw failed; this pass adds diagnostics rather than a card-specific workaround.

AI scorer timing is sampled once in 128 polls; its labelled sample total is not total AI time. PF3 AI playability remains a separate unimplemented integration described in the investigation notes.

For the lanes/death/superpowers pass, also exercise:

- Repeated lane indexes, Also plus Multishot, and Only overriding an all-lane marker. Test selected-pair combat separately from Only/all lane encounters and multiple secondary selections.
- Before-combat effects and their continuous modifiers finish before attack values are built. Both counterattacks must survive simultaneous lethal damage. Toggle RotateFighters off and verify identical rules/history.
- Cancel a damage, buff and explicit-destroy death. Bounce in the interception window, then revive the same card while its old view is dying. Verify one view, one zone and no duplicate discard.
- Regrant one triggered GUID after bounce and revive. Compare different sources, Set/Add, EndOfTurn and NextFighter expiry, and restoration of an innate ability.
- Add a superpower with zero, one, three and more than three remaining. Verify actual blocking resumes, shields refill, the count is visible on both sides, and initial-four queries still match drawn powers.
- Deep Pea Beast: change the strongest in-play attack without moving either card. Verify its continuous hand-cost reduction updates and unwinds.
- Patch of Soul: capture DamageRedirect/SELECT and the redirected effect's damage outcome; preventing the first damage is not evidence that replacement damage succeeded.
- Halfbread: first play another plant, then exercise the stack threshold. Both listeners must use the same event snapshot and produce their separate buff/draw outcomes.
