## Exact Error

Command:

```bash
./build/compatctl native-service-manager-fixture /tmp/linuxoid-native-spike/packages/com.example.app/vc1/bootstrap/activity-bootstrap.json
```

Observed output:

```text
compatctl error: unable to read file: /tmp/linuxoid-native-spike/packages/com.example.app/vc1/bootstrap/activity-bootstrap.json
```

## Reproduction Steps

1. Invoke `native-service-manager-fixture` with a bootstrap manifest path that is not present in the current workspace state.
2. Run the command above.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`

## First Hypothesis

The command path was aimed at an example bootstrap location from older documentation, but that manifest had not been generated in the current local workspace. The service-manager fixture itself may still be healthy; the verification input path was wrong.
