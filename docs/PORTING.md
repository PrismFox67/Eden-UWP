# Porting map: Eden, Jadyx and what Cemu taught us

Step 2 of docs/PLAN.md, revised for the owner's decision to build on Jadyx (2026-10-22). For each subsystem: what
Eden does, what Jadyx changed, what our Cemu lessons (docs/CEMU_LESSONS.md, "CL" below) say, and whether anything is
NEW to us. Then prior art from NXbox and juan, and a ranked list of improvements.

Read-only recon of `jadyx/main` (a46b3e75b, 2026-10-21), `nxbox/main` (b5e1d57a1, 2026-10-22) and
`juan/development` (33969e857, 2026-06-17). Jadyx's docs are in Spanish; everything quoted here is summarised in
English. Numbers from Jadyx and NXbox are their own measurements unless marked otherwise.

## 1. Where the code stands

| | Jadyx (our base) | NXbox | juan |
|---|---|---|---|
| Eden base | d16735f5b (2026-10-02), 20 commits behind Eden master 989768aa4 | same base, 20 behind | 5219b9f3d (2026-06-10), 300 behind |
| Own commits | 590 | 1511 | 31 |
| Total diff vs base | 1142 files, +149k / -1.5k | 2302 files, +258k / -83k | 17 files, +515 / -11 |
| Shared Eden code touched | 504 files, +27k / -1.3k | (more, incl. Android) | 17 files |
| Frontend | `src/jadyx` 181 files, +37k | `src/eden_uwp` + `src/nxbox_launcher`, +34k | none (headless boot) |
| D3D12 renderer | `src/video_core/renderer_d3d12` 129 files, +35k | 115 files, +33k, plus Mesa GL-on-D3D12 fallback | none |
| Shader path | Eden SPIR-V -> Mesa spirv_to_dxil -> DXIL | own HLSL backend (`shader_recompiler/backend/hlsl`, +7k) -> DXC | none |
| Tests / tools | `tools/xbox` (+ tests, ~16% of changed files) | `tests/port`, `tools/nxbox*` | none |
| Status | most games run; MK8D, Strikers, Let's Go 60 FPS, Odyssey 45-60, BotW ~20 open world, TotK hits the memory wall | SM3DW 1-1 ~59 FPS on D3D12; BotW ~12 FPS on the GL path; archived from 2026-10-24 | groundwork only |

Platform isolation in Jadyx: the frontend and renderer are in their own directories, but 504 shared files changed,
with large additions in `common/host_memory.cpp` (+1.5k), `core/pulse_*` (session save/resume, ~3k),
`common/bug_tracker.cpp`, dynarmic's `block_of_code.cpp`, kernel scheduling (`k_thread`, `physical_core`,
`core_timing`) and the texture cache (+400). Rebasing onto newer Eden will conflict mostly in kernel, texture cache
and service code. Plan: rebase Jadyx's 590 commits as a unit only when Eden master gains something we need; otherwise
merge Eden periodically.

## 2. Subsystem map

Status key: **CL** = Cemu already solved it (section of CEMU_LESSONS.md), **J** = Jadyx solved it on hardware,
**NEW** = nobody has solved it, **GAP** = solved but known to be weak.

