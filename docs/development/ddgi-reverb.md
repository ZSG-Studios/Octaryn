# DDGI reverb and sealed-cave investigation — 2026-09-19

The user reported two issues after the stability repair: sunlight projected into
dark caves below tree canopies "even though it passes through blocks", and a
reverb-like echo when sources update the GI nodes. The attached screenshot could
not be viewed by this session, so both were investigated with GPU fixtures.

## Follow-up: sun-view leaks and GI face flicker — fixed

Using the debug views, the user isolated two live defects and both were fixed.

**Direct sun (`Shadows/RayTracing/Shadow.slang`):** the self-intersection gate
ignored every occluder within 5 mm of the ray origin. The origin is offset 2 mm
along the surface normal and grazing sun rays are folded to skim only 0.02 above
the surface, so real near occluders — block outlines — fell inside the gate and
leaked sun. The gate now matches the 2 mm offset (2.5 mm) so only genuine
numerical self-hits are ignored.

**Shadow denoiser (`RTShadowSystem.cpp`, `Shadows/Temporal.slang`):** any sun
motion that changed the sun direction dot below 0.999 discarded the whole
history, exposing one raw per-pixel sun-disk sample. Moving the sun therefore
showed unaccumulated jitter noise. Sun motion now scales inherited history
continuity smoothly (full weight at dot 1, zero below 0.98) instead of
discarding it.

**GI solid-face flicker (`DDGISchedule.cpp`):** an abrupt sun/sky change placed
every probe — including sealed-interior probes whose radiance cannot change
directly — into the bounded reactive window, re-blending them at 50% with fresh
noisy ray samples. Only sky-visible probes now enter that window; interior probes
keep the mature per-observation filter, so sun motion cannot flicker solid faces.
The unused global response timer was removed.

Verification: the 576-case shadow reprojection fixture and its negative control
pass; the publication/scheduler suites pass with sky-classification coverage;
all 25 GPU transitions pass on DX12 (0.15–1.77 s) and Vulkan (0.15–1.15 s); the
sealed-canopy case stays dark; torch-removal steady variation remains 90–97%
below the pre-stability baseline; a 12-frame 1920x1080 natural-world capture has
no temporal resets, 2.23 ms median / 2.69 ms p95 frame wall and no outliers.

## Sealed-cave sunlight: not reproduced, now guarded

A new production GPU case places a full canopy of leaves above a sealed stone
roof over the measured tunnel interior, with the sun on:

| Case | Interior energy |
| --- | ---: |
| Open roof, sun on | 0.1349 (DX12) / 0.1363 (Vulkan) |
| Sealed stone roof (no canopy) | 0.0016 / 0.0010 |
| Sealed stone roof with canopy above | 0.0016 / 0.0019 |

A sealed roof keeps the interior at about **1.2% of the open-sun level**, and the
canopy above it changes nothing: leaves do not open a sun path through solid
blocks in the DDGI trace, occupancy or sampling path. The residual is the
configured sky ambient floor, not sunlight. `canopy_over_sealed_roof` is now a
permanent regression case in `--ddgi-response-only` on both backends.

The reported leak is therefore not in the DDGI probe path for a sealed interior.
Likely remaining candidates are camera-space direct sun/RT-shadow behavior at a
specific cave opening, or light entering a cave mouth and spreading wider than
physically correct through probe interpolation near the entrance. A reproduction
with the actual world coordinates is needed before changing more code; no leak
was "fixed" without evidence.

## Source-update reverb: measured and reduced

The reported "reverb" matches measurable transition oscillation. The existing
`analyze_ddgi_transition.py` metrics quantify it:

| Measurement | Baseline build | Current build |
| --- | ---: | ---: |
| Worst overshoot (DX12) | 37.2% (`roof_parked`) | 2.8% (`resident_emission`) |
| Worst overshoot (Vulkan) | 41.5% | 2.4% |
| Worst reversals | 8 | 4 (single small-span case) |
| Typical reversals | 0–8 | 0–1 |

Changes that produced this:

- An explicit lighting or geometry change adopts the bounded reactive policy
  (four observations leaning 50% toward the new field) instead of replacing the
  accumulated field with one noisy ray sample. Only a new cell, relocation or
  genuine occlusion change discards irradiance outright.
- Abrupt environment changes (sun toggle, day/night) request a bounded, smooth,
  age-prioritized response instead of a global dirty burst that retraced the
  whole field at once and echoed across it. Gradual drift stays on the ordinary
  cadence and never republishes controls.
- The unused per-observation interval buffer and environment-tracking uniform
  were removed; the update pass keeps steady per-observation retention.

A progressively damped cascade (leaning 50%→75%→mature across the window) was
implemented and verified on CPU, but it slowed measured GPU response about 5x
and was reverted in favor of the fast bounded response above.

## Follow-up: removal latency — fixed

Removing a light blended the old energy out at 50% per observation, leaving a
visible tail. A hard removal now snaps the affected probes dark on the change
observation (the recursive marker already rejected the old bounce, and the
remaining ambient is stable). The 108-sequence CPU fixture still reports zero
overshoot, and the GPU removal phases reach 80% in 0.15–0.40 s on DX12. The
reported transition dip after a removal is the physically correct rebuild of the
ambient bounce field.

## Follow-up: full shader/RT/GI scan — through-wall probe seeding found and fixed

