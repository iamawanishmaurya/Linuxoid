Related problem: [2026-05-16-adb-daemon-start-blocked-during-linux-launch-verification.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-adb-daemon-start-blocked-during-linux-launch-verification.md)

## What Failed

The generated Linux-side launcher was executed inside the workspace sandbox. That caused the nested `compatctl` -> `adb` path to try to auto-start the ADB daemon under sandbox restrictions, and the daemon could not bind its listener.

## Solutions Considered

1. Start the ADB daemon once outside the sandbox, then rerun the launcher inside the sandbox.
   - Pros: minimal code change.
   - Cons: the sandboxed launcher still re-entered its own restricted environment and retried daemon startup, so this did not hold.

2. Run the generated Linux-side launcher itself outside the sandbox.
   - Pros: matches the real host-launch environment, exercises the exact wrapper the user will run, and avoids changing Linuxoid just to satisfy the verifier shell.
   - Cons: needs escalated execution for verification.

3. Add code to Linuxoid to detect daemon-unavailable startup and emit a friendlier preflight failure.
   - Pros: improves diagnostics.
   - Cons: helpful but does not solve the verification environment problem by itself.

4. Rework Linuxoid to avoid ADB for the current runtime bridge.
   - Pros: would remove the daemon dependency entirely.
   - Cons: far beyond the current MVP slice and not justified by an environment-only verification failure.

## What Worked

The chosen approach was option 2: run the generated Linux-side launcher outside the sandbox during verification.

## Why It Worked

Linuxoid’s launcher script is intended to run in the host environment, not inside the workspace sandbox. Verifying it outside the sandbox aligns the test environment with the real user path and avoids a false negative caused by the verifier’s restricted process model.

## Commands Run

```bash
adb start-server
/tmp/linuxoid-launchers-auto/org.futo.inputmethod.latin.sh
```
