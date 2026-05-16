# Problem: Manifest component validation too literal

- Date: 2026-05-16
- Slug: manifest-component-validation-too-literal

## Exact Error

```text
compatctl error: requested launcher component is not declared by the desktopified APK
```

## Reproduction Steps

1. Run `./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop-verified`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Live target: `emulator-5590`
- Date: 2026-05-16

## First Hypothesis

The new manifest-backed component validator is still comparing the caller-selected component and the manifest-declared component too literally, even when they are equivalent short and fully qualified forms of the same class.
