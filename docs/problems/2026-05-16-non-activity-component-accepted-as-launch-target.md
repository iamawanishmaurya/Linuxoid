# Problem: Non-activity component accepted as launch target

- Date: 2026-05-16
- Slug: non-activity-component-accepted-as-launch-target

## Exact Error

```text
Reviewer finding:
- Medium: desktopify-apk can still validate a same-package service, receiver, or provider as a ready launch target even though the generated wrapper always uses activity launch semantics.
```

## Reproduction Steps

1. Use `desktopify-apk` with a same-package service component such as `org.example/.SyncService`.
2. Observe that the current manifest-backed validation can treat it as declared and ready even though the launcher path is activity-only.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Context: final P5 review pass
- Date: 2026-05-16

## First Hypothesis

The manifest validator is checking “declared anywhere” instead of “declared as an activity-like component”, so it is broader than the `launch-activity` execution path it is meant to protect.
