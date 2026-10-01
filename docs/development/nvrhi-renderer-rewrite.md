# Octaryn NVRHI renderer rewrite

Status: active implementation plan, approved direction, saved 2026-09-30.
Repository: https://github.com/ZSG-Studios/Octaryn.

This file is the persistent checklist until the new renderer is implemented and
qualified. Update it after each verified slice with evidence and remaining work.
Do not mark the rewrite complete from compilation, isolated probes, or small
scene captures alone. Current execution still uses standalone slang-rhi;
NVRHI implementation has not started. Existing shared-scene work remains subject
to its own verification and does not establish complete large-scene rendering.

## Locked direction

- NVRHI becomes the sole GPU execution interface; retain Slang as the shader
  language/compiler and retain Octaryn's custom virtual geometry algorithms.
- DX12 is the Windows default. Qualify Vulkan separately, then native Linux.
  macOS/Metal is deferred; DX11 cannot meet the renderer's required feature set.
- Initial NVRHI pin: `d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076`.
  Keep the existing Slang SDK initially. All external and transitive pins belong
  in `cmake/Dependencies/DependencyRegistry.cmake`.
- Keep RTXMU and NVAPI disabled initially. Standard DX12/Vulkan ray tracing and
  engine-controlled bounded allocation remain the cross-vendor foundation.
- Preserve the 512 MiB scene budget, complete source coverage, original instances,
  authored materials, exact independent collision and server authority.
- Keep the owner layout and the hard 500-line source/code file limit.
- Preserve existing source changes and qualification artifacts. Do not invent a
  slang-rhi compatibility wrapper, indexed opaque fallback, or second renderer.

## Change, swap and removal inventory

| System | Action |
| --- | --- |
| Graphics dependency | Replace slang-rhi with pinned NVRHI; retain Slang compiler SDK. |
| Device/presentation | Rewrite device, queue, surface and swapchain setup; retain SDL window/input. Native calls stay inside focused bootstrap/backend owners. |
| Shader creation | Replace device-owned Slang sessions and linked-component programs with independently compiled DXIL/SPIR-V artifacts and binding metadata. |
| Bindings | Remove ShaderCursor/IShaderObject; use explicit layouts, cached binding sets and descriptor tables. |
| Material ABI | Replace Slang/RHI handles with engine descriptor indices; change raster, forward and ray consumers together. |
| Frame lifecycle | Replace encoders, command buffers and fence plumbing with NVRHI command lists and engine submission tokens. |
| Resource creation | Replace buffers, textures, samplers, views, framebuffers, pipelines and queries throughout active GPU owners. |
| Virtual geometry | Retain format/cook/hierarchy/residency/selection policy; rewrite uploads, shared selection, feedback, hybrid visibility and batched raster. |
| Ray tracing | Rewrite BLAS/TLAS allocation, builds, compaction and GPU publication; retain complete cuts and immutable snapshots. |
| Lighting/presentation | Port shadows, reflections, HDR, sky/clouds, Hi-Z and temporal passes without redesigning their algorithms. |
| FSR | Replace its custom GPU backend; preserve pinned FSR 2.2.1 and existing adaptations. |
| UI/dynamic content | Port RmlUi clipping/layers/filters, item rendering and GPU morph/skinning; preserve UI audio and gameplay contracts. |
| Asset preparation | Retain PreparedMapAsset and CPU import/cooking; replace GPU-facing MapAssetBuild and remove duplicate loader/upload paths. |
| World/saves | Preserve world/save IDs, source fingerprints and preparation seals; renderer cache invalidation is separate. |
| Qualification tools | Port GPU probes/captures/timing/retirement diagnostics; replace dependency-specific tests without dropping behavioral coverage. |

After verified cutover, remove slang-rhi linkage, bootstrap/receipt modules and
its 18-patch stack. Extract independent Slang SDK acquisition before removal.
Remove obsolete backend status fields and settings for removed render paths.

