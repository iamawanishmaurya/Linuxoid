# Solution: forked-agent-type-conflict

Related problem: [/home/astra/codex/wine-for-android/docs/problems/2026-05-16-forked-agent-type-conflict.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-forked-agent-type-conflict.md)

## What Failed

The first native-runtime research wave tried to create full-history forked agents while also overriding `agent_type` and `reasoning_effort`. The spawn API rejected every request before any agent started.

## What Worked

Re-dispatching the same work as self-contained prompts without `fork_context=true` succeeded. The agents could still specialize by role because the role pinning was no longer in conflict with full-history inheritance.

## Why It Worked

The platform treats forked agents as inheriting the parent role/model configuration, so role overrides are invalid in that mode. Removing the full-history fork restored the legal combination: explicit role plus self-contained context.

## Commands Run

- No shell commands were required for the fix.
- Tool actions used: `spawn_agent` with self-contained prompts instead of forked prompts.
