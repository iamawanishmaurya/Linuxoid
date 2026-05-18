# Phase 1: Keyboard APK Intake - Research

**Researched:** 2026-05-18
**Domain:** Android APK intake, binary manifest metadata, staged runtime inventory
**Confidence:** MEDIUM

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Use `/home/astra/Downloads/keyboard-0.1.28.apk` as the real verification target.
- Stay inside Linuxoid's native direct APK path and avoid Waydroid, emulator, ADB, Android SDK, or Gradle dependencies for the runtime path.
- Keep blockers explicit instead of overstating readiness.
- Prioritize manifest/package/activity/permission/native-library/resource intake before deeper execution work.

### the agent's Discretion
- Exact helper extraction and artifact naming
- Whether to add a focused intake report surface or extend existing JSON

### Deferred Ideas (OUT OF SCOPE)
- Full managed launcher activity execution
- JNI-backed execution
- Visible Wayland interaction
- System-wide IME enablement

</user_constraints>

<architectural_responsibility_map>
## Architectural Responsibility Map

Single-tier application — all capabilities in this phase reside in Linuxoid's native C++ CLI/runtime layer.

</architectural_responsibility_map>

<research_summary>
## Summary

Phase 1 should extend Linuxoid's existing direct APK launch path rather than adding a second intake stack. The repo already stages APK sessions, inventories archive entries, exposes assets/resources, and emits structured JSON. The missing seam for a real-world verification APK is trustworthy intake of Android-native metadata from a production APK layout, especially binary `AndroidManifest.xml` and the package/activity/permission facts it carries.

The strongest implementation direction is a minimal binary-manifest decoding slice that extracts only the facts Phase 1 needs: package name, version info, SDK levels, permissions, declared activities, launcher identity, and enough component metadata to support later phases. Native-library and asset/resource inventory should continue to piggyback on the existing archive/session path. Tests should stay offline and deterministic with generated APK-like fixtures, while the real keyboard APK remains an opt-in local smoke path and milestone verification target.

**Primary recommendation:** teach `src/apk_native_launch.cpp` to ingest the real APK's manifest and inventory metadata through the existing staged-session/report path, then pin that with offline regression fixtures plus an optional local keyboard APK smoke check.
</research_summary>

<standard_stack>
## Standard Stack

The established stack for this phase is mostly the code Linuxoid already owns.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/apk_archive` layer | repo-local | ZIP/APK entry inspection and safe staged reads | Already integrated with Linuxoid's direct APK surfaces |
| Existing `wfa/apk_native_launch` path | repo-local | Session staging, package inventory, JSON reporting | Already ties together package, runtime, storage, and diagnostics |
| Existing tests in `tests/test_main.cpp` | repo-local | Deterministic CLI/regression coverage | Existing proof slices already land here |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `apktool` | local developer tool only | Decode the real target APK for manual reference | Useful for validating expected metadata during development, not as a Linuxoid runtime dependency |
| Existing `wfa/apk_asset_bridge` and `wfa/apk_permission_bridge` | repo-local | Resource/permission inventory and reporting | Reuse when Phase 1 exposes real-APK metadata through existing surfaces |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Minimal in-repo manifest decoder | Android SDK tools (`aapt2`, `apkanalyzer`) | Faster to prototype locally, but introduces external platform/tooling dependencies the project explicitly wants to avoid |
| Existing archive/session/report path | New standalone intake utility | Would fragment the runtime truth and create a second reporting path to maintain |

**Installation:**
```bash
# No new runtime dependency is preferred for Phase 1.
# Local developer reference tools such as apktool are optional only.
```
</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Recommended Project Structure
```
src/
├── apk_native_launch.cpp   # Central intake/staging/report path
├── apk_archive.cpp         # Safe archive access
├── apk_asset_bridge.cpp    # Resource/asset inventory reuse
└── apk_permission_bridge.cpp
```

### Pattern 1: Minimal fact extraction
**What:** Decode only the manifest/resource facts required by the current phase rather than full Android framework semantics.
**When to use:** When Linuxoid needs real-world APK metadata without committing to complete Android resource or framework support.

### Pattern 2: Stage once, report many
**What:** Push real-APK facts into the existing session root and reuse them across `launch-apk`, permissions, assets/resources, and future execution phases.
**When to use:** Whenever a new runtime slice needs to build on previously staged identity and artifact data.

### Anti-Patterns to Avoid
- **Full framework decoding too early:** Trying to implement all of binary XML/resources.arsc before Linuxoid can even trust the keyboard APK's package/activity metadata.
- **Tool-only truth:** Depending on `apktool` or Android SDK tooling as Linuxoid's runtime source of truth instead of using them only as a development cross-check.

</architecture_patterns>

<dont_hand_roll>
## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ZIP traversal and staged entry safety | A second archive layer | Existing `wfa/apk_archive` path | The safety checks and session integration already exist |
| Session/report layout | New artifact tree | Existing staged package/session layout | Keeps later phases on the same runtime truth |
| Full Android resource semantics | Complete `resources.arsc` runtime | Metadata-only resource inventory | Full resource-table behavior is much larger than this phase needs |

**Key insight:** The smallest successful Phase 1 is not a second parser stack; it is a sharper intake seam inside the launch path Linuxoid already uses.
</dont_hand_roll>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Binary manifest blind spot
**What goes wrong:** Real APK intake fails or hangs because the parser expects plain-text `AndroidManifest.xml`.
**Why it happens:** Synthetic fixtures and decoded manifests hide the binary XML boundary.
**How to avoid:** Add a minimal manifest decoder or a precise binary-manifest blocker path inside the launch code.
**Warning signs:** `manifest_plain_xml_required`, empty package/activity data, or long-running intake on the real APK.

### Pitfall 2: Over-scoping resource support
**What goes wrong:** Intake work expands into full Android resource-table semantics before the package/activity facts are trustworthy.
**Why it happens:** Real APKs carry a lot of resource metadata and it is tempting to treat all of it as phase-critical.
**How to avoid:** Limit Phase 1 to metadata visibility required by the verification path.
**Warning signs:** Parser work drifts into theme/layout inflation or broad framework-resource behavior.

### Pitfall 3: False compatibility optimism
**What goes wrong:** Reports imply the real keyboard APK is launch-ready when only synthetic fixtures are trustworthy.
**Why it happens:** Existing proof modes make it easy to overgeneralize from fixture success.
**How to avoid:** Keep the real keyboard APK smoke path and intake blockers explicit in reports and docs.
**Warning signs:** `launch_ready: true` for the real APK without matching package/activity/permission/native inventory.

</common_pitfalls>

<open_questions>
## Open Questions

- Is the current 20-second timeout on the real keyboard APK caused primarily by manifest parsing, resource enumeration, or a larger staging/report loop?
- What is the smallest binary manifest slice needed to extract launcher activity and permission inventory for the verification target?
- Do existing resource/permission proof surfaces need a focused real-APK intake mode to stay efficient on larger APKs?

</open_questions>

---

*Phase: 01-keyboard-apk-intake*
*Research completed: 2026-05-18*
