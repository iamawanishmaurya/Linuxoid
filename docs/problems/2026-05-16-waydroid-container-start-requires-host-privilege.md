## Exact Error

```text
[09:53:00] ERROR: [Errno 1] Operation not permitted: '/var/lib/waydroid/waydroid.log'
[09:53:00] See also: <https://github.com/waydroid>
Run 'waydroid log' for details.
```

## Reproduction Steps

1. Run `waydroid container start` outside the sandbox.
2. Observe the container startup fail while trying to touch `/var/lib/waydroid/waydroid.log`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Runtime candidate: Waydroid container
- Execution mode: escalated host command
- Date: 2026-05-16

## First Hypothesis

This host expects the Waydroid container to be started through a privileged service path rather than as an unprivileged user command. Linuxoid itself is not blocking here; the missing step is host-side runtime privilege or service activation.
