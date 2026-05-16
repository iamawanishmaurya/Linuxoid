Links back to:

- [2026-05-17-native-service-manager-fixture-verification-path-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-native-service-manager-fixture-verification-path-missing.md)
- [2026-05-17-native-service-manager-fixture-stale-bootstrap-schema.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-native-service-manager-fixture-stale-bootstrap-schema.md)

## What Failed

Manual verification initially pointed at:

1. a bootstrap manifest path that did not exist in the current workspace
2. then an older bootstrap artifact with a stale schema

That produced two non-code verification failures before the service-manager fixture itself was exercised on a current bootstrap.

## Distinct Solutions Considered

1. **Keep guessing existing bootstrap paths in `/tmp`**
   - Fast, but brittle and likely to repeat the same verification-input class of failure.

2. **Teach the new command to support old bootstrap schemas**
   - Useful only for backward compatibility with stale manual artifacts, not for the current gate.

3. **Generate a fresh bootstrap manifest with the current `bootstrap-native-spike` flow**
   - Best fit for this gate because it verifies the service-manager fixture against the current Linuxoid-native artifact contract.

4. **Add a dedicated test-only bootstrap fixture to the repo**
   - Useful later, but not necessary when the project already has a current bootstrap generator.

## What Worked

The chosen fix was **solution 3**: generate a fresh bootstrap manifest with the current Linuxoid command surface, then run `native-service-manager-fixture` against that generated artifact.

## Why It Worked

The real issue was not the Binder-shaped manager implementation. The issue was stale or missing verification input. By generating the bootstrap from the current command path, Linuxoid validates the feature against the same schema and artifact layout it now owns.

## Commands Run

```bash
./build/compatctl bootstrap-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
./build/compatctl native-service-manager-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```
