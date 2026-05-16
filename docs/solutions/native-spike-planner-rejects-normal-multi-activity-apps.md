# Native Spike Planner Rejects Normal Multi-Activity Apps

Related problem: [2026-05-16-native-spike-planner-rejects-normal-multi-activity-apps.md](../problems/2026-05-16-native-spike-planner-rejects-normal-multi-activity-apps.md)

## What Failed

The first live native spike verification against Calculator produced a false blocker because Linuxoid required exactly one declared activity for a native candidate.

## What Worked

Relaxed the native spike eligibility rule to focus on the meaningful constraints:

- keep requiring a resolvable launcher activity
- keep rejecting services, IME apps, boot receivers, and secondary processes
- stop rejecting normal launcher apps just because they declare more than one activity

Added a regression test for a launcher-resolved multi-activity app and reran the live Calculator proof.

## Why It Worked

The first native slice only needs one launcher entry point that Linuxoid can bootstrap. Multiple declared activities do not inherently make an app too complex for the native spike, so removing that artificial gate lets Linuxoid target realistic simple apps while still excluding advanced runtime shapes.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl plan-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
```
