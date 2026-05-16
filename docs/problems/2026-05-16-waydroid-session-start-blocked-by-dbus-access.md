## Exact Error

```text
[09:50:10] ERROR: org.freedesktop.DBus.Error.AccessDenied: Failed to connect to socket /run/user/1000/bus: Operation not permitted
[09:50:10] See also: <https://github.com/waydroid>
Run 'waydroid log' for details.
```

## Reproduction Steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `waydroid session start`.
3. Observe the D-Bus access failure while trying to start the Waydroid session.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Runtime candidate: Waydroid
- Sandbox mode: workspace-write
- Date: 2026-05-16

## First Hypothesis

Waydroid is installed on the host, but starting a session requires host D-Bus access that is blocked in the sandboxed shell. Starting the session outside the sandbox should determine whether Waydroid can serve as Linuxoid’s live Android runtime on this machine.
