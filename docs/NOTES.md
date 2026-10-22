# Notes: Eden on Xbox (UWP)

Decisions, open questions, and what has been verified on hardware (with build and date). Newest first.

## Decisions
- 2026-10-22: Test homebrew for the first console boot is `tools/xbox/boot_nro` (Jadyx's own open-source payload:
  a JIT liveness check plus a CPU-drawn test pattern, libnx only, no keys or firmware). CI builds it from source
  with devkitPro's official `devkitpro/devkita64` image (job `boot_nro`, artifact `boot-nro`).
- 2026-10-22: Console A/B switches go in `LocalState\boot.cfg`, which now adds to the packaged boot.cfg:
  `jit_rwx=1` (RWX code memory instead of W^X, falls back to W^X; log line `JIT code memory: ...`) and
  `readback_uncached=1` (readback buffers in a write-combined custom heap; Magnesis suspect). Both default off
  until measured on the console.
- 2026-10-22: Local build toolchain: VS 2026 (v18, toolset 14.50) with the UWP workload configures the uwp-x64 preset
  unchanged. Perl (Strawberry portable), NASM and glslang are taken from a user tools folder on PATH.
- 2026-10-22 (owner): build on Jadyx. Branch `main` starts at jadyx/main (a46b3e75b) with our docs on top; the
  first docs commits were made on `xbox` (clean Eden master) and cherry-picked. Steps revised in PLAN.md.
- 2026-10-22 (owner): published as the public repo PrismFox67/Eden-UWP; the owner pushes. App name
  "Eden UWP (unofficial)", package identity distinct from Jadyx's so both install side by side.
- 2026-10-22 (owner): the owner dumps their own keys, firmware and games from their Switch; we never download or
  reference sources for them.
- 2026-10-22: Repo set up locally from git.eden-emu.dev/eden-emu/eden (master at
  989768aa4). Fetch-only reference remotes: `jadyx` (JulianDr14/jadyx), `nxbox` (NspxMiguel/NXbox), `juan`
  (juanresendiz813/eden-xbox), `mirror` (eden-emulator/mirror).

## Open questions (for the owner)
- (none open)

## Verified on hardware
- (nothing yet for this project; Cemu facts are in CEMU_LESSONS.md)
- 2026-10-22: Profiling workload `tools/xbox/draw_stress` (our own GPL source, OpenGL via devkitPro's
  switch-mesa/glad): 4000 uniform+draw calls per frame for 60 s, logs `DRAW_STRESS fps=.. draws=..`
  each second and `DRAW_STRESS_DONE`. CI builds it with devkitpro/devkita64 (artifact draw-stress-nro).
  Not yet compiled locally (no devkitPro on this PC) - first CI run is its first build.
