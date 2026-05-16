# Changelog

## v0.1.12 - 2026-05-16

- Add `verify-apk-host-launch-auto` so Linuxoid can verify the full local-APK Linux launcher flow against a live Android runtime.
- Verify the new APK-backed path live with `/home/astra/Downloads/keyboard-0.1.28.apk`, reaching `Ready for typing: yes` through the generated Linux launcher.
- Raise verified project loading to `89/100` while checkpoint gates remain `70/100`.

## v0.1.11 - 2026-05-16

- Add the current full working Mermaid architecture to the GitHub README so the live Linuxoid flow is visible at a glance.
- Document both verified execution paths in the README diagram: APK-backed IME provisioning and installed-package Waydroid launch verification.

## v0.1.10 - 2026-05-16

- Add `verify-waydroid-matrix` so Linuxoid can verify several installed Waydroid apps in one pass and report package-level pass/fail results.
- Extend the Waydroid integration layer with matrix reporting that keeps running even when one app fails, so compatibility gaps surface honestly instead of aborting the whole batch.
- Keep verified project loading at `88/100` while raising the repeatability of the direct-on-Linux verification workflow.

## v0.1.9 - 2026-05-16

- Add `verify-waydroid-package` so Linuxoid can prove a direct Linux launch path for an installed Waydroid app through three checks: runtime launch, launcher generation, and generated launcher execution.
- Verify the new direct-on-Linux path on live Waydroid with `com.android.calculator2`, `com.android.settings`, and `org.fdroid.fdroid`.
- Raise verified project loading to `88/100` while keeping checkpoint gates at `70/100`.

## v0.1.8 - 2026-05-16

- Add `launch-waydroid-package` so Linuxoid can launch installed non-IME apps on Waydroid without requiring APK reinstall or hardcoded component selection.
- Add `desktopify-waydroid-package` so Linuxoid can generate Linux launchers and `.desktop` entries for installed Waydroid apps.
- Verify the new installed-package path on live Waydroid with `com.android.calculator2`, including a Linuxoid-generated host launcher execution from Linux.
- Raise verified project loading to `87/100` and checkpoint gates to `70/100`.

## v0.1.7 - 2026-05-16

- Normalize fully qualified IME identifiers to the short `package/.Class` form before Linuxoid sends `ime enable` and `ime set`, fixing the Waydroid mutation failure for the keyboard app.
- Verify the Linuxoid-generated host launcher against a live Waydroid runtime and reach `Ready for typing: yes` for the FUTO keyboard APK from Linux.
- Raise verified project loading to `84/100` while runtime checkpoint gates remain `58/100`.

## v0.1.6 - 2026-05-16

- Rename the project-facing identity to `Linuxoid` and point the local Git remote at `https://github.com/iamawanishmaurya/Linuxoid`.
- Add `desktopify-apk-auto`, which infers the launcher activity from the APK manifest instead of requiring a caller-supplied component.
- Split desktop-entry and launcher-script roots so Linuxoid can target host-discoverable application directories without forcing the wrapper script to live beside the `.desktop` file.
- Raise verified project loading to `78/100` while keeping runtime checkpoint gates at `58/100`.

## v0.1.5 - 2026-05-16

- Add a generic `launch-activity` bridge for explicit Android component launches from Linux.
- Add a `desktopify-apk` path that generates a Linux wrapper script and `.desktop` entry for a caller-selected Android component.
- Harden component matching so the runtime treats short and fully qualified Android component forms as equivalent.
- Raise verified project loading to `76/100` and runtime checkpoint gates to `58/100`, with host integration now in active validation.

## v0.1.4 - 2026-05-16

- Add a real `provision-ime` path that installs a keyboard APK on a live Android target, enables the IME, sets it as default, and re-verifies the resulting runtime state.
- Extend live runtime verification to prove the target IME is explicitly present in `enabled_input_methods` in addition to being registered and selected as default.
- Guard the provisioning verdict against wrong-APK/package mismatches and preserve partial evidence when post-action readback fails.
- Raise verified project loading to `71/100` and runtime checkpoint gates to `48/100`, with the golden-app launch and repeatability checkpoints now complete.

## v0.1.3 - 2026-05-16

- Add a real `load-apk` path that stages APKs into a compat root using decoded manifest and `apktool.yml` metadata.
- Add an `adb-ime-status` runtime bridge that verifies install state, IME registration, default IME selection, and settings launch on a live Android target.
- Harden the loader/runtime slice with install-key sanitization, exact ADB matching, optional launch verification, and unique cleaned temp decode directories.
- Raise verified project loading to `65/100` and runtime checkpoint gates to `28/100`.

## v0.1.2 - 2026-05-16

- Add a Phase 4 decoded-manifest assessor for runtime and service requirements.
- Add CLI support for `compatctl assess-manifest`.
- Verify the keyboard APK manifest path and raise Phase 4 loading to `45/100`, bringing overall phase loading to `58/100`.

## v0.1.1 - 2026-05-16

- Add the first C++ MVP scaffold with `compatctl`, a checkpoint engine, and package-layout planning.
- Add local build and test verification for the new scaffold.
- Add reproducibility notes in the project README.

## v0.1.0 - 2026-05-16

- Bootstrap the repository, documentation workflow, and MVP planning baseline.
- Establish English-only project documentation and commit policy from the first version.
