# MVP Foundation

## Chosen Strategy

- Runtime direction: container-first Android userspace integration
- Core implementation language: C++
- Secondary language later: Rust for isolated helper services behind stable C ABI boundaries
- First build target: a host-side MVP scaffold that tracks checkpoints, models package layout, and records measurable progress

## Critical Decision: C++ vs Rust

### Option A: C++

- Strengths:
  - Best fit for Linux and Android native integration, especially graphics, Binder, and low-level host plumbing
  - Lowest friction for the first systems-facing MVP slice
  - Aligns with the long-term compatibility-layer core
- Trade-offs:
  - More memory-safety risk than Rust
  - More discipline required around ownership and boundaries

### Option B: Rust

- Strengths:
  - Strong safety guarantees and excellent tooling
  - Faster for purely greenfield service logic and deterministic packaging
- Trade-offs:
  - More FFI boundary work for Android-native integration
  - Higher friction for the earliest compatibility-layer plumbing

## Decision

Choose `C++` for the first MVP core.

Reason: the first meaningful slice needs to model the compatibility-layer core, not only the repo tooling. The initial build should stay close to the low-level integration path we will eventually need.

## Confidence Loop

### Initial confidence: 82/100

- Risk: a pure C++ start could slow basic tooling and reproducibility
  - Fix: use standard-library-only C++20 and a minimal CMake build with no external dependencies
- Risk: the repo could drift into architecture talk without executable proof
  - Fix: build a runnable CLI and test suite in the first slice
- Risk: progress percentages could become decorative instead of evidence-based
  - Fix: encode checkpoints and phases directly in code and verify them through testable output
- Risk: push verification is blocked by the missing `origin` remote
  - Fix: continue local commits, preserve the logged blocker, and avoid re-triggering the same push failure until the remote is configured

### Re-evaluated confidence: 100/100

The scoped strategy is now narrow enough to execute with full confidence:
- implement a C++ MVP scaffold
- verify it with local build and tests
- show truthful phase and checkpoint progress
- keep remote publishing explicitly blocked, documented, and out of the critical path

## Phase Progress Model

Each phase is shown as a completion value from `0` to `100`.

| Phase | Meaning | Current target state for this slice |
|---|---|---:|
| P1 | Research and architecture lock | 100 |
| P2 | Build and tooling scaffold | 100 |
| P3 | Package and storage contract | 100 |
| P4 | Runtime and service contract | 45 |
| P5 | Graphics and host integration | 0 |
| P6 | APK execution and validation | 0 |

## Evidence Checkpoints

The runtime checkpoints remain stricter than the implementation phases.

| Checkpoint | Weight | Current expected state after this slice |
|---|---:|---:|
| C1 Environment Reproducibility | 15 | 50 |
| C2 Golden App Launch | 20 | 0 |
| C3 Representative Compatibility Set | 25 | 0 |
| C4 Host Integration | 20 | 0 |
| C5 Repeatability and Regression Guard | 20 | 50 |
