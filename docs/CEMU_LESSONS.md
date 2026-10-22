# What the Cemu Xbox port taught us

Step 1 of docs/PLAN.md. Everything we learned getting Cemu (PrismFox67/Cemu-UWP, branch `d3d12-backend`) running on
an Xbox Series X in Developer Mode, written down before any Eden code is touched.

Every item is tagged:
- **[HW]** verified on hardware, with where (Xbox log in `swarm/Logs/Agents/xbox_logs/<build>/`, commit, date).
- **[PC]** verified on a Windows PC only (Win32 host or the UWP package on Windows).
- **[assumed]** documented, reported by others, or inferred, but never proven by us on the console.

Paths starting with `Cemu:` are relative to `C:/Users/jason/src/Cemu-UWP`. Build ids are Cemu-UWP commits.

## 1. Packaging, deployment and debugging

### 1.1 Package and manifest (`Cemu:src/gui/host/uwp/Package.appxmanifest`)
- `TargetDeviceFamily Windows.Universal`, MinVersion 10.0.17763.0, MaxVersionTested 10.0.22621.0. Installs and runs on
  the console. **[HW]** (every build since 0.1.x, e.g. aeaba14, 2026-10-20)
- Capabilities in use: `internetClient`, `privateNetworkClientServer` (DSU gyro over LAN), `codeGeneration` (the JIT),
  `uap:removableStorage`, `rescap:broadFileSystemAccess`, `rescap:expandedResources`. **[HW]** (package installs and
  runs with them; restricted capabilities are accepted in Dev Mode)
- `codeGeneration` is what lets the process have executable memory at all (see 2.1). **[HW]** indirectly: the
  recompiler runs on the console with it. Running without it was never tried. **[assumed]**
- A `windows.protocol` extension (`cemu:?path=...`) starts a game directly. **[PC]**
- The package can't carry some file types: `.asm` files had to be packaged as `.asm.txt` and renamed at first start
  (12c63d4). **[HW]** (packs install on the console after 51af729). Eden will ship few data files, but keep this in
  mind for anything with an unusual extension.
- Every CI build is signed with a fresh test certificate and gets a higher version `0.1.<run>.0`, so an install over
  the old one is an update that keeps LocalState and the App type setting (15186ac). **[HW]**
- Dependency: `Microsoft.VCLibs.x64.14.00.appx` must be deployed with the msix. **[HW]**
- **App type must be set to Game** in Dev Home (View button -> View details). As an App the process gets about 1 GB
  and shared cores. As a Game the log reports a 5120 MB budget (see 3.1). **[HW]** for the Game budget; the App limits
  are from Microsoft's resource allocation docs **[assumed]**.

### 1.2 Build
- CI (`Cemu:.github/workflows/uwp_fork_ci.yml`, windows-2022, vcpkg with a UWP triplet) builds three things: the
  desktop renderer build, the Win32 host (the Xbox frontend as a normal exe) and the msix. A run takes about an hour.
  The Win32 host is the most useful one: it reproduces most frontend and renderer bugs on a PC with a debugger.
- `Cemu:tools/build-uwp-local.ps1` builds locally. The laptop (8 threads, 16 GB) is shared: use /m:4 or -j4.
- Every vcpkg dependency must build for the UWP triplet (no Win32-only APIs). In Cemu that removed Vulkan/OpenGL,
  cubeb, SDL, wxWidgets, DirectInput and Wiimotes from the UWP build. Eden will hit the same wall with SDL2 (input,
  audio), Qt, Vulkan loader, OpenGL, libusb, and anything using `GetModuleHandle`, `LoadLibrary` (use
  `LoadPackagedLibrary`), registry or `CreateFile` with non-app paths. **[PC]** (compile errors in 4a4fb3a, e1e8454)
- COM/WinRT initialization belongs to the UWP shell; library code must not call `CoInitializeEx` with a conflicting
  apartment (d91cf6f). **[HW]**

### 1.3 Deploy and debug workflow
- Device Portal REST API on `https://<console>:11443` (self-signed cert, basic auth). The swarm wraps it in
  `swarm/Tools/Xbox/xbox.py`: `status`, `apps`, `deploy <msix> <deps>`, `launch`, `kill`, `screenshot`, `home`, and
  `ls/get/put/mkdir` in an app's LocalAppData. Every call takes the `xbox` lease. **[HW]**
  - Install: POST msix + VCLibs as FormData to `/api/app/packagemanager/package?package=<name>` with the CSRF-Token
    cookie as `X-CSRF-Token`, poll `/api/app/packagemanager/state`. **[HW]**
  - Files: `/api/filesystem/apps/files|file|folder?knownfolderid=LocalAppData&packagefullname=<pkg>&path=\LocalState\..`
    (file GET needs `&filename=`). **[HW]**
  - If fetches fail with "Failed to fetch" the portal itself is stuck; only a Device Portal restart on the console
    helps. **[HW]**
  - From this lane: never deploy yourself, file QA tickets with `needs=xbox`.
