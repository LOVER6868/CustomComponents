# Repository maintenance

Public documentation describes current behavior and complete authoring examples.
Keep investigation notes, captures and temporary analysis in local working storage.

## Local development files

`tests/`, `tools/`, `.build/`, `.local/` and `Clean-Project.ps1` remain on the
 development machine and are excluded by Git ignore/archive rules. They are not
module dependencies. The dependency installer stays available at
`patches/BNM-Android/Apply.ps1` in a clean checkout.

Ignore rules do not remove already tracked files. When migrating a checkout,
untrack private paths while retaining their local contents:

```powershell
git rm -r --cached --ignore-unmatch -- tests tools Clean-Project.ps1
```

Review the staged deletions before committing. Keep local directories intact.
The source export contains the module, required dependency patch, manual and
examples. Verify it builds without tests or tools present.

## Implementation and validation

Reuse shared hook owners. Keep identity/model checks, pooling and stock query
semantics explicit. Add small public examples instead of runtime-state dumps.
Optional local tools run host regressions, ARM64 syntax checks, native links and
schema validation. Device gameplay and frame-time testing are separate evidence.

Keep the changelog relative to its linked upstream baseline, grouped by visible
behavior. Private iteration numbers and raw debugging notes belong in local notes.
