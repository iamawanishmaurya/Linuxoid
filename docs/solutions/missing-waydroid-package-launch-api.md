Related problem: [2026-05-16-missing-waydroid-package-launch-api.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-waydroid-package-launch-api.md)

## What Failed

Linuxoid had no runtime API for launching already installed Waydroid apps and no host-launch artifact generator for installed-package launches, so the new non-IME tests could not even compile.

## What Worked

- Added `launch-waydroid-package` with a Waydroid-native runtime launch report.
- Added `desktopify-waydroid-package` with Linux launcher and `.desktop` artifact generation for installed packages.
- Verified the path live with `com.android.calculator2` through both the direct command and the generated Linux launcher.

## Why It Worked

The current host already has a functioning Waydroid app-launch surface. Reusing that backend directly avoids unnecessary APK reinstall logic for non-IME apps and gives Linuxoid a more realistic “launch installed app from Linux” path.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl launch-waydroid-package com.android.calculator2
./build/compatctl desktopify-waydroid-package com.android.calculator2 /tmp/linuxoid-apps-calc /tmp/linuxoid-launchers-calc
/tmp/linuxoid-launchers-calc/com.android.calculator2.sh
```
