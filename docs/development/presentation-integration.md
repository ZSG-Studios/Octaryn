# Inventory, world items and temporal presentation

This pass repairs the restored engine in place. First-party rendering remains
Slang through standalone Slang RHI. No LOD, Godot engine dependency, alternate
renderer, or wholesale engine rewrite was introduced.

## Player-facing changes

- The inventory uses a 10-by-5 upper-left grid and integrated hotbar, original
  blue/gold pixel panels, counted cursor icons and tooltips. Drag, click, merge,
  swap and single-item right-click interactions use the actual RmlUi document.
- T tosses one item; Ctrl+T tosses a whole stack. A 999-item creative stack becomes
  16 authoritative world entities of at most 64 items. Gravity, collision,
  compatible-stack merging, delayed pickup and expiry run on the server.
- The client persists a reservation before sending a toss, and persists pickup
  credit before acknowledgement. Restart/replay, save contention and bounded
  capacity paths preserve counts. Existing invalid inventory history fails
  clearly instead of resetting transaction watermarks.
- Display settings expose Off, Native AA, Quality, Balanced, Performance and
  Ultra Performance for the actual pinned FSR 2.2.1 implementation.

The catalog still supplies unlimited creative building blocks. Armor, survival
loot tables and consumable placement are not implemented by this change. Full
inventory pickups remain server-owned pending grants until capacity is available.
See [inventory-ui.md](inventory-ui.md) and [world-items.md](world-items.md).

## Rendering and performance changes

The Godot reference and AMD algorithm pins, vendor hashes, licenses, SDK callback
adapter and shader changes are documented in [fsr2-integration.md](fsr2-integration.md).
The production pipeline supplies depth, jitter-free camera/object motion, reactive
masks, previous successfully submitted transforms, jitter and history resets.
Terrain, fluids, player and item rendering use the matching scene resolution;
RmlUi remains at native display resolution after reconstruction.

The existing sky was authored in display-linear color and inverse-tone-mapped
into HDR, producing a blue value near 999. Reconstructing that artificial
radiance caused blue silhouettes. The existing temporal-input compute now
tone-maps each pixel once after calculating its HDR reactive mask; FSR uses its
real LDR permutation, then presentation applies only the sRGB transfer. The
original HDR lighting before this conversion and the Off path remain intact.
There is no added full-screen pass or input texture. The adapter's scene-HDR
mode is separately supported and tested; the game's current SDR pipeline does
not claim HDR display output.

FSR also applies the required texture mip bias, handles rounded render extents,
and conservatively culls against the actual jittered raster footprint. Item and
player shaders subtract the camera before adding fractional local geometry to
retain precision at large world coordinates.

The pinned DX12 RHI already implemented indirect multi-draw but did not expose
the necessary capabilities. A narrow dependency patch now reports them after
command-signature creation, with actual shader-model/hardware gates. Production
terrain can use the existing bindless batching path on this DX12 device. Exact
mesh/material/culling behavior is retained; unsupported hardware still follows
the existing explicitly reported capability path.

The actual long-distance profiling attempt exposed another DX12 integration
bug: RHI signals the caller's frame fence before its internal query-tracking
fence. Treating a briefly pending query as failure could stop the client after
hundreds of frames. The profiler now uses the documented `getResult` wait for
that query submission; reset/error states still fail with a precise diagnostic.
No queue-wide wait or disabled profiling was substituted. Explicit benchmark
measurement also bypasses a second statistics warmup, with an independent check
that accumulated frame time covers the entire requested interval.

Explicitly loading Khronos Core + Synchronization validation exposed two images
that were initially cleared without transfer-destination usage. World color and
the HDR scene now declare `CopyDestination`. Opaque/sprite vertex interfaces also
exclude the fluid-only varyings; forward pipelines select a separate entry with
the complete interface. This removes native Vulkan errors and unused-output
warnings without discarding diagnostics or changing surface shading.

At 1440p/radius 32 with 4,225 fully resident columns and no LOD, matched DX12
batching raised average FPS from 181.95 to 225.12. The final-build confirmation
measured 225.38 FPS. FSR Quality was slightly slower than Off on both measured
APIs in this geometry-heavy scene. Frame-time outliers and cold/moving streaming
remain separate concerns. See [presentation-performance.md](presentation-performance.md)
for exact configurations, hashes, timings, retained evidence and limitations.

