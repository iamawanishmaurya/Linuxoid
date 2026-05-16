# Solution: native-runtime-agent-wave-thread-limit

Related problem: [/home/astra/codex/wine-for-android/docs/problems/2026-05-16-agent-thread-limit-reached-native-runtime-wave.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-agent-thread-limit-reached-native-runtime-wave.md)

## What Failed

The second native-runtime agent dispatch wave started three agents successfully and then hit the platform thread cap when trying to spawn the remaining two.

## Options Considered

1. Retry the same full wave immediately.
   - Rejected because the same error had already occurred and would likely repeat unchanged.
2. Reduce the total number of roles and do the missing work locally.
   - Viable, but it would drop the explicit testing role the user asked for.
3. Reuse an existing open subagent for one missing role, then spawn the last role only after freeing capacity.
   - Good fit because `Noether` already existed in the thread and could cover the second verification role.
4. Close completed agents and run a wave queue.
   - Strong fit because it keeps specialization while respecting the platform cap.
5. Wait for all currently running agents to finish before any further staffing.
   - Safe, but slower than necessary.

## What Worked

I combined options `3` and `4`:

- queued the second verification task onto the existing `Noether` agent
- let the first successful wave keep running
- closed the completed planner and verifier agents as soon as their memos returned
- used the freed slot to backfill the missing testing role

## Why It Worked

The platform limit is on simultaneously active threads, not on total tasks over time. Reusing an existing agent avoided one new spawn, and closing completed agents converted the staffing problem into a wave queue the platform could accept.

## Commands Run

- No shell commands were required for the fix.
- Tool actions used: `send_input`, `close_agent`, and a follow-up `spawn_agent` after capacity was freed.
