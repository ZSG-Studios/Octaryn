# Cloud and water temporal stability

The later [hardware RT and white-cloud checkpoint](ray-tracing-baseline.md)
adds actual terrain reflections/shadows and brighter daytime cloud radiance.
Its bundle hashes and captures supersede the pre-RT color bundle below.

The reported distant-cloud and water jitter exposed an incorrect projection
jitter convention shared by the whole renderer, including FSR Native AA.

The pinned AMD FSR 2.2.1 [reconstruction shader](https://github.com/GPUOpen-Effects/FidelityFX-FSR2/blob/1680d1edd5c034f88ebbbb793d8b88f8842cf804/src/ffx-fsr2-api/shaders/ffx_fsr2_upsample.h#L102)
places an input pixel's sample at `pixel + 0.5 - jitter`. Octaryn previously
shifted clip coordinates by `(-2*jitter.x/width, +2*jitter.y/height)`. With
screen Y downward, that moves geometry by negative jitter and samples the
unjittered scene at `pixel + jitter`. Reconstruction was therefore displaced by
twice the jitter, changing every frame.

The correction uses positive clip X and negative clip Y. The temporal-input
pass removes the same corrected projection offset when reconstructing camera
motion. The SDK jitter, history convention, cloud ray construction, texture
filtering and requested FSR 2.2.1 algorithm remain intact.

Earlier synthetic FSR tests sampled `pixel - jitter` directly. They qualified
the SDK reconstruction but bypassed production raster projection, so they did
not catch this integration defect. New qualification must exercise that
boundary and actual forward cloud/water rendering across multiple frames.

Water already uses unwrapped texture coordinates with explicit gradients,
anisotropic sampling and complete animation mip chains. No mip-filter defect
was found in this investigation. Forward surfaces still lack dedicated motion
for their own animation/parallax; that is separate from the stationary sample
placement error and must not be represented as fixed by changing jitter signs.

## Requested blue colors

The user explicitly requested a vibrant classic blue sky and deep Lake
Michigan-style water. This is an art-direction change, not a claim of exact
restoration or physical lake-depth simulation.

Daytime atmosphere now uses a saturated azure horizon `(0.08, 0.40, 0.95)` and
deep blue zenith `(0.012, 0.14, 0.70)` in the existing linear color domain. The
warm horizon lift is limited to low sun. Fog and reflected sky use the matching
daytime palette; sunset and night equations retain their existing behavior.

Grayscale water animation is tinted cobalt, with deeper blue body/absorption
colors. Water alpha changes from roughly 0.32-0.62 to 0.74-0.96 before the side
multiplier, intentionally limiting gray lakebed bleed-through. Ripple normals,
flow detail, highlights and Fresnel reflection remain. Lava's early branch is
unchanged. No global gamma or exposure change was used.

## Verification

The new production raster oracle rejects the old jitter signs on its first
Native AA frame: error 0.00277781 versus a 0.00001 tolerance. Corrected DX12 and
Vulkan pass 18 frames each in Native AA and Quality (55,296 actual raster pixels
per mode), worst error below 0.00000018. The full native FSR probes also pass
with zero errors and warnings on both APIs.

The actual cloud and fluid shaders run through production temporal preparation
and FSR in an isolated headless sequence: 64 warmup frames, 16 frozen stationary
frames and 16 translated-camera frames per mode. At 384x216 output, DX12 fixed
reference-edge variation falls by:

| Layer | Native AA | Quality |
| --- | --- | --- |
| Clouds | 58.77% | 33.47% |
| Water | 77.46% | 55.56% |

These paired sequences isolate the jitter correction before recoloring. All
stationary Off reference frames match exactly. Moving differences include real
parallax and do not independently establish movement quality. Final color and
opacity are verified separately by rendered captures, not these percentages.

Vulkan's production sequence and jitter oracle pass, but its strict paired
comparison does not: 24 stationary water reference pixels differ consistently
by at most 2/255 between versions. Each Off sequence is internally constant.
No qualified Vulkan shimmer-reduction percentage is claimed, and the strict
reference gate remains active.

Final vibrant/deep-blue packaged captures pass on DX12 with Native AA and
sharpness 0.3. The isolated pool run renders 1,800 frames and all 81 columns.
Actual before/after images were inspected. Packaged mode switching, resizing,
history reset and native-resolution UI checks pass on DX12 and Vulkan; the final
deep-water variant was checked again on Vulkan. Changed sky/water entry points
compile to SPIR-V, DXIL and Metal source. Metal runtime remains untested.

Evidence under `logs/client/validation/cloud-water-jitter`:

- `dx12-jitter-before.log`: expected old-sign oracle failure.
- `dx12-fsr-fixed.log`, `vulkan-fsr-fixed.log`: corrected native oracles.
- `jitter-dx12-comparison.json`: qualified paired stationary sequence report.
- `before-*`, `fixed-*`: actual multi-frame images and CSV records.
- `final-color-dx12`: first blue-water shader sequence.
- `final-temporal`, `vibrant-temporal`, `lake-temporal`: packaged checks.
- `final-lake-blue/frame.bmp`: final vibrant sky and deep-blue water image.

Final client executable SHA256:
`c78935ec576b0cb7df8653612ebc565af1fca42bf5a026dd7aa4534d6c68042b`.
The final shader assets are also required; the executable hash alone does not
identify a palette revision. Earlier intermediate captures remain as evidence.
