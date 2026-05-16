# Linuxoid Self-Healing Browser Architecture

## Status

This track is a researched target slice only. It is **frozen until `P5 Audio + Network + Browser`** while Linuxoid works through `P0 Freeze & Triage`, `P1 NDK Execution Core`, `P2 Window + Graphics`, `P3 DEX + ART Bridge`, and `P4 Binder + Services`.

## Goal

Linuxoid should eventually expose an Android browser that keeps trying to satisfy user intent when the page, browser process, or Android system UI gets in the way, while still stopping safely when scope or risk expands.

This browser track should stay aligned with Linuxoid's larger goal:

- direct Android-app-on-Linux execution
- MCP- and harness-friendly control surfaces
- deterministic artifacts and replayable evidence
- fail-closed behavior instead of optimistic false success

## Recommended Foundation

Start with a **Linuxoid-owned browser shell built on the Android `WebView` API surface**.

Why this is the best first path:

- it matches Android app compatibility work better than porting a full Chromium browser UI stack
- it exposes the hooks Linuxoid needs for navigation, prompts, permissions, downloads, cookies, renderer crashes, and recovery
- it lets Linuxoid own the self-healing logic outside the page instead of burying it inside ad hoc browser UI code

This should **not** begin as a `Chrome.apk` port or a full Chromium/CEF-style browser shell.

## Core Design

The browser should have five explicit layers:

1. **Intent Planner**
   - normalizes the user request into `goal`, `subgoals`, `allowed scope`, and `required capabilities`

2. **BrowserSession**
   - one WebView-backed browser session
   - one Linuxoid profile directory
   - one machine-readable run/session journal

3. **Recovery Supervisor**
   - classifies failures
   - chooses the smallest recovery scope
   - preserves completed subgoals
   - escalates or stops when recovery is unsafe

4. **Policy Layer**
   - caps retries and redirects
   - blocks destructive or out-of-scope auto-recovery
   - enforces domain/package/capability boundaries

5. **Machine-facing Contract**
   - commands, reports, traces, replay artifacts, and eval artifacts that agents and harnesses can consume directly

## Failure Taxonomy

Linuxoid should classify failures before retrying:

- `target_failure`
  - missing selector
  - stale node
  - hidden or disabled target
  - frame or popup change

- `flow_failure`
  - expected navigation did not happen
  - wrong page reached
  - modal or chooser blocked progress

- `permission_prompt_failure`
  - JS dialog
  - Android runtime permission sheet
  - WebView permission request
  - file chooser or geolocation prompt

- `page_failure`
  - HTTP error
  - SSL failure
  - safe-browsing interstitial

- `browser_failure`
  - renderer unresponsive
  - renderer crashed
  - tab or session vanished

- `policy_failure`
  - CAPTCHA
  - MFA
  - destructive confirmation
  - scope expansion

## Recovery Loop

Recommended Linuxoid recovery sequence:

1. capture a compact state snapshot
2. execute one action
3. classify the failure if it fails
4. retry only if it is transient
5. otherwise re-plan on the same page
6. then try an alternate path
7. then reload or recreate the browser session if necessary
8. stop if scope, safety, or identity checks fail

Recommended bounded policy:

- `2` fast same-step retries after re-wait or re-fetch
- `1` re-grounded retry
- `2` alternate plans per subgoal
- hard stop on repeated destructive ambiguity or scope drift

## Auto-Heal Rules

Linuxoid may auto-fix:

- stale or missing elements after rerender
- lost focus, viewport, or keyboard state
- short navigation delays
- same-task tab/frame changes
- benign overlays or cookie banners if target identity is still proven

Linuxoid must stop on:

- auth, MFA, payments, checkout, installs, clipboard writes
- cross-domain or cross-package drift outside allowed scope
- ambiguous target identity
- permission grants outside explicit policy
- repeated mutation risk
- security warnings, prompt injection, or external-app handoffs

## BrowserSession Requirements

The first BrowserSession slice should include:

- one `android.webkit.WebView` surface
- Linuxoid-managed cookies and storage profile
- DOM/message bridge
- permission broker
- download broker
- recovery supervisor
- lifecycle/session state
- machine-readable journal

Prefer these hooks:

- `WebViewClient`
- `WebChromeClient`
- `WebViewCompat`
- `CookieManager`
- `ServiceWorkerController`
- `TracingController`
- Android accessibility fallback for browser chrome and system prompts

## MCP and Harness Contract

Every browser run should live under a stable path like:

```text
packages/<package>/<install_id>/browser-runs/<run_id>/
```

Suggested artifacts:

- `request.json`
- `result.json`
- `trace/events.jsonl`
- `trace/latest.json`
- `snapshots/<step_id>/...`
- `replay/replay.json`
- `eval/eval-spec.json`
- `eval/eval-result.json`

Suggested machine result envelope:

```json
{
  "contract_version": "1",
  "command": "browser-act",
  "run_id": "example",
  "backend": "native-webview",
  "package_name": "org.linuxoid.browser",
  "intent": "open account settings",
  "status": "partial",
  "exit_class": "recovery_stop",
  "failure_code": "permission_prompt_failure",
  "repairable": false,
  "artifacts": {},
  "metrics": {},
  "next_action": "ask_user"
}
```

Rules:

- JSON is authoritative
- text reports are secondary
- trace events are append-only
- artifact paths stay deterministic
- outputs remain backend-neutral

## Recommended Next Steps

Do not implement these before `P5`. When the browser track is unfrozen, the next steps are:

1. Add a BrowserSession architecture scaffold to Linuxoid docs and command surfaces
2. Add a recovery policy module with explicit failure codes and retry budgets
3. Add a DOM/message bridge plus Android accessibility fallback
4. Add trace, replay, and eval artifact generation
5. Add permission and download brokers

## Validation Targets

Use these benchmark families as future validation inputs:

- AndroidWorld
- WebArena
- BrowserGym
- ST-WebAgentBench

Success should require both:

- task completion
- zero unauthorized side effects

## Research Basis

This architecture note synthesizes:

- Linuxoid repo constraints and artifact patterns
- Android WebView and AndroidX WebKit APIs
- MCP structured tool/resource patterns
- browser-agent recovery patterns similar in spirit to Hermes-style persistent browser automation
