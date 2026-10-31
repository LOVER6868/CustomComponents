# Pick3 hand highlight crash isolation hotfix

- Restores previous native avatar-art behavior and factory diagnostic selector range 0–7.
- Disables only the additional injected HighlightPlayableCardsCommand.Execute() call after custom Pick3 completion. The original GameStateUpdatedSignal dispatch, PvP CardDatabase protocol, button fix, sampler, and live deck audit are kept.
- If the crash stops, it implicates the added highlight execution; this is a controlled isolation patch, not proof of the exact null argument.
- The provided pick3_live_deck.txt is the old 19:30 log; it has no LIVE_DECK_AUDIT or HAND_HIGHLIGHTS events. Capture a *new* log for a genuine live deck eligibility diagnosis.
- Re-enable stock highlights via the game's ordinary input/turn transitions; do not manually call the injected command twice.
