Related problem: [2026-05-16-waydroid-matrix-generated-launcher-failure.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-waydroid-matrix-generated-launcher-failure.md)

## What Failed

Linuxoid's new `verify-waydroid-matrix` command reported `0/3` passed with each package stuck at `33/100`, even though `verify-waydroid-package` had already passed for the same apps and artifact roots.

The repeated failure turned out to be an execution-environment issue, not a broken Android-app launch path. When the matrix command ran inside the sandbox, Waydroid could not reach the user D-Bus and returned:

```text
ERROR: org.freedesktop.DBus.Error.AccessDenied: Failed to connect to socket /run/user/1000/bus: Operation not permitted
```

## Solution Options Considered

1. **Structured in-process reuse**
   - Run the matrix through `VerifyWaydroidPackageWithRunners()` and keep the per-package result as structured data.
   - Pros: least brittle, no text scraping, closest to the core library design.
   - Cons: still needs a top-level environment that can talk to Waydroid.

2. **CLI subprocess orchestration**
   - Shell out to `compatctl verify-waydroid-package` for each package and parse its output.
   - Pros: uses the already-proven single-package flow.
   - Cons: adds recursion, text parsing, and still fails if the outer command is sandboxed.

3. **Low-level matrix reimplementation**
   - Rebuild the matrix from direct launch, artifact generation, and launcher execution primitives.
   - Pros: full control over each check.
   - Cons: duplicates verifier semantics and increases drift risk.

4. **Top-level unsandboxed verification with better reporting**
   - Keep the matrix command on the structured path, improve per-package reporting, and run the live verification outside the sandbox so Waydroid can access the user D-Bus.
   - Pros: fixes the real blocker, preserves the cleaner architecture, and produces an honest pass/fail report.
   - Cons: requires an elevated run in this Codex environment.

## What Worked

Linuxoid kept the cleaner structured matrix verifier, improved the package-level report to show `direct`, `launcher`, and `generated` subchecks, and reran the live matrix command outside the sandbox. The unsandboxed run passed for all three installed apps:

- `com.android.calculator2`
- `com.android.settings`
- `org.fdroid.fdroid`

## Why It Worked

The code path for Linuxoid's direct Linux launch verification was already valid. The failure came from the Codex sandbox blocking Waydroid's access to the user D-Bus. Running the top-level matrix verifier outside the sandbox restored the same capability that the earlier approved single-package verifier already had, and Linuxoid immediately returned a `3/3` pass matrix.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl verify-waydroid-matrix /tmp/linuxoid-matrix com.android.calculator2 com.android.settings org.fdroid.fdroid
'/home/astra/codex/wine-for-android/build/compatctl' verify-waydroid-package 'com.android.calculator2' '/tmp/linuxoid-matrix/applications' '/tmp/linuxoid-matrix/launchers'
./build/compatctl verify-waydroid-matrix /tmp/linuxoid-matrix com.android.calculator2 com.android.settings org.fdroid.fdroid
```
