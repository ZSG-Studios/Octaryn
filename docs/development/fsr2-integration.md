# FSR 2.2.1 Slang RHI integration

2026-09-13. The native FSR adapter and headless temporal reconstruction execute
on Vulkan and Direct3D 12 through standalone Slang RHI. This record qualifies
that adapter and its production temporal-input shader, not the complete game or
Metal runtime.

The subsequent [FSR and moving-delivery pass](fsr-streaming.md) records opaque
object mask corrections, motion-sentinel rounding, dynamic-size GPU oracles and
packaged visual verification. Use that report for the latest integration fixes.

## Sources and ownership

- Godot: `2f698aa5fe31d0be68f205ec41aec9365081d364`, including its patched FSR
  CPU API, build/options/storage-format patches and MIT notices.
- AMD FSR 2.2.1: `1680d1edd5c034f88ebbbb793d8b88f8842cf804`, matching HLSL
  shader implementations used as the Slang translation source.
- [Godot source](https://github.com/godotengine/godot/tree/2f698aa5fe31d0be68f205ec41aec9365081d364/thirdparty/amd-fsr2)
  and [AMD source](https://github.com/GPUOpen-Effects/FidelityFX-FSR2/tree/1680d1edd5c034f88ebbbb793d8b88f8842cf804/src/ffx-fsr2-api).

`tools/build/acquire_fsr2.py` obtains the pinned files, checks their Git blob
hashes, records SHA256 provenance and rejects conflicting existing downloads.
Immutable third-party code remains in `build/dependencies`; its original file
lengths are preserved. First-party adapters remain below 500 lines per file.

`tools/build/prepare_fsr2_shaders.py` produces a separate checked derivative.
Every contextual patch requires exactly one matching source occurrence. Its
37-file manifest covers shader headers, AMD/Godot notices and provenance.
Packaged `Client/Shaders/Fsr2/Vendor` must match that verified manifest exactly.
The shader validators permit only this narrowly identified vendor material;
first-party shaders remain Slang, and no GLSL runtime path is introduced.

`Rendering/Fsr2` owns SDK callbacks, resource views, pipelines, GPU job ordering
and history allocation. `Shaders/Fsr2` owns Slang entry wrappers.
`Rendering/Temporal` separately owns Octaryn's camera/object motion and reactive
inputs. The SDK schedules reconstruction, depth clipping, locks, accumulation,
luminance and optional RCAS; this is not a spatial approximation.

## Scene presentation domain

Octaryn keeps HDR lighting and forward composition, then applies its existing
linear Reinhard map `max(color,0)/(1+max(color,0))` before reconstruction. The
actual FSR2 LDR permutation consumes that tone-mapped linear image. Presentation
applies only the original sRGB transfer to reconstructed output; UI follows at
display resolution. FSR Off retains its original tone-map/transfer path.

This domain choice is necessary for the original display-authored sky. Same-frame
readback showed the old inverse tone map producing horizon RGB approximately
`(2.5,5.5,999)` beside terrain near `(0.02,0.03,0.01)`. A tiny linear HDR mixture
then becomes visibly blue after the component-wise tone map. The original Off
path hid this interaction because it did not reconstruct the silhouette.
This is not negative HDR, a backend synchronization defect or an arbitrary color
clamp. The adapter's HDR mode remains supported and independently qualified.

Conversion is fused into `Temporal/Inputs.slang`: each thread computes its
reactive value from its own original HDR pixel, then overwrites only that pixel
through a UAV. Other threads do not read it. FSR subsequently reads the result
as an SRV. This adds no full-screen pass or allocation.

`OCTARYN_CLIENT_CAPTURE_TEMPORAL=1` with an ordinary requested frame capture
enables exact same-frame scene/output/depth/motion/reactive/object/opaque raw
readback and JSON row pitches/formats. Metadata labels the tone-mapped scene and
output separately from the earlier opaque HDR snapshot. Normal rendering has
no capture readback cost.

## Audited shader adaptations

The derivative carries Godot's padded reprojection matrix and invalid-motion
fallback, reactive-mask cap of 0.9, R32F dilated depth and RGBA16F prepared color.
Octaryn supplies finite, non-reversed 0..1 depth and valid pixel motion; it does
not copy Godot's reverse-Z flags. FP32 and group-shared SPD avoid requiring a
particular hardware wave size.

Slang reflection replaces fixed D3D register annotations. Normalization comes
from exact texture-view formats rather than HLSL `unorm`/`snorm` type modifiers,
which otherwise fail Slang's DXIL sampling emission. Accumulation and sharpened
accumulation use distinct virtual module paths as well as distinct module names.

Two explicit edge-load derivatives make border accesses defined:

- RCAS cross taps clamp to the display bounds.
- In AMD `ffx_fsr2_upsample.h`, the already-offset sample position passed to
  `ClampLoad` had a zero offset argument. `ffx_fsr2_common.h` only clamps axes
  with nonzero offsets, so this call did not clamp. Out-of-bounds zero loads
  polluted the rectification box even though color weights excluded those taps.
  Replace that call with integer `clamp(position, 0, RenderSize()-1)`.

The second issue reproduced identically on Vulkan and DX12: a constant HDR image
developed a 0.125 border error on unsharpened frame 7. RCAS clamping alone did not
fix it. With the corrected upsample load, all eight unsharpened frames have zero
whole-image error on both APIs. Sample weights and temporal equations remain
unchanged.

## Resource and history contract

Callers submit dispatches chronologically on one queue and wait for outstanding
work before destroying or recreating the context. SDK history is independent of
the renderer's frame slots. Inputs and output remain caller-owned; the adapter
retains exact SRV and per-mip UAV bindings while recording each dispatch.
`fsr2_gpu_bytes` reports owned texture mip bytes, excluding external images.

The SDK emits duplicate clears during first execution plus reset. The scheduler
coalesces only uninterrupted clear runs to the final value for each target;
it never moves a clear across a consumer. This eliminates redundant transfer
writes that otherwise produced a Vulkan synchronization error.

Native R32Uint texture atomics are mandatory and checked through RHI. Context
maximum render dimensions require both axes at least 2 and the larger axis at
least 64: the SDK creates half-size luminance and binds mip 5. Unsupported
contexts fail; there is no silent spatial or backend fallback.

## Evidence and limits

`build/fsr2-vulkan-clamp-proof.log` and `build/fsr2-dx12-clamp-proof.log` record
actual RX 9070 XT headless execution, 64x64 native AA and 64x64-to-96x96 upscale:

- Eight unsharpened constant-HDR frames: zero maximum error; RCAS frame:
  maximum error 0.00146484.
- Checker input produces temporal reconstruction changes; reset-to-black equals
  a newly created context exactly and leaves no ghost energy.
- Owned history bytes: 327103 at 64x64 display; 496063 at 96x96 display.
- Zero RHI/backend validation errors or warnings, including teardown. Vulkan
  additionally runs Khronos Core and Synchronization validation.
- The actual production temporal-input shader passes stationary jitter,
  high-coordinate camera translation, sky translation, object override, reset
  and reactive-cap checks on both APIs.

`build/fsr2-domain-fixed-vulkan.log` and `build/fsr2-domain-fixed-dx12.log`
subsequently pass the stronger 0.0025 bound on every constant-HDR frame and
undersized-context rejection, including teardown validation. The production
temporal-input shader also proves its tone-map output against an independent
CPU formula, including the actual sky blue value 999.

The added jittered, slanted sky/terrain edge fixture uses those measured HDR
colors and a matching depth discontinuity. Its HDR-domain negative control
reproduces a maximum display-linear chroma deviation of 0.4502. The corrected
path invokes production `prepare_temporal` and the actual SDK LDR permutation:
all 12 frames at native and Quality scales stay in [0,1] and within 0.0025 of
the two colors' RGB mixture line; measured worst deviation is 0.000993. This
distinguishes an antialiased mixture from a new blue-only fringe without
requiring pixel-identical reconstruction.

Packaged DX12 Quality and Off both pass 600-frame/81-column HUD validation in
`build/fsr2-edge-fixed-package.log` and `build/fsr2-edge-fixed-off-package.log`.
The Quality capture at
`logs/client/validation/fsr-edge-fixed/rml-vgk1sm4y/hud/frame.bmp` was inspected.
Within the recorded silhouette region, the original saturated-blue count of
1152 falls to zero, matching Off; the exact paths and counts are recorded in
`logs/client/fsr2-fringe-comparison.json`. These are bounded scene/capture checks,
not general quality or performance claims across all motion and materials.

`logs/client/fsr2-shaders/domain-proof.json` records exact source/compiler hashes
and 78 successful shader variants covering both HDR and LDR: 26 SPIR-V, 26 DXIL,
26 Metal 3.1 source emissions. It supersedes the earlier 39-variant HDR-only
`final-proof.json`. Per-case compiler logs accompany it. Metal source emission is not an
Apple compiler, native atomic capability or runtime qualification. Packaged
world motion, resize, settings, complete scene materials and performance remain
separate integration checks owned by the renderer qualification.
