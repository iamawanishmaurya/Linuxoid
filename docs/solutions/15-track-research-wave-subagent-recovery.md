# Solution: 15-track-research-wave-subagent-recovery

Problem reference: [2026-05-16-15-track-research-wave-subagent-constraints.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-15-track-research-wave-subagent-constraints.md)

## What Failed

The first attempt to launch the full 15-track research wave hit two known platform limits:

- unsupported pinned-role models under a ChatGPT-account Codex session
- maximum active subagent thread count

## Options Evaluated

1. Retry the exact same 15-role burst immediately.
   - Rejected because it would repeat both failure classes unchanged.

2. Drop the agent count and do the remaining research locally.
   - Viable, but it defeats the user’s explicit request for one subagent per listed task.

3. Keep the 15-task structure, but replace incompatible roles with supported agent types.
   - Good fit because the task is research-heavy and does not require the pinned unsupported roles.

4. Keep the 15-task structure, but run it through a bounded wave queue.
   - Strong fit because it respects the thread cap while still completing all 15 assignments over time.

5. Reuse already-running compatible agents for adjacent follow-up tasks when possible, then close completed slots before spawning more.
   - Strong fit because it reduces needless churn and helps the queue drain faster.

## Chosen Recovery

Use a combined recovery:

- switch incompatible roles to supported research-capable agent types
- keep the one-task-per-gap structure
- run the wave through a bounded queue
- close completed or errored agents promptly before spawning the next queued tasks

## Why It Worked

The failure was in orchestration, not in the research prompts themselves. Supported agent types preserve the scope, and a wave queue respects the platform’s concurrency cap without abandoning the 15-track breakdown.

## Commands / Tool Actions Used

```text
spawn_agent initial compatible tasks
close_agent errored incompatible roles
wait_agent active compatible roles
close_agent completed slots
spawn_agent next queued tasks with supported roles
repeat until all 15 tasks have assigned research outputs
```