### 2.1 Packaging, frontend, files, input, audio
| Part | Eden | Jadyx | Cemu lesson | Status |
|---|---|---|---|---|
| UWP app shell | Qt / SDL frontends | C++/WinRT CoreApplication (`src/jadyx/app`), own D3D12 UI | CL 1.1, 2.6 | J |
| Manifest | n/a | codeGeneration, internetClient, privateNetworkClientServer, packageManagement, HID | CL 1.1 (we also used broadFileSystemAccess, removableStorage, expandedResources) | J; consider expandedResources |
| Files | std::filesystem, SDL paths | LocalState + USB folders opened through the broker (`xbox_rom_storage.md`), games read in place | CL 2.4: typed `E:` paths with ALL APPLICATION PACKAGES are fast; `\\?\`; lexically_relative; FlushFileBuffers | J; check long paths and flush on USB |
| Keys/firmware/games import | Qt dialogs | network portal (HTTP upload from a PC/phone), USB import | - | J |
| Input | SDL2 | XInput / Windows.Gaming.Input, per-game controller type, no silent pad replacement | CL 2.5 (topology change mid-game, BackRequested) | J |
| Audio | cubeb / SDL | XAudio2 2.9 sink (+651 lines) | CL 2.6 (XAudio 2.8 on our console) | J |
| Updates | n/a | signed self-updater via PackageManager | - | disabled in our fork until we have a key |
| Diagnostics | log file | `jadyx_diag.txt`, crash logger, sampling profiler, frame traces, bug tracker | CL 1.3 (session marker, previous log, GPU watchdog, memory log) | J; compare and fill gaps |

### 2.2 CPU: dynarmic and core/arm
| Part | Eden | Jadyx | Cemu lesson | Status |
|---|---|---|---|---|
| Code memory | RWX on desktop | `VirtualAllocFromApp` + **W^X**: pages toggled RW <-> RX with `VirtualProtectFromApp` per write window (`block_of_code.cpp`); comment says "RWX is never grantable under the AppContainer" | CL 2.1: **RWX via `VirtualProtectFromApp(PAGE_EXECUTE_READWRITE)` works on Series X** (Cemu logs, 2026-10-20) | **Conflict, test first** (see 4.1) |
| Code cache size | 512 MiB+ | capped at 128 MiB per core on Xbox, compact block metadata | - | J |
| Warm-up | none | per-game JIT prewarm profile (`jit_prewarm_*`, +700) | - | J |
| Core idle | OS waits | spin with MWAITX/UMWAIT/pause before blocking (~40 us OS wake) | - | J |
| Stacks | 8 MiB fibers | fiber stacks commit 64 KiB and grow (saved ~500 MiB) | CL 2.3 (128 MiB reserve, no commit) | J |
| 32-bit games | A32 JIT | works | - | J |
| Persistent JIT cache | none | none | - | NEW (NXbox has an opt-in, small-coverage attempt) |

### 2.3 Guest memory: common/host_memory, core/memory, kernel
| Part | Eden | Jadyx | Cemu lesson | Status |
|---|---|---|---|---|
| Backing | 4 GiB+ pagefile section, `VirtualAlloc2` placeholders, views | placeholders through `VirtualAlloc2FromApp` / `MapViewOfFile3FromApp`; backing is private memory committed on demand by a vectored exception handler in 64 KiB chunks | CL 2.2: Cemu committed up front, never used placeholders | J |
| Fastmem | arena that mirrors the guest address space | **off on Xbox**: full fastmem fails, the console limits mapped views to about 1 GiB (their measurement); a "hybrid" 384 MiB hot window covered ~15% and ended in bad_alloc (0.2.63.0); page table used instead | CL 6.1-6.2 (unproven for us) | **GAP**: biggest CPU cost in heavy games (on PC, Game C dropped to 30 Hz without fastmem) |
| Commit budget | n/a | memory ledger (who owns each MiB), memory guard shrinks caches near 5120 MiB | CL 3, 4 (eviction thresholds) | J |
| Kernel | HLE kernel | many scheduling changes (k_thread, physical_core, core_timing, condition variable, arbiter) | - | J |
| Session save/resume | none | "Pulse": saves and resumes a running game (~3k lines in core) | - | J (big, rebase risk) |

### 2.4 GPU: video_core
| Part | Eden (Vulkan) | Jadyx (D3D12) | Cemu lesson | Status |
|---|---|---|---|---|
| Device / caps | Vulkan device | FL 11.0, SM 6.4, no Agility SDK, `SraKmd_arden` | CL 3 table (same adapter; binding tier 3) | J |
| Shaders | Maxwell -> IR -> SPIR-V | IR -> SPIR-V (extra passes: local memory scratch, demote return) -> Mesa spirv_to_dxil (patched, `tools/xbox/mesa`) -> DXIL, signed by `dxil.dll` | CL 4: SPIRV-Cross -> HLSL -> DXC works; spirv_to_dxil never ran on our console | J |
| Shader cache | Vulkan pipeline cache + disk SPIR-V | DXIL cached per game on disk, rebuilt in parallel at boot | CL 3: no pipeline library / blobs, never call them on SraKmd | J |
| Pipelines | VkPipeline cache | PSO built when first drawn, released when idle (pressure-dependent residency), hot set of recent sessions (`d3d12_hot.bin`, 16 sessions, up to 4096 at boot), shared shader reads | CL 4: records + usage history + per-level preload + quick start + Smooth mode + 2 s wait cap + blocklist of crashing PSOs | J; take blocklist and per-scene preload |
| Descriptors | descriptor sets / push descriptors | fence-retired shader-visible ring, table reuse by content, interned samplers | CL 4 (same pattern) | J |
| Per-draw CPU | ~1 us/draw on desktop Vulkan | ~8 us/draw, 32 ms for 4000 draws in BotW open world; experimental pipeline key reuse | CL 4 (Cemu rebinds everything per draw too) | **GAP** |
| Textures | texture_cache + Vulkan images | texture_cache (+400), BC block copies, SRV dimension fixes, virtual BC dimensions | CL 3: BC arrays on Series, round layer indices | J |
| ASTC | compute decode to RGBA8 | compute decode to BC3/BC1 (`d3d12_astc_gpu_decoder.cpp`, `d3d12_bc3_encoder.comp`), arrays too | - | J |
| Buffers / upload | staging ring | upload ring capped at 128 MiB | CL 4 (overflow submit early) | J |
| Readback | host-visible coherent | readback heaps; Magnesis (BotW) still fails on Series only, works on PC | CL 3: ranged `Map` on Series readback (our Cemu fix for the same symptom) | **GAP**: try ranged Map |
| Queries / conditional rendering | Vulkan queries | `SetPredication` on the GPU | CL 4 | J |
| Transform feedback, geometry passthrough | Vulkan ext | geometry passthrough lowered; TFB status not documented | Cemu used SSBO TFB | check |
| Present | Vulkan swapchain | present manager, FSR, CPU present fallback | CL 2.6: 60 Hz only, HDMI resolution | J |
| Device removal | crash | returns to the library (still being validated on Series) | CL 3: the old removed device is handed back; restart needed | GAP: verify |
| Frame generation | some | none | Cemu interpolation abandoned (60 Hz) | skip |

## 3. Prior art beyond Jadyx

### 3.1 NXbox (NspxMiguel, archived from 2026-10-24)
Worth reading before it disappears (it stays in our `nxbox` remote):
- **HLSL backend + DXC** (`shader_recompiler/backend/hlsl`, ~7k lines). An alternative to spirv_to_dxil without the
  Mesa build; SM3DW lit at ~59 FPS. Keep as a reference if spirv_to_dxil causes Series-only divergences.
- **LAN shader cache sharing** (`src/eden_uwp/shader_share.*`): a second console skips compilation. Small and
  self-contained: candidate to port.
- **GameBanana mods**: Jadyx already has GameBanana + CNX; compare UX only.
- **NxbHelper** (`tools/nxbox-pack`): a PC tool that checks and prepares the USB drive. Jadyx uses a network portal;
  a USB checker is a nice extra, low priority.
- **Docs with measurements**: `DRAW-CPU.md` (MK8D race: ~1019 draws, 33 ms GPU-thread CPU, 32 us/draw on their
  backend), `jadyx-comparison.md` (descriptor, pipeline and warm-up comparison with Jadyx), deferred recording
  (a recorder thread that takes draw snapshots off the GPU thread), `FASTMEM-crash.md` (fastmem boot faults in the
  alias arena), `jit-disk-cache.md` (persistent JIT needs symbolic relocation; opt-in, small coverage),
  `guest-memory-budget.md`. Their deferred recording is the most relevant idea for our per-draw CPU problem.
- Mesa OpenGL-on-D3D12 fallback: slow (BotW ~12 FPS), not worth carrying.

### 3.2 juan (juanresendiz813/eden-xbox)
The 17-file groundwork Jadyx started from: CMake presets for UWP, `*FromApp` host memory, dynarmic UWP switches,
spin lock fix, path utilities. Fully superseded by Jadyx; nothing to take.

## 4. Ranked improvements (impact x effort)

1. **RWX code memory instead of W^X** - high impact, low effort. Cemu got RWX through `VirtualProtectFromApp` on Series
   X; Jadyx assumes it is impossible and pays a protect call per JIT write window. A 1-day test (switch on dynarmic's
   RWX path in the UWP build, check the log on the console, compare BotW/Odyssey frame time). Risk: different
   behaviour for many small regions; fall back to W^X automatically.
2. **Per-draw CPU cost** (~8 us/draw) - highest impact for BotW/Odyssey, high effort. Profile on PC first
   (Jadyx has a sampling profiler), then: state diffing so unchanged root tables and PSOs are not rebound, NXbox's
   deferred recording thread, Jadyx's pipeline key reuse made default once proven.
3. **Readback coherence (Magnesis)** - medium impact, low effort. Apply our ranged-Map fix (Cemu 17aab5a) to Jadyx's
   query/readback paths and test BotW Magnesis on the console.
4. **Memory wall** - high impact for TotK-class games, medium-high effort. Find the ~400 MiB of unattributed heap
   blocks (ledger + heap walk), texture resolution cap option, earlier eviction; HeapCompact under pressure (Cemu).
5. **Pipeline stutter** - medium impact, medium effort. Add Cemu's blocklist of PSOs that crash the driver,
   per-scene preload records, and a CPU-aware async queue (compile threads scaled by queue depth and emulation load).
6. **Fastmem within the view limit** - high impact, high effort, uncertain. The ~1 GiB view limit and the failed hybrid
   are Jadyx's measurements; revisit with larger alias granularity or a hot-set chosen from runtime fault statistics.
   Only after 1 and 2.
7. **LAN shader cache sharing** from NXbox - low-medium impact, low effort.
8. **Frontend lessons from Cemu** - low effort each: typed USB paths with ALL APPLICATION PACKAGES, flush on USB,
   session marker + crash notice, GPU watchdog, `\\?\` paths, memory log every 30 s.
9. **Merge Eden master** (20 commits) - maintenance, low effort now, grows with time.

## 5. On the base question

Decided by the owner on 2026-10-22: build on Jadyx. The evidence agrees: Jadyx runs commercial games at 60 FPS on
Series X with a D3D12 renderer modelled on Eden's Vulkan backend, is only 20 commits behind Eden master, keeps the
frontend and renderer in their own directories, and has tests and console-tested docs. A clean Eden fork would spend
months reaching the same point. The costs are its 504 changed shared files (rebase effort) and a large custom Mesa
build for spirv_to_dxil.
