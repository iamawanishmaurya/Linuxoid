# Conventions

This codebase has several strong local conventions worth preserving.

Naming conventions:
- bridge interfaces use `apk_*_bridge` names for direct APK session slices
- fixture and smoke helpers use `*_fixture` or `*_smoke`
- report renderers often use `Render*Json` or similar explicit names

Behavioral conventions:
- report exact blockers instead of overstating support
- persist deterministic JSON artifacts under staged session roots
- keep tests offline and reproducible
- use fixture APK-like ZIP payloads instead of network or SDK builds

Current execution-first conventions:
- prove the smallest real execution seam possible
- name unsupported boundaries precisely
- distinguish `framework-stubbed`, `object-placeholder`, and real execution
- keep the next blocker explicit in status and docs

Documentation conventions:
- phase progression is recorded in `README.md`, `CHANGELOG.md`, `docs/phased-build-plan.md`, and `docs/steps.md`
- runtime honesty is reinforced in `docs/self-healing-runtime-skeleton.md`
- the phrase `Self-Healing Android Device` is part of the project/runtime language

CLI conventions:
- `compatctl` owns the operator surface
- proof modes use flags such as `--first-app-start-proof`
- inspection commands use names like `inspect-apk-*`

Code style conventions visible in source:
- standard library and explicit structs over metaprogramming-heavy style
- deterministic string values for states and blockers
- minimal abstraction around JSON string assembly where needed

Testing conventions:
- most regression coverage is concentrated in one large `tests/test_main.cpp`
- fixtures are deterministic and synthetic
- build verification is normally `cmake --build build && ctest --test-dir build --output-on-failure`

Any new work should align with these proof-oriented and report-heavy patterns rather than introducing hidden magic.
