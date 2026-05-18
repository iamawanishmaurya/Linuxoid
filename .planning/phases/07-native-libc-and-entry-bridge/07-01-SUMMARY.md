# 07-01 Summary

## Result

Linuxoid now gets the real keyboard entry library `libjni_latinime.so` past the earlier Android-libc unresolved-symbol seam and into a smaller native startup boundary.

## What changed

- Extended the tiny Android-compat shim slice with the minimum extra fortified/libc entry points needed by the real keyboard library.
- Kept Android soname preloading explicit and deterministic through `native_execute.android_compat_state`, `elf_undefined_versions_normalized`, `android_compat_preloaded_paths`, and `android_compat_diagnostics`.
- Tightened primary-library selection so the JNI-shaped `libjni_latinime.so` becomes the authoritative entry library instead of letting helper libraries blur the seam.
- Added focused regression coverage for Android-compat and JNI-only fixture cases without introducing emulator, ADB, Waydroid, or Android SDK dependencies.

## Verified truth

- The real keyboard APK no longer stops at `undefined symbol: __strchr_chk`.
- Linuxoid now loads `libjni_latinime.so` on the direct launch path.
- The remaining upstream seam is now smaller than raw `dlopen` failure and ready for explicit post-load native reporting.
