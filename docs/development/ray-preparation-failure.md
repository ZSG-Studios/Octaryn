# Manual Vulkan ray-preparation failure, 2026-09-18

The manual run `logs/client/manual-visual-20260918-051037.log` exited with code 1
at 729 columns / 3,773,863 quads, with `world_frame_failed stage=ray_prepare` in
the matching `.err.log`. It used Vulkan at 2560x1440 and radius 16. The last ray
sample had 706 ready columns, one pending job, 1,309 BLAS builds, 992 TLAS builds,
753,727,460 BLAS bytes and scene generation 1,309. `tlas_updates=0`: these were
full TLAS builds, not update-mode operations. Aggregate GPU bytes were
2,588,027,079. This log does not establish the individual failed operation.

## Added evidence on the real rendering path

`RayPrepareDiagnostics.h` and the WorldRay preparation/build/snapshot paths now
report failure-only records with phase, operation, last successful operation,
hexadecimal RHI result, frame, scene generation, column, face count, requested
resource bytes and observed/required fence values. Buffer descriptors, BLAS/TLAS
size queries, allocations, scratch, uploads, timestamp setup/results, shader
bindings, command completion and submission are distinguished. Start failures
also print resident/ready columns and the previous accounting sample.

The existing exact-value BLAS fence poll is preserved. The current pinned
Vulkan source at `build/dependencies/slang-rhi/src/vulkan/vk-device.cpp` maps
`VK_TIMEOUT` to `SLANG_E_TIME_OUT`; therefore ordinary Vulkan timeout handling
alone does not prove this manual failure's cause. No fatal is downgraded to a
successful frame, and RT shadows remain independent of disabled DDGI.

The per-column completed-pass-count map now erases unloaded/empty columns along
with their ready BLAS entry, restoring its intended residency-bounded lifetime.
That source-proven CPU bookkeeping leak is not claimed as the 729-column fatal.

## Candidates requiring a reproduction

- A start operation is worth examining because it previously returned false
  without the additional phase messages present in prepare/poll/snapshot. This
  inference depends on which source revision was in the failing package.
- Vulkan allocation-count pressure is distinct from VRAM exhaustion. The pinned
  RHI's `vk-buffer.cpp` allocates separate `VkDeviceMemory` per buffer, and BLAS
  storage uses those buffers. At 729 columns, four raster buffers per column
  plus roughly one ready BLAS per column already imply thousands of allocations,
  before textures, workspaces, staging and deferred retirement. The run did not
  record `maxMemoryAllocationCount` or live Vulkan allocation count; exhaustion
  is a candidate, not a proven cause or a repaired allocator.
- The capability log's `max_buffer_bytes=2147483648` limits individual buffers.
  Comparing it directly with aggregate GPU bytes is not evidence of overflow.
- Growing generation/build counters do not alone prove redundant TLAS work:
  completed replacement meshes also advance the generation. Same-generation
  snapshots already reuse the existing TLAS.

## CPU verification

`python tools/validation/validate_world_ray_poll.py` compiles the exact production
`WorldRayTracing::State::poll` body against explicitly CPU-only lifecycle doubles.
It passes unfinished-fence repetition, exact/later completion, exactly-once
publication, replacement/removal cancellation, retained in-flight resources,
fence failure, device-lost sentinel, timing failure and diagnostic-content checks.
Evidence: `logs/tools/world-ray-poll.json`.

Real-header `clang-cl /Zs` checks passed for WorldRayBuild.cpp,
WorldRaySnapshot.cpp and WorldRayTracing.cpp using the configured target's
defines/include paths; see `logs/tools/world-ray-syntax.log`.
No full native build or GPU execution was performed in this investigation slice.
The same radius-16 manual workload still needs reproduction with these diagnostics;
a smaller startup radius does not qualify this fatal as fixed.