## Qualification

Windows x64, AMD Radeon RX 9070 XT, Slang 2026.17.1:

- Native item authority: 101 checks; client item persistence/replay: 20;
  inventory model/persistence: 71; settings and JSON roundtrip: 41.
- Actual managed server process tests pass toss/replay/save retry/restart,
  pickup grant and ordered acknowledgement alongside existing fluid, player
  clock and publication regressions.
- DX12 and Vulkan exact batched topology, replacement, indirect base/instance
  addressing and seam tests pass. Two-frame GPU sentinels, profile association
  and ordered drain pass for 257 submissions on both APIs with required validation.
- Actual FSR SDK execution passes both APIs, including HDR constant signals,
  temporal history, reset, tiny-context rejection, production motion/mask inputs
  and a high-contrast oracle reproducing the original fringe. All 78 shader
  permutations compile (26 SPIR-V, 26 DXIL, 26 Metal source).
- The complete first-party shader entry graph also passes 153 compiler cases:
  51 per target. Per-entry artifact, source and compiler hashes are under
  `logs/client/firstparty-platform`. This caught and fixed an
  unsupported Metal item-buffer dimension query and preserved absolute indirect
  raster-probe indices across the targets.
- Packaged temporal qualification passes all six modes and nine mode/resize
  phases on DX12 and Vulkan, each with one and two frames in flight. It verifies
  submitted resets, render/output dimensions, native-resolution UI and capture.
- Packaged whole-stack toss/render/pickup passes both APIs with Quality: 999
  debited, 16 entities, 16 ordered grants, 999 restored, server acknowledgement
  16 and no remaining world entities/grants. The one-item DX12 Off case passes.
- Seven packaged DX12 Quality UI surfaces each pass 600 resident frames at
  81 columns, including the 640x480 compact layout. Current document checks cover
  four viewports and all 13 display controls; inventory checks exercise all 50
  slots and first/last creative entries. The actual GPU screenshots were inspected; image existence and
  validation-layer success alone were not accepted as visual proof.

Evidence is under `logs/client/validation/presentation-final`; the FSR fringe
capture/oracle is under `logs/client/validation/fsr-edge-fixed` and
`logs/client/fsr2-fringe-comparison.json`. Build and bounded regression logs use
`build/ui-items-*.log`. Every domain runner uses an isolated world and settings,
and runs without injected OS input. `validate_temporal.py`,
`validate_world_items.py --whole-stack`, and `validate_rml_ui.py` reproduce the
packaged cases against `build/release-windows/client/bundle`.

The final native-validation closure is under
`logs/client/validation/presentation-native-final`, using client SHA-256
`d10221d336cc97eff4eb96b45d3a635aed7f92bf7f0ea581809190dfd38192a9`:

- `world-items-sd7eae2o`: Vulkan Quality whole-stack conservation and capture.
- `world-items-eand36ct`: DX12 Quality whole-stack conservation and capture.
- `temporal-3_vvx4yf` / `temporal-dglu3ifu`: Vulkan two/one-frame mode and resize
  qualification, with Khronos Core + Synchronization explicitly loaded.
- `temporal-bjs6zjjw`: DX12 two-frame mode and resize qualification.
- `build/presentation-native-final-batch-{vulkan,dx12}.log`: exact batch topology
  and 256 seam views on each API; no reported warnings or errors.
- `ui/rml-ggfpx6yx/inventory`: final 640x480 Vulkan Quality inventory, 600
  resident frames with native validation; captured compact layout inspected.

The earlier packaged Vulkan runs enabled RHI validation but did not explicitly
load the native layer. They are functional history, not the final native-layer
proof. Failed native runs are retained as evidence of the defects above.

The subsequent Terraria screenshot comparison and coordinated visual refinements
are documented in [terraria-ui-comparison.md](terraria-ui-comparison.md). They
preserve working inventory behavior and identify the unimplemented Terraria
systems explicitly rather than presenting decorative slots as implemented gear.

Metal shader-source emission does not prove Apple's compiler, GPU atomics,
native linking or runtime execution. Native Linux/macOS dependency integration
is present but remains unqualified on those operating systems. Linux currently
requires X11/XWayland for the pinned RHI surface bridge. See
[slang-rhi-native-platforms.md](slang-rhi-native-platforms.md).
