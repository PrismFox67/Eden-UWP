# Per-draw CPU cost (PC baseline, #190 item 4)

2026-10-22, local build b931e749db + CI `draw_stress.nro` (run 39838656298), installed on the dev laptop
(Surface, 8 threads, Intel iGPU, `igc64.dll`) as a registered loose layout. Package:
`package-appx.ps1 -BootNro draw_stress.nro -RunSeconds 70 -Profile on`. Report:
`tools/xbox/profile-report.py LocalState/jadyx/log/jadyx_profile.txt` (4 windows, 71 s at 250 Hz,
profiler cost 4.1% of a core).

## Workload

`tools/xbox/draw_stress`: guest OpenGL (Mesa nouveau, inside the guest) issues 4000 `glUniform4f x2 +
glDrawArrays` per frame. Each draw reaches Eden as a Maxwell3D draw with a constant-buffer update.

## Numbers

Guest frame rate: median 32-33 fps (range 21-42 after warm-up) = ~130 k draws/s.

| Thread | CPU % of a core | ms/s | per draw |
| --- | --- | --- | --- |
| HostTiming | 43.0% | 430 | (busy-wait, not per draw) |
| CPUCore_0 (guest, JIT) | 36.7% | 367 | ~2.8 us (guest Mesa driver) |
| GPU (Maxwell3D -> D3D12 draw setup) | 8.4% | 84 | **~0.64 us** |
| D3D12Recorder | 7.3% | 73 | **~0.56 us** |
| D3D12Present | 0.9% | 9 | - |

GPU-thread phase breakdown: draw setup (rasterizer) 31%, GPU command processing (DmaPusher,
Maxwell3D) 24%, pipelines/shaders 12% (`GraphicsPipeline::Configure`, `FixedPipelineState::Refresh`),
buffer cache/uploads 11%, texture cache 9%, guest memory access 7%, descriptors 2%, queries 2%.
Hottest own-time functions: ntdll 8%, `DmaPusher::ProcessCommands` 4.5%, `GraphicsPipeline::Configure`
4.5%, `DmaPusher::CallMethod` 4%, `RasterizerD3D12::Draw` 3.6%, `FixedPipelineState::Refresh` 3.4%.

## Findings

1. **The host draw path is not the limit here.** GPU + recorder threads together cost ~1.2 us per
   draw and neither is above 9% of a core. No thread is >= 85% busy: the frame waits between threads
   (guest idles 17% of samples in `PhysicalCore::Idle`), i.e. it is latency-bound (guest waits on
   fences/vsync), not throughput-bound.
2. **HostTiming burns 43% of a core spinning** (`Common::CpuWaitForBits` 70%, `WallClock::GetUptime`
   28%). On the Xbox (fewer usable cores, game OS) this steals a core from the JIT and GPU threads.
   Candidate quick win: a coarser wait (`PreciseTimer::Sleep` / waitable timer) when the next event is
   >~1 ms away; measure on the console. `timer_profile=1` exists for this.
3. Per-draw guest cost (~2.8 us of JIT time per draw for Mesa) dominates host cost ~2:1; real games
   with NVN are leaner, so the host share will be larger there. Re-measure tonight on real games.
4. A deferred recording thread would save at most the ~0.6 us/draw now on the GPU thread, and the
   recorder already runs separately (`D3D12Recorder`). **Not recommended yet**; revisit only if the
   console or real games show the GPU thread >= 85%.

## Caveats

PC numbers (x86 laptop, iGPU), not the Xbox. One synthetic workload. `profile=full` (per-draw
timers) not used, so phase shares are from sampling only.

## Next

- Same run on the Xbox via QA (needs=xbox) for the console baseline.
- Real games tonight (owner's dumps): same report, compare GPU-thread us/draw.
- Try the HostTiming wait change behind a boot.cfg switch and A/B.
