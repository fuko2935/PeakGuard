# Mainline Virtual Audio Maintenance Design

> Historical note: this document predates the PeakGuard rename. Current active commands and paths are documented in root `AGENTS.md` and `README.md`.

## Goal

Make `main` the real project branch for the VB-CABLE based Codex Limiter, retire the APO feasibility branch, and remove APO-era project clutter.

## Current State

The repository root at `C:\Users\mansi\new\sound` is on `main` and is clean, but it only contains planning documents and the `.worktrees/` ignore rule. The active application code lives in the linked worktree at `.worktrees/apo-feasibility` on branch `apo-feasibility`.

That worktree is dirty: older APO feasibility files are deleted, newer VB-CABLE limiter files are untracked, and local build/runtime artifacts exist beside source files. This makes it unclear which branch is authoritative and which files are part of the product.

## Desired End State

- `main` contains the VB-CABLE based Codex Limiter source, scripts, tests, assets, and docs.
- `apo-feasibility` no longer carries active product work.
- The `.worktrees/apo-feasibility` checkout can be removed after its useful source files are migrated.
- APO feasibility docs, scripts, snapshots, and implementation remnants are removed unless they still document a current VB-CABLE decision.
- Build and test commands are discoverable from the repository root.
- Generated files, local snapshots, binaries, and Windows accidental artifacts such as `nul` are ignored or deleted.

## Scope

This maintenance pass covers:

- Migrating current VB-CABLE limiter source from `.worktrees/apo-feasibility` into `main`.
- Adding a root CMake entry point that builds the active tools from one place.
- Keeping `common/`, `tools/`, `scripts/`, `assets/`, and `docs/` as the top-level source layout.
- Removing APO-named scripts and feasibility documents from active source control.
- Adding concise project documentation for build, test, install, uninstall, and branch status.
- Running available build and test verification.

This pass does not redesign the limiter DSP, VB-CABLE routing model, UI behavior, or installer behavior. Behavioral refactors can happen after the repo is clean and buildable from `main`.

## Architecture

The project should use this layout:

```text
common/                  Shared state and utility headers used by multiple tools.
tools/EndpointProbe/      Diagnostic endpoint inspection tool.
tools/LimiterAudioTests/  Lightweight native test executable.
tools/LimiterStateProbe/  Shared-state inspection tool.
tools/LimiterTray/        Tray UI and WASAPI loopback limiter app.
scripts/                  User-facing install/uninstall/build helper scripts.
assets/                   Product image assets that are intentionally source controlled.
docs/                     Current design, plan, and developer documentation.
```

The root `CMakeLists.txt` should add the active tool subdirectories. Each tool keeps its own `CMakeLists.txt` so it can still be built independently when needed.

## Migration Rules

- Treat `.worktrees/apo-feasibility` as the source of the current VB-CABLE implementation.
- Copy only intentional project files into `main`.
- Do not copy `build/`, `snapshots/`, generated binaries, logs, PDBs, ETL traces, or the accidental `nul` file.
- Do not keep files whose only purpose was APO feasibility, SysVAD patching, APO registry probing, or diagnostic APO installation.
- Preserve the current VB-CABLE limiter files and product assets.
- After migration and verification, delete the `apo-feasibility` linked worktree and delete the `apo-feasibility` branch.

## Validation

Minimum validation:

- `git status` on `main` shows an intentional, reviewable change set.
- Root CMake configure succeeds when a CMake executable is available.
- `LimiterAudioTests` builds and passes.
- `LimiterTray` builds in Release.
- A text search for `APO`, `SysVAD`, and `SwapAPO` finds only historical design references that are deliberately kept, or no active source references.
- `git worktree list` no longer includes `.worktrees/apo-feasibility` after cleanup.
- `git branch --list apo-feasibility` returns no active local branch after branch deletion.

## Risks

- Copying the dirty worktree blindly could bring local artifacts or obsolete APO evidence back into source control.
- Deleting the branch before migration would risk losing uncommitted VB-CABLE work.
- Moving directly into code refactors before root build hygiene would make verification harder.
- Removing too much documentation could erase useful rationale for why APO was abandoned; keep only concise current-facing notes if needed.

## Success Criteria

- A fresh checkout of `main` contains the actual application, not only docs.
- A developer can build and test from the repo root.
- The branch/worktree story is simple: active work is on `main`; APO feasibility is retired.
- APO-related active source files and scripts are gone.
- Verification results are recorded in the final handoff.