A complete pass over every lighting-carrying shader (RT rays, shadow clipmap and
temporal filter, DDGI trace/update/seed/sample/resolve, composite, ambient,
local lights, reflections, sky visibility) found one genuine through-block
projection path:

`DDGISeed.slang` accepted a seed donor when the target was within the donor's
mean occluder distance **plus two standard deviations plus half a probe cell**.
With the fine volume's one-block spacing, that half-cell slack alone spans a
one-block wall, so every camera scroll seeded new edge probes with a sunlit
exterior probe's full irradiance field straight through terrain. Seeded probes
participate in shading immediately, an up-facing floor samples their brightest
texels, and the borrowed field tracks the sun — matching every reported
symptom: bleed that projects through blocks, worsens while moving (scrolls
reseed), follows fast sun movement, and lights underground ground faces. The
slack is now a numeric epsilon (5% of spacing). Starving a seed is safe by
design: the pending corner falls back to the coarser volume instead of borrowing.

The scan also re-confirmed the audited paths: sun/shadow rays terminate on
opaque geometry, the clipmap resolve is raster-only, the temporal shadow filter
gates every neighborhood on the same block face within 0.2 blocks, the sky
fallback is ray-tested in both volumes, reflections share the same occluders,
and the raster ambient hemisphere term is position-independent by design and
only reaches pixels where probe coverage is partial — where the ray-tested sky
replaces it.

Verification after the fix: all shader cases compile for DXIL/SPIR-V/Metal; the
25-transition GPU fixture passes on DX12; the torch fixture still measures
90–97% lower steady variation than the pre-stability baseline.

## Dark-room leak localization fixture

At the user's direction a dedicated GPU instrument now exists:
`octaryn_client_world_mesh_probe --ddgi-dark-room-only` (evidence under
`logs/client/ddgi-dark-room/`). It builds a lightproof stone shell around a
sealed interior, parks the camera inside, and measures **every interior probe
texel** while (a) the sun is static, (b) the sun sweeps quickly, (c) the camera
scrolls the volume, and (d) both. Any irradiance that appears inside is reported
with the exact probe cell and octahedral direction that carried it, plus a CSV
time series; an exterior cohort verifies the sun actually lit the outside so the
test cannot pass vacuously.

Current result on Windows DX12 after the seed fix: interior maximum
0.0005–0.0017 against an exterior of 0.28–0.41 — the sealed interior stays dark
under sun motion and volume scrolling. The pre-fix baseline for comparison
retained about 12% of exterior ambient through the walls.

At the user's follow-up, a two-layer transmissive **leaf canopy was added
directly above the sealed roof** and the same phases re-run: interior maximum
0.0003–0.0012 against an exterior of 0.33–0.43. The GI probe path does not leak
through a sealed shell even with foliage above it.

## Direct-sun range truncation — fixed

The user confirmed the artifact is visible in the **Sun direct** debug view. The
sun-visibility ray traced only to the user's Shadow Distance setting and treated
a range-truncated miss as fully lit, so inside large enclosures — where grazing
rays exceed the shadow distance before reaching a wall — surfaces lit up with
geometry-correlated patterns. The ray now traces to the full scene range; the
shadow-distance fade still softens the boundary without deciding lit/shadowed.

## Foliage-edge light stripes — fixed

The user identified solid clear light stripes cast from the edges of transparent
foliage onto underground walls, varying with time of day, and suspected gaps
between blocks. Both seam locations at block boundaries were closed:

- Merged cutout quads carry atlas UVs beyond one tile; at every integer block
  boundary the bilinear cutout sample straddles the atlas wrap seam where leaf
  edge texels read near-transparent alpha. The raster blends it invisibly behind
  other leaf layers, but the shadow ray's binary 0.35 test transmitted along
  those seam lines. `world_ray_sample`'s crisp mip-zero lookup now snaps to the
  enclosing texel center.
- Each merged quad is tested as two triangles with strict barycentric rejection;
  rounding could reject a shared-edge hit on both neighbors, opening hairline
  cracks between blocks. `world_ray_triangle` now carries a micron of edge
  tolerance so seam hits always commit.

Ray-scene bounds are padded outward and corner geometry uses exact integers, so
no geometric block gaps exist; both artifacts were seam-sampling effects. All
shader targets compile; the torch fixture still measures 90–97% below the
pre-stability baseline.

## Verification

- CPU: `validate_ddgi_response.py` passes response/visibility/transition
  fixtures with zero overshoot across 108 add/remove sequences, and rejects
  snap-on-change, reversed visibility and presentation black-hole mutations.
  `validate_ddgi_light_publication.py` passes publication, 35 scheduler cases
  and the response matrix; abrupt environment responses complete in 0.43–0.47 s
  at 30–144 Hz on CPU. Gradual drift requests no global work.
- GPU: `--ddgi-response-only` passes all 25 transitions on Windows DX12 and
  Vulkan, including the new canopy case. Torch-removal steady variation remains
  90–97% below the pre-stability baseline.
- DX12 transition settling in this fixture is now 1.5–2 s (Vulkan 0.3–0.9 s),
  slower than the previous full-snap build's 0.03–0.78 s. That is the measured
  cost of removing the noisy full-field snap; local geometry and emission cases
  still settle in 0.15–0.65 s.

## Remaining limits

The reverb is greatly reduced but not provably zero in every scene; moment-based
visibility cannot prove every thin-wall case, and the cave leak report needs a
reproducible case before further changes.
