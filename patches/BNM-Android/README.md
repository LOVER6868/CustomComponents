# BNM exception metadata initialization

This module uses BNM TryInvoke around managed calls. The included Exceptions.cpp
initializes exception metadata before use, avoiding an uninitialized native
metadata pointer in the audited dependency version.

From the module root, run:

```powershell
.\patches\BNM-Android\Apply.ps1 -SprigRoot 'C:\path\to\Sprig-main'
```

The script supports -WhatIf, accepts an already applied fix, checks the audited
original SHA-256, rejects redirected descendants and verifies the copied result.
If your dependency differs, review its changes before applying the payload.
Rebuild the native library and APK after applying it. No developer tools folder
is needed for this installation step.