Trace callers before removing indexed/dense-meshlet opaque map pipelines,
uploads and old LOD staging. Preserve BLEND and dynamic item geometry.
Remove dormant voxel-column ray allocators, procedural jobs/traversal and stale
column diagnostics after extracting live map/item functionality. Shared surface,
material and GI shaders still have live includes: do not delete whole directories
by their names. Do not remove backup bundles or imported source assets.

## Interfaces and resource contracts

### Shader artifacts and bindings

Own Slang compiler sessions independently from NVRHI devices. Emit target
bytecode, entry/stage/profile identity, dependency hashes and binding metadata.
Compile/package known shaders before gameplay; selected content preparation and
pipeline warm-up report actual progress through the loading screen.

Use explicit stable register spaces/descriptor sets, validated against compiler
reflection. Replace runtime name lookups with declared layouts. Cache immutable
pipelines/bindings by artifact, layout and target formats.

Replace each 64-bit opaque material handle with an engine-owned uint32 descriptor
index plus reserved field; preserve the initial 304-byte material record footprint.
Ray buffer references use descriptor index plus byte offset, preserving the initial
160-byte ray record footprint. Version these CPU/Slang interfaces together and
validate scalar layout, stride, nonzero slices and packed page offsets on both
targets. Descriptor index zero has a defined safe default resource.

Descriptor tables do not provide automatic lifetime/state tracking. Every live
scene/ray snapshot owns its buffers, images, samplers and descriptor allocations.
Never rewrite/reuse a descriptor slot until all its consumers finish.

### Submission and lifetime

Introduce `GpuSubmission { queue, serial }`, with nonblocking completion polling
and bounded waits. Replace fence/value arguments in MapAssetBuild, page reuse,
feedback tickets, scene publication, raster banks and ray snapshots. Preserve two
fenced frame slots and existing watchdog deadlines.

Start with one ordered graphics queue. The submission order is page upload,
selection/expansion, BLAS build, compact-size readback, compact copy, complete
TLAS, lighting, presentation and completed-consumer retirement. Independent
asynchronous queues require measured benefit and explicit cross-queue tests.

Preserve recorded uploads on abandoned resize/minimized frames. Commit temporal
history and scene revisions only with successful submissions. Run NVRHI garbage
collection every frame and during loading/unload. Dropping an application owner
does not release its memory lease while backend references remain.

### Budget and admission

Retain SceneMemoryLedger and parent-or-all-children transactions. Charge actual
physical heap capacity once, including pending/compacting/retired allocations.
Use shared aligned virtual-AS heap slices instead of one committed heap per root.
Reserve uncompacted sources plus compact destinations until copies and old
consumers complete. Keep upload/readback allocation bounded and accounted.

Publish a candidate only after whole-cut raster/ray admission. Retain the previous
complete cut when finer geometry cannot fit; do not raise error thresholds or
budget defaults to disguise the rejection. Failed size queries are admission
failures, not zero-byte resources. Report requested and achieved error separately.

Pass a versioned renderer admission profile into CPU world preparation. Source
and cook seals certify content validity; device-specific AS/result/scratch queries
and texture allocations certify runtime admission. Preserve metadata-only menu
startup and defer GLB/glTF payload work until world/save selection.

### Focused NVRHI extensions

Implement extensions inside pinned NVRHI DX12/Vulkan backends and its validation
layer, with a deterministic patch receipt and no scene/pass-level native bypass:

- Explicit root Raw/Structured UAV layout option for DX12 visibility atomics;
  Vulkan retains the equivalent storage-buffer binding. Include range checks,
  strong resource references, layout hashing and compute/graphics state support.
- Allocation-free BLAS/TLAS result/build/update-scratch/alignment queries.
- Caller-owned aligned AS scratch ranges and bounded scheduler reuse.
- Asynchronous compact-size query/readback and explicit Compact copy.
- Explicitly sized compact destinations and allocation/reclamation telemetry.

Initially use production fixed-count indirect mesh dispatch. Upstream count-buffer
mesh dispatch is unsupported on DX12; expose that honestly. Keep existing count
tests as explicit supported/unsupported contract tests rather than CPU-readback
emulation. Root-UAV atomics must preserve the current hardware capability contract.

## Persistent implementation checklist

