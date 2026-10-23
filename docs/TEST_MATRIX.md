# Pre-release validation matrix

**Default branch:** test with `SubmitInPVP=false`,
`AllowUnverifiedPvp=false`, `EnableNativeArtwork=false`,
`AllowNativeCardViewFactory=false`.

| Scenario | Expected result | Status |
|---|---|---|
| Static picker opens in local PvE | 1–3 eligible options, readable art and localized prompt | Device required |
| Ordinary PickFromThree one pick | Exactly one stock SelectTarget submission, effect resolves | Device required |
| UniqueGuidTargets with repeated cards | No duplicate GUID in the three options; eligible copies weighted | Pure tests PASS; device required |
| CardDatabase single-choice local PvE | GUID resolved through Picked query; stock recipient automatically selected | Device required |
| Empty/invalid query | No invalid Play, no stale/duplicate callback | Device required |
| Two pick / three pick live modes | All required target IDs in a single Play.Targets, no double send | Device required |
| Timeout/scene reset | No second Play after uncertain dispatch; no stale UI across next match | Device required |
| Same match repeated 5–10 times | Popups don't leak/overwrite stale objects or crash | Device required |
| Match A→menu→match B and app restart | No zombie GameObjects or native callbacks | Device required |
| Problem physical phone with static art | No SIGSEGV during repeated card plays/choice exits | Device required |
| Native three-card art re-enabled | Test separately after passing static branch | Experimental; **do not release** |
| Regular PvP on two matched clients | Both clients same authoritative result; no stuck selection | Not established |
| CardDatabase PvP | Selected GUID replicated and validated by server | Not implemented |

Suggested capture after a crash in Windows PowerShell:

```powershell
adb logcat -c
# reproduce once in game
adb logcat -b crash -d -v threadtime > .\pf3-crash.txt
adb logcat -d -v threadtime > .\pf3-full.txt
```

Keep the original device's *ABI*, game build number, Sprig Build ID,
card GUID, selection mode, whether stock/native artwork was enabled, and
whether the match was PvE/PvP. These fields are necessary to distinguish
native ARM64 faults from translated/emulator traces.
