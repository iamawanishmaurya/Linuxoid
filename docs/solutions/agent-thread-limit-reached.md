# Solution: agent-thread-limit-reached

- Problem file: [2026-05-16-agent-thread-limit-reached.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-agent-thread-limit-reached.md)
- What failed:
  Attempting to spawn all remaining research agents at once exceeded the session's maximum number of active subagent threads.

- What worked:
  Running the research plan in waves fixed the issue. Active agents were allowed to complete, their slots were explicitly closed, and the next queued research task was spawned only after capacity became available.

- Why it worked:
  The failure came from a hard platform concurrency cap, not from the prompts themselves. A bounded queue respects that cap and guarantees eventual completion without repeating the same spawn error.

- All commands run:

```text
spawn_agent Research task 1 of 10
spawn_agent Research task 2 of 10
spawn_agent Research task 3 of 10
spawn_agent Research task 4 of 10
close_agent 019e2d16-b0c3-7be3-909c-648d722f88c8
spawn_agent Research task 2 of 10 (supported role)
wait_agent active wave
close_agent completed slot
spawn_agent Research task 5 of 10
wait_agent active wave
close_agent completed slot
spawn_agent Research task 6 of 10
wait_agent active wave
close_agent completed slot
spawn_agent Research task 7 of 10
wait_agent active wave
close_agent completed slot
spawn_agent Research task 8 of 10
wait_agent active wave
close_agent completed slot
spawn_agent Research task 9 of 10
close_agent completed slots
spawn_agent Research task 10 of 10
wait_agent remaining agents
close_agent remaining completed agents
```
