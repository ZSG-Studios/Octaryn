# Native and reconstructed RT timing matrix

72 serialized Windows DX12 runs on RX 9070 XT. Three repetitions per variant/workload. Build 10; later streaming and diagnostic changes require their own qualification.

median of run-level mean/median/p95/p99; worst is maximum across all runs. Capture-adjacent frames and 120 ready-frame warmup are excluded using the same observed frames for both variants. All groups pass exact observed camera equality and available captured sunlight equality.

| Output / mode / workload | Render size | Reference GPU mean ms | Adaptive GPU mean ms | Adaptive GPU p95 / p99 / worst ms | Adaptive CPU p99 / worst ms | GPU reduction |
| --- | --- | ---: | ---: | --- | --- | ---: |
| dx12-1440p-mode 0-static (native) | 2560x1440 | 28.246 | 16.198 | 18.968 / 19.051 / 19.151 | 19.296 / 20.711 | 42.65% |
| dx12-1440p-mode 0-motion (native) | 2560x1440 | 31.497 | 17.436 | 22.949 / 25.901 / 29.304 | 25.929 / 29.449 | 44.64% |
| dx12-1440p-mode1-static (native_temporal_AA) | 2560x1440 | 46.768 | 22.710 | 26.644 / 26.705 / 26.762 | 26.958 / 28.896 | 51.44% |
| dx12-1440p-mode1-motion (native_temporal_AA) | 2560x1440 | 45.417 | 22.408 | 27.750 / 28.453 / 31.954 | 28.822 / 32.043 | 50.66% |
| dx12-1440p-mode2-static (reconstructed) | 1707x960 | 23.749 | 11.470 | 13.340 / 13.402 / 13.658 | 13.666 / 50.603 | 51.70% |
| dx12-1440p-mode2-motion (reconstructed) | 1707x960 | 23.180 | 11.370 | 14.002 / 14.305 / 15.659 | 14.686 / 16.024 | 50.95% |
| dx12-4k-mode 0-static (native) | 3840x2160 | 59.657 | 34.215 | 40.226 / 40.342 / 40.386 | 40.608 / 58.000 | 42.65% |
| dx12-4k-mode 0-motion (native) | 3840x2160 | 65.248 | 36.529 | 46.893 / 54.054 / 62.071 | 54.151 / 63.737 | 44.02% |
| dx12-4k-mode1-static (native_temporal_AA) | 3840x2160 | 92.714 | 46.798 | 54.434 / 54.528 / 54.945 | 54.764 / 55.764 | 49.52% |
| dx12-4k-mode1-motion (native_temporal_AA) | 3840x2160 | 90.600 | 46.242 | 57.648 / 59.532 / 67.929 | 61.854 / 81.836 | 48.96% |
| dx12-4k-mode2-static (reconstructed) | 2560x1440 | 46.256 | 22.968 | 26.791 / 27.016 / 27.165 | 27.138 / 29.843 | 50.35% |
| dx12-4k-mode2-motion (reconstructed) | 2560x1440 | 45.039 | 22.744 | 28.005 / 28.924 / 32.239 | 29.039 / 64.281 | 49.50% |

Native mode 0 disables temporal reconstruction. Mode 1 is native-resolution temporal AA. Mode 2 uses pinned FSR 2.2.1 Quality and is reported separately. Generated frames are absent. These are short controlled workloads, not full release acceptance.

The native 1440p motion mean remains above 16.7ms, and tail times are higher. Reconstructed 1440p motion meets 16.7ms in this short timing sample, but this does not establish native performance or the complete visual/soak acceptance. Native 4K remains well above budget.

All adaptive gains exceed the conservative observed three-run mean spread. This is not a statistical significance claim. Per-phase distributions, memory, exact identities, each run and capture exclusions are preserved in `logs/client/performance-build10-full/matrix-summary.json` and the 72 isolated case directories.

The original reference sampling shares new compaction/history storage; it is not an untouched pre-change executable. Production-clock native-AA captures can have different startup-dependent jitter/ray phases. Their aggregate performance remains useful; pixel comparisons require the separate ready-frame fixed-sampling harness and are not FPS evidence.
