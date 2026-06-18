# Mainline Virtual Audio Maintenance Implementation Plan

> Historical note: this document predates the PeakGuard rename. Current active commands and paths are documented in root `AGENTS.md` and `README.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the VB-CABLE Codex Limiter project from the retired `apo-feasibility` worktree onto `main`, remove APO-era active files, and make the project buildable from the repository root.

**Architecture:** `main` becomes the only active project branch. The source layout is root-level `common/`, `tools/`, `scripts/`, `assets/`, and `docs/`, with a root `CMakeLists.txt` delegating to each active native tool. The old `.worktrees/apo-feasibility` checkout is removed only after useful files are copied and verified.

**Tech Stack:** Windows PowerShell, Git worktrees, CMake, MSVC C++17, Win32/WASAPI.

---

## File Structure

- Create: `CMakeLists.txt` for root-level native tool build orchestration.
- Create: `README.md` for project purpose, branch status, build/test/install commands.
- Create/Copy: `common/LimiterSharedState.h`.
- Create/Copy: `tools/EndpointProbe/*`.
- Create/Copy: `tools/LimiterAudioTests/*`.
- Create/Copy: `tools/LimiterStateProbe/*`.
- Create/Copy: `tools/LimiterTray/*`.
- Create/Copy: `scripts/Install-CodexLimiter.ps1`.
- Create/Copy: `scripts/Uninstall-CodexLimiter.ps1`.
- Create/Copy: `assets/logo-candidates/*`.
- Modify: `.gitignore` to keep local build outputs, snapshots, worktrees, binaries, logs, and accidental Windows device-name artifacts out of source control.
- Remove from active source: APO feasibility documents and scripts whose only purpose is APO, SysVAD, SwapAPO, or registry probing.

## Task 1: Snapshot And Source Inventory

**Files:**
- Read: `.worktrees/apo-feasibility`
- Read: `.worktrees/apo-feasibility/.gitignore`
- Read: `.worktrees/apo-feasibility/tools/**`

- [ ] **Step 1: Record current git state**

Run:

```powershell
git status --short --branch
git worktree list --porcelain
git -C .worktrees\apo-feasibility status --short --branch
```

Expected: `main` is clean except this plan commit when already committed; `apo-feasibility` shows deleted APO-era files and untracked VB-CABLE files.

- [ ] **Step 2: List intentional source files**

Run:

```powershell
rg --files .worktrees\apo-feasibility -g '!build' -g '!snapshots' -g '!*.exe' -g '!*.dll' -g '!*.pdb' -g '!nul'
```

Expected: output includes `common`, active `tools`, `scripts/Install-CodexLimiter.ps1`, `scripts/Uninstall-CodexLimiter.ps1`, current `docs`, and `assets`.

- [ ] **Step 3: Confirm APO-only source is excluded from migration**

Run:

```powershell
rg -n "SysVAD|SwapAPO|DiagnosticApo|APO" .worktrees\apo-feasibility\scripts .worktrees\apo-feasibility\tools .worktrees\apo-feasibility\common
```

Expected: no active VB-CABLE source requires APO-only scripts. Any hits must be evaluated and removed unless they are a harmless comment in historical docs.

## Task 2: Migrate Active Files To Main

**Files:**
- Create/Copy: `common/`
- Create/Copy: `tools/`
- Create/Copy: `scripts/`
- Create/Copy: `assets/`
- Create/Copy: selected current `docs/`

- [ ] **Step 1: Copy active directories**

Run:

```powershell
Copy-Item -Recurse -Force -LiteralPath .worktrees\apo-feasibility\common -Destination common
Copy-Item -Recurse -Force -LiteralPath .worktrees\apo-feasibility\tools -Destination tools
Copy-Item -Recurse -Force -LiteralPath .worktrees\apo-feasibility\scripts -Destination scripts
Copy-Item -Recurse -Force -LiteralPath .worktrees\apo-feasibility\assets -Destination assets
Copy-Item -Recurse -Force -LiteralPath .worktrees\apo-feasibility\docs\references -Destination docs\references
Copy-Item -Recurse -Force -LiteralPath .worktrees\apo-feasibility\docs\superpowers\specs\2026-06-18-vbcable-limiter-power-performance-design.md -Destination docs\superpowers\specs
```

Expected: active source exists in `main`; build outputs and snapshots are not copied.

- [ ] **Step 2: Remove accidental local artifact if copied**

Run:

```powershell
if (Test-Path -LiteralPath nul) { Remove-Item -LiteralPath nul -Force }
```

Expected: no root `nul` file.

- [ ] **Step 3: Check copied file list**

Run:

```powershell
rg --files common tools scripts assets docs -g '!build' -g '!snapshots'
```

Expected: active VB-CABLE files appear; no `build/`, `snapshots/`, binaries, logs, or PDBs appear.

## Task 3: Add Root Build And Documentation

**Files:**
- Create: `CMakeLists.txt`
- Create: `README.md`
- Modify: `.gitignore`

- [ ] **Step 1: Add root `CMakeLists.txt`**

Create:

```cmake
cmake_minimum_required(VERSION 3.20)
project(CodexLimiter LANGUAGES CXX)

add_subdirectory(tools/EndpointProbe)
add_subdirectory(tools/LimiterAudioTests)
add_subdirectory(tools/LimiterStateProbe)
add_subdirectory(tools/LimiterTray)
```

- [ ] **Step 2: Update `.gitignore`**

Ensure it contains:

```gitignore
# Local worktrees
.worktrees/

# Build outputs
build/
out/
bin/
obj/
*.user
*.suo
*.vcxproj.user
*.log
*.etl
*.pdb
*.ilk
*.dll
*.exe
*.lib
*.exp

# Local machine state
snapshots/
nul
```

- [ ] **Step 3: Add `README.md`**

Document:

```markdown
# Codex Limiter

Codex Limiter is a Windows tray app that routes system audio through VB-CABLE, applies a simple limiter in a WASAPI loopback path, and renders the processed audio to a physical output device.

The old APO feasibility branch is retired. Active development lives on `main`.

## Layout

- `common/`: shared limiter state headers.
- `tools/LimiterTray/`: tray UI and loopback audio engine.
- `tools/LimiterAudioTests/`: native regression tests.
- `tools/LimiterStateProbe/`: shared-state inspection helper.
- `tools/EndpointProbe/`: endpoint diagnostic helper.
- `scripts/`: install and uninstall helpers.
- `assets/`: product assets.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
cmake --build build --config Release --target LimiterTray
```

If `cmake` is not on `PATH`, use the Visual Studio bundled CMake executable.

## Test

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

## Install

```powershell
.\scripts\Install-CodexLimiter.ps1 -Configuration Release
```

## Uninstall

```powershell
.\scripts\Uninstall-CodexLimiter.ps1
```
```

## Task 4: Verify Build Surface

**Files:**
- Read: `CMakeLists.txt`
- Read: `tools/*/CMakeLists.txt`

- [ ] **Step 1: Locate CMake**

Run:

```powershell
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  $cmake = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
$cmake
```

Expected: prints a valid CMake path.

- [ ] **Step 2: Configure root build**

Run:

```powershell
& $cmake -S . -B build -A x64
```

Expected: configure and generate complete successfully.

- [ ] **Step 3: Build and run native tests**

Run:

```powershell
& $cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

