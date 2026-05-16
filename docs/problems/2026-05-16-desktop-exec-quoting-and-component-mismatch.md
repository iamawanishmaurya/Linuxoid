# Problem: Desktop Exec quoting and component mismatch

- Date: 2026-05-16
- Slug: desktop-exec-quoting-and-component-mismatch

## Exact Error

```text
Reviewer findings:
- Medium: `.desktop` launchers can still report ready even when a space-containing launcher path makes the raw `Exec=` line ambiguous.
- Medium: `desktopify-apk` can still report ready when the caller-selected component belongs to a different package than the APK being desktopified.
```

## Reproduction Steps

1. Generate desktop artifacts under a desktop-root path that contains spaces.
2. Or call `desktopify-apk` with an APK from one package and an explicit component from another package.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Context: P5 host-integration re-review
- Date: 2026-05-16

## First Hypothesis

The desktop integration layer is still trusting raw file paths and caller-selected components a little too much. It needs one more fail-closed pass for desktop quoting and package/component consistency.
