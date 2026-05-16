# Problem: Same-package component typo readiness

- Date: 2026-05-16
- Slug: same-package-component-typo-readiness

## Exact Error

```text
Reviewer finding:
- Medium: desktopify-apk still reports a ready launcher when the caller-selected component stays inside the APK package namespace but does not actually exist in the APK manifest.
```

## Reproduction Steps

1. Run `desktopify-apk` with a component like `org.futo.inputmethod.latin/.DoesNotExist`.
2. Observe that the generated files can still be marked ready even though the component is not declared by the APK.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Context: final P5 review pass
- Date: 2026-05-16

## First Hypothesis

The desktopify path currently validates the package prefix of the chosen component but does not yet validate that the component actually exists in the decoded manifest.