- The log is the only debugger on the console. What made it work:
  - Create the log file at startup, keep `log_previous.txt` from the last session (b874f92). **[HW]**
  - A session marker file (`session_running.txt`) so the next start knows the last one crashed and shows a notice.
    **[HW]**
  - A crash handler that writes the exception, registers and a stack scan to the log; ignore a second crash while
    logging and only scan the stack when RSP is inside it (7cebf70). Publish PDBs with every build so addresses can be
    symbolized (3aa20b7). **[HW]**
  - Log memory use vs budget every 30 s while a game runs (f479fef). **[HW]**
  - A GPU watchdog thread: if submissions stop finishing, log after 2 s and every 5 s (c567bc4). Hangs used to leave
    no trace. **[HW]** (code runs; no logged hang since)
  - D3D12 DRED (auto-breadcrumbs + page fault) is available on the console, and device checkpoints logged before
    suspicious calls (588955b) found the pipeline library removal. **[HW]** (log "Device removed diagnostics (DRED)
    enabled", aeaba14)
- No PIX or debugger attach from the swarm; the owner takes PIX captures on request. **[assumed]** that PIX works
  against Dev Mode UWP; we have not done it.

## 2. CPU, memory and the sandbox

### 2.1 Executable memory and the JIT (`Cemu:src/util/MemMapper/MemMapperWin.cpp`)
- UWP apps must use `VirtualAllocFromApp`, `VirtualProtectFromApp`, `CreateFileMappingFromApp`,
  `MapViewOfFileFromApp`. `VirtualAllocFromApp` rejects executable protections at allocation time.
- What works on the console: allocate RW with `VirtualAllocFromApp`, then `VirtualProtectFromApp(...,
  PAGE_EXECUTE_READWRITE)`. Log line `MemMapper: using read/write/execute memory for generated code`. **[HW]**
  (aeaba14 log_packs.txt and crash_1020 log, 2026-10-20). So with `codeGeneration`, **RWX pages are allowed on Series X
  Dev Mode** - no W^X needed for Cemu.
- Fallback that exists but was never needed on the console: one section from `CreateFileMappingFromApp(...,
  PAGE_EXECUTE_READWRITE)` mapped twice, a RW view and a RX view, and the recompiler writes through one and runs the
  other (8ae59fc). **[PC]** only.
- Cemu's PPC recompiler emits x86-64 into one large pre-reserved region. It never frees or reprotects code pages often,
  so toggling cost was never measured. **[assumed]** that frequent `VirtualProtectFromApp` toggles (dynarmic's W^X mode)
  are slow.

### 2.2 Guest memory mapping
- Cemu reserves a 4 GB+ range for the Wii U address space (`Init Wii U memory space (base: 0x...)`) and commits
  regions with `VirtualAllocFromApp(MEM_COMMIT)`. **[HW]** (log, base changes each run).
- Cemu commits its guest RAM up front; we never used placeholders (`VirtualAlloc2FromApp` with
  `MEM_RESERVE_PLACEHOLDER`, `MapViewOfFile3FromApp`) or mapped the same backing memory at two addresses inside the
  guest range. **Not proven.** Eden's HostMemory/fastmem needs exactly that (see section 6).
- Committed memory counts against the 5120 MB budget whether touched or not. A lazy commit of emulated MEM2 was
  planned for CTGP/4K but never done. **[assumed]** (backed by the memory log: 1826 MB before the game starts, 35%).

### 2.3 Threads and stacks
- Default stacks are too small for the recompiler and shader translation. Cemu links with `/STACK:134217728` (128 MB
  reserve per thread, `Cemu:CMakeLists.txt`) and compile threads get explicit 256 MB stacks (89d3bb8, 637f204). Reserve
  only, so it doesn't count against the budget. **[HW]** (stack overflow crashes in shader translation stopped)
- The Game type gets dedicated cores; Cemu uses 6 pipeline compile threads plus the emulation threads (log: `D3D12: 6
  pipeline compile threads`). **[HW]** Exact core count and how many are shared with the system: **[assumed]** 7 usable.
- `HeapCompact` every 5 s while memory pressure is on returns freed CRT pages (0e2933c). **[assumed]** effective; shipped,
  not measured.

### 2.4 File access
- Installed package: read only. LocalState is `Q:/Users/UserMgr1/AppData/Local/Packages/<pfn>/LocalState` on the
  console. **[HW]** (log, mlc01 path)
- USB drives appear as `E:` (and further letters); with `broadFileSystemAccess` + `removableStorage` and the folder
  given `ALL APPLICATION PACKAGES` full control on an NTFS drive, plain Win32 file APIs on a typed path work and are
  fast. **[HW]** (games load from `E:/WiiU/DATA/USA/GAMES/...`, aeaba14)
- File pickers work on Windows; on the console the typed path is the reliable way. Picker folders go through the storage
  broker, reported slow on the console by another Cemu port. **[assumed]**
- Paths of 260+ characters need the `\\?\` prefix (backslashes only), both for raw file APIs and for `std::filesystem`;
  MK8's time trial ghost path crossed it (2f8110e). **[PC]**; the console path base (`Q:/Users/...`) is long, so
  assume it bites earlier there.
- `std::filesystem::relative` canonicalizes, which fails inside the sandbox on the console; use
  `lexically_relative` (51af729). **[HW]** (bundled packs stopped installing, fixed by this, 2026-10-18)
- Files written to the USB drive must be flushed: `CreateFile2` + `FlushFileBuffers` before close, otherwise copies were
  0 bytes after the app was closed (119e421). **[HW]** (2026-10-18)
- DLL loading: only packaged DLLs (`LoadPackagedLibrary`); `dxcompiler.dll`/`dxil.dll` ship in the package. **[HW]**

### 2.5 Input
- XInput works in the UWP app on the console for up to 4 pads (`XInputGetState`), including Elite/Series pads. **[HW]**
- Retry disconnected controllers periodically (8c8a027). **[HW]**
- Don't change the controller topology while a game runs; adding/removing controllers mid-game reportedly made the
  Xbox terminate the app (4d98399). **[assumed]** (report, never reproduced)
- The B button raises the system back action; handle `SystemNavigationManager::BackRequested` and mark it handled or
  the app goes to the background. **[HW]**
- Windows.Gaming.Input was not used; XInput was enough. Keyboard works on PC; on the console a USB keyboard is
  **[assumed]**.
- Pairing UI: "press A on the controller for player N" is how users bind physical pads to players. **[HW]**
- Hotkey for the in-game menu read straight from XInput (both sticks, or View+Menu held 1 s), independent of mapping.
  **[HW]**

### 2.6 Window, display and audio
- The CoreWindow is 1920x1080 view pixels regardless of the TV; render at the HDMI output resolution from
  `HdmiDisplayInformation::GetCurrentDisplayMode()` (`Cemu:src/gui/host/uwp/HostMainUWP.cpp`). **[HW]** (4K output)
- Presentation is paced at 60 Hz: launcher log `59.9 presents/s, Present blocks 16.2 ms`, DXGI frame statistics report
  0 Hz. Requesting a 120 Hz HDMI mode did not change the pacing in Dev Mode UWP. **[HW]** (aeaba14; 120 FPS abandoned)
- XAudio 2.8 is available; 5.1 output works (log `XAudio 2.8: available`). **[HW]**

## 3. D3D12 on Series X Dev Mode (as seen by a UWP app)

| Item | Value | Status |
|---|---|---|
| Adapter name | `SraKmd_arden` | **[HW]** aeaba14, crash_1020 (2026-10-20) |
| Device creation | `D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0)` succeeds | **[HW]** |
| Highest feature level | not logged by Cemu | **[assumed]** 12_1 hardware; Jadyx creates at 11_0 |
| Shader model | DXC/DXIL shaders run, so >= 6.0 | **[HW]** (log `Shader compiler: dxc`, MK8 runs); exact max **[assumed]** 6.4 (Jadyx) |
| Agility SDK | not available (no D3D12SDKVersion export honoured in UWP) | **[assumed]** (never tried; Jadyx reports the same) |
| Resource binding tier | 3 | **[HW]** indirect: Cemu logs a warning below tier 3 and no Xbox log has it |
| Pipeline libraries | `CreatePipelineLibrary` removes the device (DXGI_ERROR_DRIVER_INTERNAL_ERROR) | **[HW]** e56bc52 (2026-10-08) |
| Single PSO blobs | `GetCachedBlob` also removes the device | **[HW]** 465ce97 (2026-10-10) |
| Shader cache support flags | 0x63 (reported, but unusable as above) | **[HW]** log aeaba14 |
| Implicit shader cache | `SetCacheMode`/enable: E_INVALIDARG for all three modes | **[HW]** log aeaba14 |
| Memory budget (Game) | 5120 MB process budget | **[HW]** log `Host: memory 1826 MB of 5120 MB` |
| Format support | B5G6R5, B5G5R5A1, B4G4R4A4, D24S8 yes; UINT logic ops no | **[HW]** log aeaba14 |
| Readback memory | CPU-visible readback is not coherent unless the read is ranged | **[assumed]** fix 17aab5a shipped, user report pending |
| Presentation | 60 Hz | **[HW]** |
| Debug layer | not available | **[assumed]** |

Consequences we designed around:
- **No pipeline persistence.** Shader bytecode (DXIL from DXC) is cached on disk; pipelines are rebuilt every launch
  from a recorded list. Never call `CreatePipelineLibrary`, `GetCachedBlob` or `CachedPSO` on `SraKmd*`; detect by
  adapter name, the caps flags lie. **[HW]**
- **Some pipelines crash the driver at creation.** Cemu writes a pipeline hash to `<title>_blocked.txt` before a
  creation that never returned and skips it afterwards (f3209f9). MK8 has 3 such pipelines. **[HW]** (crash_1020)
- **Device reset.** After a removal, `D3D12CreateDevice` in the same process hands back the old removed device if any
  reference survived; the only recovery is restarting the app, so say so to the user (732a0a5). **[HW]** (BOTW at high
  memory, 2026-10-18)
- **Command list Close returning E_INVALIDARG** on a healthy device happened; dropping that list's work and going on
  beats terminating (83ea454). **[HW]** (crash report), cause unknown.
- **BC textures in 2D arrays sampled wrong on Series.** MK8's font is a 4-slice BC4 array; on the Xbox the wrong slice's
  glyphs appear, on PC (Intel) it's correct. Binding an array SRV to a `Texture2D` declaration reads slice 0 on AMD
  (fixed by slice copies, f59df71) **[HW]**; the remaining font bug (ticket #79) is still open, suspected array layer
  rounding of a float slice index (4df4df3 rounds explicitly, unverified). **[assumed]** cause. Eden will sample many
  BC/ASTC-decoded arrays: round layer indices explicitly and test arrays on the console early.

## 4. Renderer patterns worth carrying over (`Cemu:src/Cafe/HW/Latte/Renderer/D3D12`, README "Known risks")

- **Shader path.** Latte -> GLSL -> glslang -> SPIR-V -> binding remap -> SPIRV-Cross -> HLSL -> DXC (default; about
  twice as fast to start as FXC) or FXC. **[HW]** Stage linkage: D3D12 links VS outputs to PS inputs by register, not
  location, so the stage before the PS must declare exactly the PS inputs in order; spirv-opt dead code elimination
  breaks that. A spirv_to_dxil backend exists behind a build option but was never run on the console. **[PC]**
- **One root signature** for everything: per-stage CBV/SRV/UAV tables and sampler tables plus root constants for base
  vertex/instance and internal shader push constants. Descriptors are written per draw into a shader-visible ring heap
  (256K entries). Samplers deduplicated and cached because the shader-visible sampler heap is capped at 2048; when full,
  submit, wait, reset. **[HW]** Eden's bindless/descriptor-indexing style will want a different layout (section 6).
- **Pipeline cache.** `<title>_d3d12pipelines.bin` (device independent descriptions) + usage history (drop pipelines
  unused for 20 sessions, 49a1416) + per-level records keyed by the level's file open (`<title>_d3d12pipelines.scene.bin`,
  8915efa) + Quick start (startup set on the loading screen, rest in background, 29ebbb7). MK8: ~6270 pipelines, about
  150 s to build all on Series X. **[HW]**
- **Async compile with a capped wait.** Pipelines are created on worker threads; a draw waits at most 2 s, then skips.
  Skipping at once made MK8's 3D scene black for a whole race (a once-per-course draw was lost). "Smooth" mode never
  compiles on the render thread. Fewer compiles in flight near the memory limit (728b23d). **[HW]**
- **Barriers.** Legacy resource barriers with per-subresource state tracking (no enhanced barriers without the Agility
  SDK). **[HW]**
- **Upload ring / readback.** Upload ring with early submit when it overflows (e3dfc8c); readback through ranged `Map`
  (17aab5a). **[HW]** for the overflow fix.
- **Swapchain.** CoreWindow flip-model swap chain at the HDMI resolution, 2+ buffers, present paced at 60. Flip pacing
  experiment from danprice142 behind `flip_pacing.txt` (48b25a7). **[HW]** basic path; pacing experiment **[assumed]**.
- **Memory pressure.** Query the budget (`QueryVideoMemoryInfo` local segment) and process commit; at 80% release
  unused textures between draws, at 90% all unused ones; log a texture memory breakdown when pressure starts; on a
  failed allocation release textures unused for 3+ frames (cold and large first) and retry (fac23fd). **[HW]** for the
  thresholds (MK8 1440p stable); 4K on the largest tracks still OOMs. **[HW]**
- **Startup submissions** logged per submission with draw count and result: cheap and found early device removals.
  **[HW]**

## 5. Bugs, runtime quirks and perf traps (summary)

| Symptom | Cause | Fix | Status |
|---|---|---|---|
| Device removed at startup | pipeline library on SraKmd | never create one on Xbox | **[HW]** e56bc52 |
| Freeze on a new pipeline | driver compile on the render thread | async creation, 2 s cap, Smooth mode | **[HW]** |
| Same pipelines crash every run | driver bug on a few PSOs | blocklist file | **[HW]** f3209f9 |
| Minutes of loading each launch | no pipeline persistence | quick start, per-level preload, usage history | **[HW]** |
| 12 min first start with FXC | FXC slow, cache missing | DXC default, ship the bytecode cache | **[HW]** |
| Stack overflow in translation | default stacks | /STACK 128 MB, 256 MB compile threads | **[HW]** |
| Bundled packs not installed | `fs::relative` fails | `lexically_relative` | **[HW]** 51af729 |
| 0-byte files on USB | no flush before close | `FlushFileBuffers` | **[HW]** 119e421 |
| Time trial ghosts fail | 260+ char paths | `\\?\` prefix | **[PC]** 2f8110e |
| Wrong icons (slice 0) | array SRV as Texture2D on AMD | slice copies | **[HW]** f59df71 |
| Wrong font glyphs | unknown (layer rounding?) | open, ticket #79 | **[HW]** symptom |
| Relaunch fails after reset | old removed device returned | tell user to restart | **[HW]** 732a0a5 |
| App killed on B | system back | handle BackRequested | **[HW]** |
| OOM at 4K / CTGP | 5120 MB budget | eviction, retry; 1440p recommended | **[HW]** partly |
| No more than 60 FPS | Dev Mode presents at 60 Hz | none | **[HW]** |
| Slow or tiny resources as App | App type default | set Game in Dev Home | **[HW]** |

## 6. What Cemu never proved that Eden needs

These go into Step 3 (feasibility tests) unless the prior art already proves them on hardware:
1. **Placeholder memory**: `VirtualAlloc2FromApp(MEM_RESERVE | MEM_RESERVE_PLACEHOLDER)`, splitting placeholders, and
   `MapViewOfFile3FromApp` of a pagefile section into them - the basis of Eden's `HostMemory` / fastmem (one backing,
   two views: the guest address space and the backing). Jadyx reports these work but runs without fastmem.
2. **Fastmem**: a 64-bit guest address space reservation (Eden reserves large ranges) plus a vectored exception
   handler that catches access violations in JIT code. Whether `AddVectoredExceptionHandler` sees AVs from JIT code in
   the AppContainer on the console is unproven.
3. **W^X for dynarmic**: Cemu used RWX. dynarmic can run either way; measure RWX vs toggling, and whether RWX is
   still granted with dynarmic's allocation pattern (many smaller regions, `VirtualProtectFromApp` per block).
4. **Instruction cache flush** (`FlushInstructionCache`) - trivial on x86 but unexercised.
5. **D3D12 features Eden's Vulkan renderer relies on**: descriptor indexing / bindless (tier 3 resources, unbounded
   arrays at SM 6.x), transform feedback (stream output), BC1-7 and ASTC (no ASTC hardware: compute decode), MSAA
   resolves, 64-bit atomics, wave intrinsics, the actual SM ceiling (6.4 per Jadyx vs 6.6 needed for dynamic resources).
6. **spirv_to_dxil output on the console**: Cemu only ran SPIRV-Cross -> HLSL -> DXC there. Does `dxil.dll` signing
   and validation work in the package (Jadyx says yes)?
7. **Memory at Switch scale**: 4 GB (or 6/8 GB for some titles) guest RAM plus GPU memory inside 5120 MB. Cemu needed
   ~2 GB for a 2 GB guest. Lazy commit and paging are mandatory, not optional.
8. **Audio and input through Eden's frontends**: Eden uses SDL2 and cubeb; Cemu replaced both with XAudio2 and XInput.
