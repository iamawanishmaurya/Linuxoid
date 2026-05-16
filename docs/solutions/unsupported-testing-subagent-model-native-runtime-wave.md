# Solution: unsupported-testing-subagent-model-native-runtime-wave

Related problem: [/home/astra/codex/wine-for-android/docs/problems/2026-05-16-unsupported-testing-subagent-model-native-runtime-wave.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-unsupported-testing-subagent-model-native-runtime-wave.md)

## What Failed

The `test-automator` role failed to initialize because its pinned model was not available for this account context.

## Options Considered

1. Retry `test-automator`.
   - Rejected because the error identified an account-level model restriction, not a transient failure.
2. Switch to a default unpinned agent with the same testing prompt.
   - Viable, but less explicit about the testing/quality role.
3. Reuse an existing verifier for the testing task after it completed.
   - Viable, but would blur the role split the user requested.
4. Spawn a supported quality-focused role instead.
   - Best fit because it preserves the “testing” role intent without repeating the unsupported model path.

## What Worked

I closed the failed `test-automator` slot and re-dispatched the testing memo to a `qa-expert` agent. The QA agent completed successfully and returned the testing guidance for the backend-abstraction slice.

## Why It Worked

`qa-expert` uses a supported model in this account context, while still fitting the requested testing/quality role closely enough to preserve the staffing intent.

## Commands Run

- No shell commands were required for the fix.
- Tool actions used: `close_agent` on the failed testing slot, then `spawn_agent` with the `qa-expert` role.