Expected: executable prints `LimiterAudioTests passed`.

- [ ] **Step 4: Build tray app**

Run:

```powershell
& $cmake --build build --config Release --target LimiterTray
```

Expected: Release `LimiterTray.exe` is produced under the root build tree.

## Task 5: APO Remnant Audit

**Files:**
- Read: `common/`
- Read: `tools/`
- Read: `scripts/`
- Read: `docs/`

- [ ] **Step 1: Search active source for APO terms**

Run:

```powershell
rg -n "APO|SysVAD|SwapAPO|DiagnosticApo|FxProperties" common tools scripts CMakeLists.txt README.md
```

Expected: no matches in active source. If matches exist, remove them unless they are part of a current VB-CABLE user-facing explanation.

- [ ] **Step 2: Search docs separately**

Run:

```powershell
rg -n "APO|SysVAD|SwapAPO|DiagnosticApo|FxProperties" docs
```

Expected: matches only in historical design/plan docs that are deliberately retained. Current README and active maintenance docs should state that APO feasibility is retired.

## Task 6: Commit Mainline Migration

**Files:**
- Stage all intentional source, docs, scripts, and assets.

- [ ] **Step 1: Review status**

Run:

```powershell
git status --short
```

Expected: shows intentional additions/modifications only.

- [ ] **Step 2: Stage intentional files**

Run:

```powershell
git add .gitignore CMakeLists.txt README.md common tools scripts assets docs
git status --short
```

Expected: no build outputs, snapshots, `nul`, binaries, logs, or `.worktrees/` files are staged.

- [ ] **Step 3: Commit migration**

Run:

```powershell
git commit -m "chore: move virtual audio limiter to main"
```

Expected: commit succeeds.

## Task 7: Retire APO Worktree And Branch

**Files:**
- Delete: `.worktrees/apo-feasibility` via `git worktree remove`
- Delete branch: `apo-feasibility`

- [ ] **Step 1: Verify branch content has been migrated**

Run:

```powershell
git status --short
git log --oneline -n 3
git worktree list
```

Expected: `main` is clean after the migration commit, and `.worktrees/apo-feasibility` still exists only as the old linked worktree.

- [ ] **Step 2: Remove linked worktree**

Run:

```powershell
git worktree remove .worktrees\apo-feasibility --force
git worktree prune
```

Expected: `.worktrees/apo-feasibility` is removed from `git worktree list`.

- [ ] **Step 3: Delete retired branch**

Run:

```powershell
git branch -D apo-feasibility
```

Expected: local branch deletion succeeds.

- [ ] **Step 4: Final branch verification**

Run:

```powershell
git worktree list --porcelain
git branch --list apo-feasibility
git status --short --branch
```

Expected: only `main` worktree remains, no `apo-feasibility` branch is listed, and `main` is clean.
