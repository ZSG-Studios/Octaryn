# Atlas minification and pixel magnification

The filtering repair removes the inherited `0.45` derivative multiplier, which
requested about 1.15 more detailed mip levels than the pixel footprint requires.
The old opaque sampler also selected a single nearest mip, and sprite albedo
always selected mip zero. These policies preserve sharp individual texels but
alias under minification and change mip levels abruptly. They were present in
the recovered reference; this is an explicitly requested improvement, not a
claim that the original implemented smoother filtering.

`Materials/MaterialSampling.slang` now selects exact point-sampled mip zero for
albedo footprints at most half a texel per pixel. Between half and one texel it
smoothly blends to the existing linear, trilinear, 8x anisotropic sampler.
Minified albedo uses that sampler with unbiased explicit gradients. Gradients
still come from continuous UV coordinates before POM changes the sample point.
The forward fluid sampler also uses unbiased gradients.

`Materials/SpriteSampling.slang` uses a separate clamp-to-edge sampler so coarse
mip footprints cannot wrap to the opposite sprite edge. It keeps the original
half-texel inset and the same crisp-to-filtered transition. The sampler is
appended after the retained mesh resources and bound by its reflected name.
UI atlas previews retain their independent nearest mip-zero sampler.

LabPBR packed material data remains point sampled. Interpolating encoded metal
categories, subsurface categories or alpha-255 no-emission sentinels would change
their meaning. Terrain data sampling loses the negative mip bias; sprite data
retains its original explicit mip-zero behavior. This slice does not implement
filtering of decoded material parameters or redesign POM.

CPU mip generation retains its original filtering: albedo is averaged in linear
light with alpha weighting, transparent RGB is dilated, and representable alpha
coverage is preserved. Normal mip generation constrains the encoded XY vector;
specular mip generation handles category majorities and no-emission sentinels
separately.

Two alpha-correction defects are repaired. Already-correct mip coverage now
keeps scale one instead of needlessly reducing opaque alpha to the cutoff.
The coverage search also measures rounded byte values, matching the committed
texture: previously a passing float alpha near 89.3 rounded to byte 89, below
the shader's 89.25 cutoff. Required coverage corrections retain the existing
bounded search algorithm.

The Vulkan specification separates minification, magnification and mip filters,
and notes vendor variation when anisotropy is combined with nearest filtering.
Using separate point and all-linear anisotropic samples avoids relying on that
combination for pixel-art magnification:
[samplers](https://docs.vulkan.org/spec/latest/chapters/samplers.html),
[sampling](https://docs.vulkan.org/spec/latest/chapters/textures.html).

## Qualification

The production CPU mip cases and 10,460 actual GPU filtering samples pass in
`build/mip-batch-final-validation.log`, including both alpha corrections. The
complete surface/material/fluid/culling suite and both individual and batched
watertight rendering checks also pass in that log. Whole-world performance is
recorded separately in `voxel-batching.md`.
The initial diagnostic ramp exposed a test assumption, not a mip bias.
On the RX 9070 XT, AF8 preserves integer mip endpoints but compresses each
transition into fractional LOD .25–.75 (maximum .025 output step in a .2-per-mip
ramp sampled every 1/16 LOD). Vulkan permits implementation-dependent anisotropic
schemes. The test now checks integer endpoints, adjacent-level bounds and the
original continuity limit, alongside a separate non-anisotropic interpolation
oracle and explicit negative controls using the old .45 gradient multiplier.
Measured non-anisotropic ramp error is at most 0.000781253; all five old-bias
negative controls are rejected. Across mip0–4, measured half-cutout coverage is
0.509766/0.519531/0.537109/0.576172/0.650391 for wrapping terrain and
0.504883/0.509766/0.519531/0.539062/0.577148 for clamped sprites, matching the
analytic filtered half-step oracle. This does not claim exact arbitrary cutout
coverage at every fractional mip, especially an unrepresentable one-pixel mip.

The existing `octaryn_client_world_mesh_probe` now includes:

- `--mips-cpu`: production mip routines, linear-light and alpha-weighted color,
  flat data across all six levels, categorical LabPBR channels, no-emission
  sentinels and representable half-tile cutout coverage. No device is created.
- `--atlas-only`: actual production helpers and atlas samplers on known mip and
  layer contents. It checks exact magnified colors, minified and oblique checker
  footprints, six mip levels over footprint sizes 1–32, continuous mip and
  magnification transitions, sprite edge clamping and unbiased point-data LOD.
- The full existing face, fluid, watertight patch and MRT parity cases. The
  sampling diagnostic distinguishes point material boundaries from albedo:
  a material boundary cannot excuse a minified filtered-albedo mismatch.

The fixture is deliberately sensitive to the old nearest-mip jumps and negative
bias. Anisotropic implementation precision is bounded in the mip-ramp checks;
the test does not demand one vendor's exact tap placement. Performance requires
separate whole-world measurements and is not inferred from sampler settings.
