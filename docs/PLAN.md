# Eden on Xbox (UWP): plan

Goal: port the Eden Nintendo Switch emulator (C++, GPL-3.0-or-later, yuzu lineage) to UWP for Xbox Series X|S in
Developer Mode, with a native Direct3D 12 renderer, carrying over what we learned getting Cemu running on the Xbox
(PrismFox67/Cemu-UWP). Framework from the owner's discussion with people working on Eden, revised 2026-10-22.

## Rules
- Keep GPL-3.0-or-later and every copyright notice. Code taken from other GPL ports is credited in the file and in
  the README, like the danprice142 code in Cemu-UWP.
- Never add Switch keys, firmware or game files (or anything derived from them) to the repo, to commits, logs that get
  committed, or test fixtures. Users supply their own, dumped from hardware they own. Homebrew test programs must be
  open source (build them from source, or use releases of open-source projects).
- The Cemu-UWP repo is reference only for this project: read it, never modify it from here.
- Published as the public repo PrismFox67/Eden-UWP (owner's decision, 2026-10-22); only the owner pushes. The
  upstream remotes are fetch-only references. Nothing committed may contain local paths, user names, IP addresses,
  secrets or internal tooling details.
- Platform code stays isolated (src/video_core/renderer_d3d12, a separate UWP frontend directory, small #ifdef'd hooks
  elsewhere) so rebasing onto upstream Eden stays manageable.
- Match Eden's code style. Comment sparingly. ASCII only in code and commit messages. Small, focused commits with
  short messages that say why.
- docs/NOTES.md: decisions, open questions, and what has been verified on hardware (with the build and date).
- Plan before writing large amounts of code; big architectural decisions go to the owner first.

## Revisions to the original framework (2026-10-22)
1. **Code from other ports is available.** The framework assumed the other developer's port had no code. Three
   GPL-3.0 Eden-on-Xbox ports are public and can be read and reused with credit:
   - JulianDr14/jadyx (remote `jadyx`): the furthest along. Own D3D12 backend modelled on Eden's Vulkan one,
     SPIR-V -> spirv_to_dxil -> DXIL (dxil.dll signs), FL 11.0 / SM 6.4 / no Agility SDK on the console, Dynarmic
     with W^X, placeholder memory via *FromApp APIs committed on demand (no fastmem), memory guard for the 5120 MiB
     budget, GPU ASTC -> BC1/BC3, XAudio2, own front end. Reports MK8 Deluxe / Strikers / Let's Go at 60 FPS, Wonder
     55-60, Odyssey 45-60, BotW ~20 FPS open world (GPU thread spends ~8 us recording each of ~4000 draws), TotK
     out of memory in heavy loads. Docs in docs/xbox/ (Spanish).
   - NspxMiguel/NXbox (remote `nxbox`): D3D12 via HLSL + DXC, Mesa OpenGL-on-D3D12 fallback, LAN shader cache
     sharing, GameBanana mods, NxbHelper. Being archived from 2026-10-24.
   - juanresendiz813/eden-xbox (remote `juan`): the UWP groundwork Jadyx started from (June 2026).
   Owner's decision (2026-10-22): **we build on Jadyx** (branch `main` starts from jadyx/main).
2. **We can reach the Xbox.** Our test tooling deploys packages and reads logs through the Device Portal.
   Console tests run by us; the owner is needed
   for controller sessions, visual checks and PIX captures.
3. The Cemu D3D12 backend uses SPIRV-Cross -> HLSL -> DXC. Eden-on-Xbox prior art uses spirv_to_dxil on Eden's own
   SPIR-V; compare both in Step 2 before choosing.

## Steps (check in with the owner after each)
1. **CEMU_LESSONS.md** - what Cemu taught us (UWP packaging, manifest capabilities, Dev Mode deploy/debug workflow;
   executable memory/JIT, memory mapping, file access and pickers, input, threading in the sandbox; the real D3D12
   limits on Series X Dev Mode: feature level, shader model, binding tier, memory budget as Game vs App; renderer
   architecture: descriptors, pipeline cache, barriers, upload/readback, swapchain, async compilation; bugs, runtime
   quirks, perf traps and fixes). Every item marked **verified on hardware** (with where: log, build, date) or
   **assumed**.
2. **Recon of Eden + Jadyx - docs/PORTING.md.** Map each subsystem: Eden original vs Jadyx's change vs what our Cemu
   lessons say. Prior art from nxbox/main worth taking before it is archived on 2026-10-24 (mods/GameBanana, LAN
   shader cache sharing, NxbHelper, HLSL+DXC path, anything better than Jadyx). A ranked list of improvements
   (impact x effort).
3. **Our own identity and build.** Rename the app, package identity and publisher so it installs next to Jadyx
   ("Eden UWP (unofficial)", `EdenUWP.Unofficial`); point the self-updater at PrismFox67/Eden-UWP releases or disable
   it (never Jadyx's feed or signing key); keep every Jadyx/Eden credit; README for Eden-UWP with credits; build it
   locally and in CI.
4. **Baseline on the Xbox** via QA: open-source homebrew first, real games once the owner has dumped their own.
5. **Our improvements**, profile-driven (owner captures PIX traces on request): CPU/JIT and fastmem, per-draw CPU
   cost in the D3D12 backend, the memory wall, the CPU-aware async shader queue, frontend lessons from Cemu.
6. **Feasibility tests** for anything CEMU_LESSONS.md section 6 lists that Jadyx doesn't already prove, when an
   improvement in Step 5 depends on it.


Design input from another developer's working port (from the owner's discussion): native D3D12 with spirv_to_dxil on
Eden's SPIR-V; fastmem and JIT work in the sandbox on Series X; BotW is CPU-bound (1x vs 2x resolution barely changes
FPS) - prioritise CPU emulation and threading; shader stutter comes mostly from RAM pressure and stacked translation
layers - async compile queue scheduled by queue depth and CPU load; memory is tight - unused guest memory paged to
SSD, textures/pipelines/heaps freed aggressively; Eden's multiplatform code needs stripping for Xbox; faster loading
of compressed games from slow storage reduced memory-bus pressure; Unreal Engine games render partly broken even on
Eden's Vulkan build - later milestone.
