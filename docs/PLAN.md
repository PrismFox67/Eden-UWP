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
- The Cemu repo (C:/Users/jason/src/Cemu-UWP) is reference only for this project: read it, never modify it from here.
- This repo stays local: no push anywhere until the owner decides where it is published (remotes have pushurl
  DISABLED). The upstream remotes are fetch-only references.
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
   Whether we build on Jadyx's code or on a clean Eden fork is the owner's decision (asked 2026-10-22). Until then,
   Steps 1-2 apply to both.
2. **We can reach the Xbox.** The swarm's QA lane deploys packages and reads logs through the Device Portal
   (swarm/Tools/Xbox/xbox.py, xbox lease). Feasibility tests (Step 3) run on the console by us; the owner is needed
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
2. **Recon and mapping - docs/PORTING.md.** Eden's video_core (renderer_vulkan, shader recompiler + SPIR-V emitter,
   texture/buffer caches, ASTC decoder) and CPU/memory code (dynarmic, fastmem paths, HostMemory). Map each part to
   our Cemu solution or flag it as new (expected new: bindless descriptors, ASTC, transform feedback, ARM64->x86 JIT,
   Eden's memory mapping APIs). Plus a prior-art section: how Jadyx/NXbox/juan solved each part, what is worth taking,
   and what they report as still broken.
3. **Feasibility gaps only.** Small UWP test apps for anything CEMU_LESSONS.md and the prior art don't already prove
   (dynarmic's executable memory, Eden's fastmem mapping calls, placeholder APIs in the AppContainer). Run them on the
   console via QA.
4. **Skeleton.** Port our Cemu UWP frontend and build setup to Eden (window, input, file access, packaging), null
   renderer, boot an open-source homebrew test program on the console.
5. **renderer_d3d12** next to renderer_vulkan behind the video_core interfaces, reusing the Cemu backend's
   architecture: SPIR-V -> DXIL, compute ASTC decoding, disk pipeline cache.
6. **Performance**, profile-driven (owner captures PIX traces on request): CPU/JIT hot paths first, then the
   CPU-aware async shader queue, then memory paging and eviction.

Design input from another developer's working port (from the owner's discussion): native D3D12 with spirv_to_dxil on
Eden's SPIR-V; fastmem and JIT work in the sandbox on Series X; BotW is CPU-bound (1x vs 2x resolution barely changes
FPS) - prioritise CPU emulation and threading; shader stutter comes mostly from RAM pressure and stacked translation
layers - async compile queue scheduled by queue depth and CPU load; memory is tight - unused guest memory paged to
SSD, textures/pipelines/heaps freed aggressively; Eden's multiplatform code needs stripping for Xbox; faster loading
of compressed games from slow storage reduced memory-bus pressure; Unreal Engine games render partly broken even on
Eden's Vulkan build - later milestone.
