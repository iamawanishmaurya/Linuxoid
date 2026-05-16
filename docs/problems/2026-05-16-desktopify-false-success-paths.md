# Problem: Desktopify false-success paths

- Date: 2026-05-16
- Slug: desktopify-false-success-paths

## Exact Error

```text
Reviewer findings:
- High: desktopify-apk can report success even though the generated wrapper still depends on the original APK path instead of the staged APK copy.
- High: desktopify-apk can report success even if the generated wrapper points at the wrong compatctl binary because argv[0] was resolved incorrectly.
- Medium: IME desktop artifacts understate their side effects by looking like a generic launcher even though they may reinstall the APK, enable the IME, and change the default input method.
```

## Reproduction Steps

1. Generate desktop-launch artifacts for the keyboard APK.
2. Move or remove the original source APK, or invoke `compatctl` from a context where `argv[0]` is not the real executable path.
3. Try to rely on the generated launcher as if it were self-contained.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Context: P5 host-launch artifact review
- Date: 2026-05-16

## First Hypothesis

The desktopify path is using convenience values instead of durable launch inputs: the source APK path instead of the staged APK copy, and `absolute(argv[0])` instead of the true running executable path.
