# Solution: Activity Bootstrap Fixture Missing Application Probe

Problem: [2026-05-17-activity-bootstrap-fixture-missing-application-probe.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-activity-bootstrap-fixture-missing-application-probe.md)

## What failed

`native-art-activity-bootstrap-fixture` was still modeling only the launcher activity target. Its JSON did not include a normalized application class, and its trace did not show an application probe followed by a launcher-activity probe.

## What worked

- Extended `NativeArtActivityBootstrapFixtureReport` with:
  - `selected_application_class_name`
  - `selected_application_class_descriptor`
  - `application_bootstrap_command`
  - `application_probe_attempted`
  - `application_probe_succeeded`
  - `activity_probe_attempted`
  - `activity_probe_succeeded`
- Upgraded the fixture to emit explicit JSON and JSONL trace data for both application and launcher-activity probe phases.
- Classified probe success honestly by treating missing-class failures as hard failures and “class resolved but no main method” output as a bounded class-load success for the probe seam.

## Why it worked

Linuxoid now models the post-class-resolution bootstrap path as a sequence instead of a single activity-only step. That gives the self-healing runtime a truthful application/activity bootstrap attempt surface without overstating it as real Android app execution.

## Commands run

```bash
cd /home/astra/codex/wine-for-android
cmake --build build && ctest --test-dir build --output-on-failure
```
