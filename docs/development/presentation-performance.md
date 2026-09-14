# Presentation performance qualification

The subsequent [terrain cache qualification](terrain-streaming-cache.md) halves
observed DX12 cold population time while preserving exact geometry. Results below
remain the earlier presentation comparison; they are not the latest loading time.

2026-09-13, Windows x64, RX 9070 XT. These are real packaged client/server runs
at 2560x1440, radius 32, seed 1337 / terrain revision 2. All 4,225 columns and
15,751,869 retained quads were resident with no pending streaming/meshing before
five seconds of warmup and a complete 15-second measurement. No LOD. PBR, POM,
clouds and sky were enabled; fog was disabled; two frames were in flight.

The camera was stationary. CPU frame statistics and GPU timestamp queries were
recorded separately and joined by actual frame/slot metadata. Graphics validation
was disabled for timing, after separate native-validation qualification. These
are individual bounded runs, not repeated statistical trials or gameplay-wide
performance guarantees.

## Matched DX12 submission comparison

All three rows have identical executable/content hashes, world, camera and
retained geometry. The two Off cases also have identical visible geometry:
1,723 columns / 6,178,621 quads. Quality's conservative jitter envelope can include
two additional boundary columns; it does not reduce retained mesh detail.

| Terrain submission / FSR | Average FPS | Reported 1% low FPS | Worst frame ms | Mean GPU ms | Terrain commands |
| --- | ---: | ---: | ---: | ---: | ---: |
| Per-column / Off | 181.95 | 148.15 | 28.452 | 4.539 | 1,723 |
| Batched / Off | 225.12 | 190.48 | 24.568 | 4.311 | 1 |
| Batched / Quality | 218.67 | 181.82 | 34.935 | 4.383 | 1 |

Batching improved average FPS by 23.73% in the matched Off comparison. Enabling
Quality cost about 0.360 ms for FSR and did not improve average FPS here. This
scene spends most GPU time drawing millions of terrain quads; lowering render
resolution does not eliminate that geometry. Off remains the default.

Evidence under `logs/client/validation/presentation-performance`:

- `presentation-dx12-off-off-fu9fr9qw`
- `presentation-dx12-off-required-jkk0a0qb`
- `presentation-dx12-quality-required-20uemc40`

Client SHA-256: `10155fa522bec048231109813a929dc09259b21265af38f6b42777f4e71eb567`.
These matched measurements preceded the Vulkan clear-usage and varying-interface
cleanup. Copy-destination usage does not alter DX12 resource creation flags in
the pinned RHI; the interface cleanup removed unused values without changing
surface shading. The current-build DX12 confirmation below independently checks
that this result remains representative.

## Final native-validated build

These three rows use the same final package after the Vulkan corrections. Do not
treat them as the same executable snapshot as the preceding comparison group.

| Backend / FSR, batched | Average FPS | Reported 1% low FPS | Worst frame ms | Mean GPU ms |
| --- | ---: | ---: | ---: | ---: |
| DX12 / Off | 225.38 | 190.48 | 6.235 | 4.316 |
| Vulkan / Off | 186.39 | 153.85 | 80.676 | 5.072 |
| Vulkan / Quality | 183.32 | 160.00 | 62.733 | 5.182 |

Vulkan Quality spends about 0.381 ms in FSR. Both Vulkan cases retain frame-time
outliers despite much lower maximum GPU query durations (6.62 / 6.08 ms). These
measurements do not identify the cause of the CPU/presentation outliers. Do not
claim hitch-free Vulkan gameplay or attribute the spikes to streaming: the world
was fully resident, and measured mesh/upload work was zero.

Evidence under `logs/client/validation/presentation-performance-native-final`:

- `presentation-dx12-off-required-59r_2op9`
- `presentation-vulkan-off-required-gxyuvzd7`
- `presentation-vulkan-quality-required-tlk3fvgu`

Client SHA-256: `d10221d336cc97eff4eb96b45d3a635aed7f92bf7f0ea581809190dfd38192a9`.
Each result records all executable/content hashes, settings, frame counts,
timings and geometry. `logs/client/presentation-performance-comparison.json`
collects the six rows and verifies comparisons within each hash group.

## Practical limits and reproduction

Cold startup, world population, warmup and measurement together took roughly
112–116 seconds per run. This is not proof of fast cold streaming, high-speed
travel, edit-heavy workloads, distant-world memory bounds, or other GPUs/OSes.
Those require their own moving-camera and workload measurements. The clear
verified gain in this pass is DX12 submission batching; FSR is an optional image
reconstruction feature, not a guaranteed speed increase.

```powershell
python tools/validation/benchmark_presentation.py `
  --client-bundle-root build/release-windows/client/bundle `
  --evidence-root logs/client/validation/presentation-performance `
  --backend dx12 --upscaler off --batch required --seconds 15
```

Select `--batch off` for the submission baseline, `--backend vulkan` for Vulkan,
or `--upscaler quality` for FSR. Run one GPU case at a time and keep the package
unchanged during a comparison. The runner rejects partial residency, incomplete
measurement intervals, malformed profiling or changed package hashes.
