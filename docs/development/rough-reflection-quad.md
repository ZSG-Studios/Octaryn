# Retired rough reflection quad experiment

Status: rejected after build37 DX12 pilot; production implementation, helper flags
and probe tooling removed for build38. The retained temporal path is restored.
A nonzero `OCTARYN_CLIENT_RT_ROUGH_QUAD` now fails explicitly instead of silently
selecting a different renderer. Schema7 diagnostic columns remain reserved zero
and historical readers remain available. No populated or cross-backend acceptance.

The exact experiment, shader dependencies, controls, probes and compiled artifacts
are archived with verified SHA256 hashes in
`logs/client/hq200-build37/rejected-source/manifest.json`. The pilot report is
`logs/client/hq200-build37-rough-quad-pilot-review.json`.

## Measured rejection

The single quiet pair rendered at 2560x1440 output with fixed 1280x720 internal
resolution. All 360 post-warmup frames and outliers are retained. This pair
establishes a large regression, not a promotion or noise estimate.

| Metric | Retained | Quad experiment |
|---|---:|---:|
| GPU mean | 5.975657 ms | 17.475680 ms |
| GPU p99 | 7.598996 ms | 19.126752 ms |
| GPU worst | 8.153960 ms | 20.816000 ms |
| Reflection trace + filter + classify mean | 3.147849 ms | 14.395118 ms |
| Reflection sum p99 | 4.688284 ms | 15.669212 ms |
| Whole frame mean | 6.155742 ms | 17.797740 ms |
| Whole frame p99 | 7.945383 ms | 19.635900 ms |
| Whole frame worst | 8.346900 ms | 99.676895 ms |

The producer alone fell from 3.046005 to 0.499022 ms, but resolve and full-current
fallback rose from 0.101844 to 13.896096 ms. Producer-only reporting would hide
the regression. Native1440 diagnostic quality runs reconstructed only 52,268,017
of 1,320,631,043 receivers (3.96%). Reflection queries increased from 1,085,079,514
to 7,587,143,357. Conservative rejection sent most pixels through fresh eight-ray
fallback instead of the retained temporal sampling. No further tuning was accepted.

Both native1440 quality captures exited successfully with graphics validation.
Camera, lighting, sampling phases, executable, shader and asset identities matched;
helper-source identity was not recorded and is explicitly unverified. All twelve
images at ready frames 318-329 differed. Actual sequence and native crop review
shows retained street geometry, but a brighter, broader post-cut bronze bollard
highlight and changed fine reflected detail. This is not quality parity. The
comparison also changes fresh versus temporal accumulation, so these differences
cannot be attributed solely to quad sharing. No emissive, populated, Vulkan or
long-sequence qualification followed the decisive performance failure.

## Archived design and limited pre-runtime evidence

One current GGX/VNDF quadrature direction is traced for each admissible 2x2
rough receiver cell (roughness >=0.45). Sharp and intermediate receivers remain
full rate. Current depth, normal, albedo, metallic and roughness continuity and
existing strict previous-surface reprojection gate sharing. Previous radiance is
never accumulated in the active prototype: cuts, invalid support and unsupported
cells evaluate the original full current quadrature (up to eight directions).
Output history age is one. This can be substantially more expensive than the
retained temporal renderer when reconstruction coverage is low.

The producer stores incident radiance, unchanged ray direction and its source
VNDF PDF, secondary geometric normal/distance and source receiver position.
Resolve evaluates each recipient's specular BRDF times cosine divided by the
source PDF, then normalizes only spatial weights. It does not copy source-weighted
RGB or shift sample directions; no shifted-hit Jacobian is introduced. The fixed
eight-direction quadrature is not an unbiased continuous Monte Carlo estimator.
Below-horizon source outcomes retain zero contributions and denominator weight;
if such a direction is above the recipient horizon, the recipient falls back.
An unsupported otherwise-admissible neighbor forces full-current fallback rather
than renormalizing only surviving samples. Alpha-tested traversal and secondary
visibility queries remain unchanged; alpha hits cannot supply reconstructed
radiance. Close secondary geometry and hit/miss/normal/distance disagreement
also reject sharing.

Receiver similarity does not certify secondary visibility, parallax, illumination
or subpixel opaque gaps. Ordinary G-buffer data has no exact primary triangle or
alpha provenance. These are explicit spatial approximation risks requiring actual
images, not exact visibility claims. No arbitrary mip bias or roughness blur was
added. Existing moving-history artifacts are not repaired.

Any rendered item instance, including sleeping items, disables the whole prototype
and uses the retained renderer. Switching invalidates history. The shader also
refuses sharing with a dynamic instance or player-shadow scene. This initial
control provides zero populated acceleration; static speed cannot qualify the
populated HQ200 contract. No screen-space item-box approximation is used.

Four quarter-grid targets (RGBA16F plus three RGBA32F) allocate at the existing
maximum DRS extent. Producer UAVs transition to read-only resolve SRVs on the
same queue; resize waits for existing frame fences. ReflectionTrace measures the
producer including its barrier; ReflectionFilter measures resolve, full-current
fallback rays and output transitions. Sum both when comparing this candidate.
Schema7 appends cells, source rays, reconstructed receivers, full-rate receivers
and published samples, preserving all schema6 fields. A frame with quad cells
requires reconstructed+full-rate == reflection receivers. Diagnostic reports state
whether reconstruction was actually exercised; quiet runs do not infer coverage.

CPU production-Slang math oracle: 3,000 cases verify source/recipient equality
reduces BRDF*cos/PDF to the existing Fresnel*Smith quadrature weight (maximum
absolute error 4.03e-7). This is not spatial reconstruction proof. Eight DXIL/SPIR-V
counter0/1 producer/resolve variants and focused native syntax checks pass. Reports
are under `build/release-windows/tools/rough-quad`; logs under `logs/build/rough-quad-*`.

The original pilot command plan is preserved in
`logs/tools/build37-rough-quad-pilot.json`; the analyzer and reviewed crops remain
under the build37 evidence paths. The criteria described above were not met.

Primary guidance: [AMD SSSR](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/stochastic-screen-space-reflections/)
and [AMD denoiser](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/denoiser/)
describe variable-rate reflection tracing and spatial reconstruction. The retired
experiment was a first-party Slang implementation, not an SDK substitution.
