# Activity bootstrap health evidence used wrong fields

## What failed

`BuildActivityBootstrapRecord()` referenced bootstrap-execution field names that do not exist on `NativeArtActivityBootstrapFixtureReport`, which caused the runtime-health build to fail.

## What worked

We corrected the evidence string to use the planning report's real probe fields:

- `application_probe_attempted`
- `application_probe_succeeded`
- `activity_probe_attempted`
- `activity_probe_succeeded`

## Why it worked

The activity-bootstrap fixture is now a planning seam, not the execution seam. Using the correct probe fields keeps the health evidence aligned with the right report type and preserves the planning-versus-execution separation.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

## Linked problem

- [Activity bootstrap health evidence used wrong fields](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-activity-bootstrap-health-evidence-used-wrong-fields.md)
