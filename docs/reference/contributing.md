# Working on CustomComponents

Keep the native module and public card examples in the repository. Local log captures, binaries, experiments and one-off build output belong under ignored local folders such as `tests/`, `tools/`, `.build/` and `.local/` where applicable.

The distributed module still needs its dependency installer under `patches/BNM-Android/`. Don't remove it when cleaning up development files.

When adding a JSON-facing feature, update the source registration, model-copy behavior, example, and appropriate reference page. Test the JSON shape against the stock card format, then check native compilation and actual device behavior separately. Reuse a current section instead of adding another dated patch-status document to `docs/`.

If previously tracked local artifacts need to be untracked, review the paths before running `git rm --cached`. Untracking is not the same as deleting a working file.
