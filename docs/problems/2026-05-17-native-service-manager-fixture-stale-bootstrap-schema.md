## Exact Error

Command:

```bash
./build/compatctl native-service-manager-fixture /tmp/linuxoid-p1-manual-P7qZ76/activity-bootstrap.json
```

Observed output:

```text
compatctl error: unable to extract string field from json: package_name
```

## Reproduction Steps

1. Reuse an older bootstrap artifact that does not match the current Linuxoid bootstrap schema.
2. Run the command above.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`

## First Hypothesis

The command is reading a bootstrap artifact from an earlier manual P1 experiment whose JSON shape does not contain the current required fields, so the correct fix is to generate a fresh bootstrap manifest with the current `bootstrap-native-spike` flow instead of retrying arbitrary old paths.