- [x] Lock NVRHI + Slang direction and DX12/Vulkan-first platform scope.
- [x] Audit GPU owners, CPU preparation seams, binding/lifetime and memory gaps.
- [x] Save the plan and make it the active renderer direction.
- [ ] Record reproducible pre-migration builds, fixtures, image and timing controls.
- [ ] Pin/build NVRHI and independent Slang shader tooling.
- [ ] Implement device/swapchain bootstrap, validation and submission tracking.
- [ ] Qualify root-UAV, AS query/scratch/compaction and accounting extensions.
- [ ] Port shared pages, selection, feedback and GPU residency lifetimes.
- [ ] Port hybrid visibility/material resolve and global batched raster.
- [ ] Port bounded ray builds, TLAS publication, shadows and reflections.
- [ ] Port HDR, sky/clouds, Hi-Z, temporal presentation and FSR 2.2.1.
- [ ] Port RmlUi, items and GPU morph/skinning.
- [ ] Integrate selected asset loading, cancellation, save/open and retirement.
- [ ] Remove old active backend/code/dependency paths and update documentation.
- [ ] Pass octaryn_all and DX12 runtime/capture/authority checks.
- [ ] Pass Vulkan independently on Windows; qualify native Linux separately.
- [ ] Qualify a source-complete large-scene hierarchy and actual full-world runtime.
- [ ] Record performance against the existing AAA workload targets separately.
- [ ] Mark complete only when all required cutover and qualification gates pass.

Parallel ownership: coordinator owns shared interfaces/build/integration;
virtual-geometry agent owns pages/selection/raster; ray agent owns AS/memory;
presentation agent owns UI/temporal/effects. Coordinate changes to shared ABI and
serialize native builds and GPU qualification.

## Acceptance and evidence

Require octaryn_all, no active slang-rhi linkage, and NVRHI/native API validation.
Retain matched Bistro image and timing controls across the backend cutover.
Test uint64 visibility atomics, indirect offsets, packed roots, stale generations,
feedback, physical heap accounting, scratch reuse and deferred retirement.
Compare actual material/G-buffer images for OPAQUE/MASK/BLEND, compact/authored
vertices, mirrors/shear and nonzero material offsets. Inspect reflections and
shadows with complete offscreen geometry while publication changes.

Exercise cold/warm opening, movement, resize/minimize, cancellation, unload,
repeated world switching and independent save/reopen. Verify stable authoritative
pose and networking listen/connect when networking behavior changes. Runtime
tests remain hidden, frame-capped and watchdog-supervised with no input injection;
do not weaken the 50 ms sustained-frame or 2 s heartbeat-stall guards.

Qualify a prepared large scene that actually opens within the existing 512 MiB
scene budget, preserves complete source coverage, original instances and authored
materials, and demonstrates movement, authoritative collision and reflection
publication with inspected captures. Retain complete offscreen ray coverage
during streaming. Backend migration alone does not establish hierarchy
completeness or prove 1 px error.
The native 4K/240 FPS and dynamic-content workload remains a separate measured
performance target; neither a clean build nor a small fixture proves it.

## Sources

- [NVRHI repository](https://github.com/NVIDIA-RTX/NVRHI)
- [Pinned public API](https://github.com/NVIDIA-RTX/NVRHI/blob/d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076/include/nvrhi/nvrhi.h)
- [Programming guide](https://github.com/NVIDIA-RTX/NVRHI/blob/d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076/doc/ProgrammingGuide.md)
- [Integration tutorial](https://github.com/NVIDIA-RTX/NVRHI/blob/d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076/doc/Tutorial.md)
- [Memory queries](https://github.com/NVIDIA-RTX/NVRHI/blob/d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076/doc/memory-queries.md)
- [DX12 binding implementation](https://github.com/NVIDIA-RTX/NVRHI/blob/d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076/src/d3d12/d3d12-resource-bindings.cpp)
- [SM 6.6 atomics](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Int64_and_Float_Atomics.html)

## Progress log

- 2026-09-30: saved approved direction/checklist and corrected repository identity
  to Octaryn. Publishing the existing changes does not complete the rewrite.
