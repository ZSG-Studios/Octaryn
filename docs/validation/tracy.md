# Profiling

Native Tracy 0.13.1 instrumentation, frame CSVs, GPU timestamps and renderer
captures support focused performance investigations. Build configuration and
the selected backend determine available instrumentation.

Use CPU traces for generation, scheduling, session I/O and persistence; use
completed GPU timestamps for meshing, raster and temporal passes. Associate
samples with the actual submitted frame and record the tested build/settings.
Do not infer GPU utilization from CPU submission duration.

The client CSV and bounded benchmark commands are described in
[runtime runs](runtime-runs.md). Capture actual production work and separate
cold loading from settled frames and sustained movement. The current terrain
keeps full detail with no LOD; performance comparisons must retain matching
geometry/material/visibility behavior.

[Presentation performance](../development/presentation-performance.md) and
[voxel throughput](../development/voxel-throughput.md) include workload-specific
measurements and limits. Their numbers are not portable performance guarantees.

## Native Tracy tools

Use the upstream [Tracy 0.13.1 release](https://github.com/wolfpld/tracy/releases/tag/v0.13.1)
tools matching the pinned client instrumentation. The removed shell launcher was
coupled to obsolete Linux/container bootstrap code; native instrumentation and
`tools/profiling/summarize_client_runtime_perf.py` remain available.

With the upstream capture executable on PATH and an instrumented client running:

```sh
tracy-capture -a 127.0.0.1 -p 8086 -o logs/client/session.tracy -s 15
```

Create the output directory first. Use `tracy-capture.exe` on Windows if supplied
under that name. Close an attached profiler before connecting the CLI capture.
Open the resulting capture in the matching upstream profiler. The capture
arguments are defined by the [pinned capture tool](https://github.com/wolfpld/tracy/blob/v0.13.1/capture/src/capture.cpp).
These instructions do not claim a fresh capture or installed profiler.
