# PickFromThree effect-turn presentation gate

- A queued request is no longer opened immediately in `PickFromThreePreviewSystem::Tick()`.
- It waits **without creating UGUI or enabling input capture** until the exact effect entity ID and group ID occupy the front of the local service blocking-effect queue.
- The original options are retained; no resampling or deck mutation occurs during the wait.
- The submission deadline begins when the effect is confirmed ready, not when the request is first detected.
- Waiting is bounded to 1800 frontend frames. On expiry the request is finished without ever flashing a popup. This is a safety cutoff, not a guarantee that an effect remains pending for 30 seconds.
- Remote/nonlocal requests retain the previous presentation behavior, to avoid treating remote network effects as local blocking effects.
- This is a host syntax-verified source patch, not a tested Android binary.
