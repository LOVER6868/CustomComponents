# Return-to-lane targeted play trigger and capacity guard (experimental)

Baseline: Recovery_Correction_20261024 CustomComponents. Other systems are unchanged.

- The original discard removal, LaneModifier.PlayCardInLane, CardView provenance and CreateCardInLaneRecord are preserved.
- After successful placement/history creation, calls existing CustomTriggerSystem::FireForEntity("PlayTrigger", cardId, effect, false), which uses the existing listener-source matching and validator, then fires ReturnedFromDiscardPileTrigger.
- Before removal, checks destination occupancy for the returning player's side: zero allows one; one permits a second plant when either resident or incoming has Teamup; two rejects further placement. Zombie lanes with one friendly resident reject a second. No new entity is created, no ordinary PlayCardEffect or resource cost is replayed.
- Logs LANE_FLOW: CAPACITY / TEAMUP_CHECK / PLAY_TRIGGER_ENTER / PLAY_TRIGGER_EXIT / RETURN_TRIGGER_EXIT.

LIMITATIONS: This prevents attempting placements onto known-full owner slots but does not replace the effect's upstream primary-target selection with a random legal-size subset. For seven eligible discarded fighters and three slots, the *first eligible* effects reaching available slots are attempted, not a uniformly random three. This must be fixed in target selection using the game's synchronized RNG, not client-local rand(). Also the presence of a Teamup component is used as a proxy; dynamically suppressed Teamup counters or OpenLaneQuery-specific restrictions may differ. This patch should be tested in a private/local match before PvP.

C++20 syntax-only compilation passed; no Android APK or on-device execution was performed.
