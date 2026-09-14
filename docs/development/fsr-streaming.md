# FSR quality and moving chunk delivery

This pass retains the pinned FSR 2.2.1 algorithm, Slang shaders, standalone
Slang RHI, full voxel detail and server authority. Native platform results are
reported separately. The active project is `C:\Users\Rose-X\Documents\Octaryn`.

The later [cloud/water correction](cloud-water-jitter.md) fixes a production
projection-jitter sign defect missed by these earlier synthetic tests and adds
the requested vibrant sky/deep-blue water palette. Its evidence supersedes this
report's limited screenshot-based visual qualification.

## Measured defects

The production renderer copied the opaque HDR baseline before drawing opaque
players and world items. FSR therefore interpreted their color as a transparent
contribution. In the original DX12 Quality capture, all 4,969 player-covered
pixels were strongly reactive: mean 0.86548, minimum 0.64706, maximum 0.89804.
The baseline capture and raw buffers are under
`logs/client/validation/fsr-streaming-before/fsr-avatar`.

AMD's [FSR 2 integration documentation](https://github.com/GPUOpen-Effects/FidelityFX-FSR2)
requires the opaque comparison image to include all opaque geometry. The player
and world-item pipelines write depth and do not enable alpha blending; discarded
sprite texels do not change that classification. Forward clouds and fluids still
need to contribute to the reactive mask.

The initial column delivery path also performed two blocking GPU completion
waits: one to retrieve exact mesh counts, and one to finish emitting the mesh.
These waits ran on the frame thread before camera and block-target queries.
The existing stationary benchmark measured only the interval after residency
and mesh work had settled, so it could not qualify moving streaming tails.

## Integration changes

The opaque comparison snapshot now follows the player and world-item pass and
precedes forward clouds, fluids and selection. Object coverage also excludes
emissive terrain material left behind in the earlier G-buffer. Transparency
over those objects still contributes through the color difference.

Motion encoding reserves Godot's invalid-motion sentinel, including values
that RG16F would round to that sentinel. The reserved outside-history corner
is encoded as another outside-history corner, so it cannot become an erroneous
zero-motion sample through the fallback matrix.

New columns use two reusable asynchronous count/emit jobs, with at most one new
count submitted per pump. The existing ready queue stays bounded to two columns.
A non-consuming peek stages the exact voxel
payload; query data and visible geometry publish together only after the emit
fence completes. Camera collision and targeting therefore keep the previous
data while an edit is in flight. Evicted completions are rejected. New neighbor
arrivals coalesce ordinary border repairs until the expected neighborhood is
ready; changes to existing neighbors and unloads require urgent repairs.
Publication preserves staging order even if the second job finishes first.
A completed delivery can start the next count in the same pump. Normal delivery only polls
completion; blocking completion remains in explicit mesh qualification.

Nonblocking progress checks follow frame submission/presentation.
They can advance count/emit completion but cannot
publish or start a new column.
Publication remains before the next camera update. Its CPU cost is included in
the existing mesh timing bucket. This avoids unnecessary whole-frame delays
between mesh phases. Scratch, result buffers, cancellation and neighborhood
snapshots belong to each slot; both slots contribute to memory accounting.

## Verification record

The Windows aggregate and targeted CPU checks pass. World-stream qualification
retains 24,480 server/client/parser/edit comparisons, and the residency probe
adds exact staged publication, replacement, retirement backpressure and stale
payload rejection. The compiler still reports pre-existing deprecation and
conversion warnings in other probe/application files.

The real GPU delivery and halo lifecycle fixtures pass on Windows DX12 and
Vulkan. The delivery fixture verifies absent queries before count/emit completion,
the old visible edit until replacement, edited air, eviction/reentry, and bounded
scratch reuse. Two adjacent columns are staged together and checked against the
full surface oracle after border repair. Both slots are edited, evicted and
reused, with exact query/geometry publication and ordinary-versus-urgent repair
assertions. Vulkan logs confirm Khronos Core and Synchronization checks.

FSR's GPU input oracle passes nine cases per API, including covered emissive
terrain, transparent overlays and motion sentinel rounding. The SDK also passes
seven render-size changes (`64 -> 48 -> 32 -> 56 -> 64 -> 40 -> 64`) without
reallocating history or resetting after the first frame. Existing HDR, reset,
reconstruction and sky-edge color oracles remain active. The modified input
shader compiles to SPIR-V, DXIL and Metal source; Metal runtime is not tested.

Packaged temporal qualification passes all six preset modes, nine mode/resize
phases and submitted camera/history resets on both APIs with one and two frames
in flight. The four packaged settings cases pass: Custom 72.5%, sharpening off/on,
GPU-driven dynamic resolution on DX12/Vulkan, and Native AA at full resolution.
The real native-resolution RmlUi contract runs with these captures.

Packaged FSR Quality item qualification also passes on both APIs: the isolated
999-item stack travels through 16 authoritative item entities and returns with
count conservation. These runs require native graphics validation.

The final DX12 avatar capture has 4,963 covered pixels and **zero reactivity on
all of them**. The maximum opaque-snapshot versus tone-mapped scene discrepancy
is 0.000234, compared with 0.32548 before. Coverage varies slightly with jitter;
these are independently measured pixel sets, not a pixel-identical frame pair.
Both actual images were inspected. The avatar outline is cleaner without an
obvious new terrain, sky or UI artifact. The raw-buffer validator retains a
baseline negative control and finite/format/coverage checks.

Final image: [DX12 FSR Quality avatar](../../logs/client/validation/fsr-streaming-qualified/fsr-avatar/frame.bmp).

Evidence roots:

- `logs/client/validation/fsr-streaming-native`: actual GPU lifecycle and FSR logs.
- `logs/client/validation/fsr-streaming-qualified/temporal`: final mode/resize runs.
- `logs/client/validation/fsr-streaming-qualified/settings`: settings, DRS and captures.
- `logs/client/validation/fsr-streaming-qualified/items`: authoritative item runs.
- `logs/client/validation/fsr-streaming-qualified/fsr-avatar/object-reactivity.json`:
  before/after mask and color measurements with raw hashes.
- `build/fsr-streaming-pipeline-build.log`: aggregate build and CPU owner checks.
- `build/fsr-streaming-coalescing-build.log`: final renderer and fixture rebuild.

## Moving performance

Windows x64 / RX 9070 XT, 2560x1440, radius 32. Times below are measured
before -> after; lower is better. FPS is 1000 divided by mean frame time.

| Configuration | Mean frame ms / FPS | p99 frame ms | Worst frame ms | Initial population s | Final settle s |
| --- | --- | --- | --- | --- | --- |
| DX12 FSR Quality | 9.83 -> 5.50 / 102 -> 182 | 21.11 -> 8.92 | 39.28 -> 61.16 | 39.71 -> 47.75 | 7.89 -> 6.21 |
| DX12 FSR Off | 9.67 -> 5.43 / 103 -> 184 | 21.66 -> 8.78 | 93.74 -> 94.98 | 39.52 -> 44.98 | 7.76 -> 5.52 |
| Vulkan FSR Quality | 14.67 -> 6.47 / 68 -> 155 | 75.03 -> 9.80 | 232.03 -> 76.32 | 45.48 -> 53.91 | 12.40 -> 8.71 |

Moving mean frame time improves 44-56%, and p99 improves 58-87%. This is a
frame-pacing tradeoff: initial population takes 14-20% longer, and transient
residency is lower. Minimum resident columns change from 4160 to 4115 on DX12
Quality, 4160 to 4127 with FSR off, and 4146 to 4006 on Vulkan. Rare CPU
allocation/submission hitches remain; the worst DX12 Quality sample increases.
These results do not establish hitch-free or universally faster loading.

Every pair has exactly **15,573,564 final quads**, all 4,225 expected column
identities and zero pending meshes. Final accounted GPU storage increases from
936,358,875 to 938,726,751 bytes (2,367,876 bytes, 0.25%) for the second reusable
delivery scratch slot. Peak accounted storage does not increase in these runs.

Reports under `logs/client/validation/fsr-streaming-qualified`:

- `dx12-quality-comparison.json`, run `streaming-dx12-quality-s9soud8k`.
- `dx12-off-comparison.json`, run `streaming-dx12-off-yoi84yi2`.
- `vulkan-quality-comparison.json`, run `streaming-vulkan-quality-ksrxbo6r`.

Frozen baseline executable SHA256:
`68ce6cab223cb18ce7c62761e1b0007196aed7fa069caf5bc654031ec060a62c`.
Final executable SHA256:
`3aea47e7e92b238952d2a96883e6db7d529639d143263ffee1bbec8fa754e57c`.
Intermediate single-slot and pre-coalescing experiments remain under the
`fsr-streaming-after`, `fsr-streaming-final` and `fsr-streaming-pipeline` evidence
roots. Their results are not the final implementation's measurements.

The explicit benchmark moves the rendered camera and the server/client stream
request 480 metres along negative Z at 32 m/s, after initial residency and five
seconds of warmup. The camera is elevated 48 metres and the authoritative player
stays stationary. Every moving frame is recorded and joined to its matching GPU
record. Movement clamps to the same endpoint, then the run waits for all 4,225
columns and zero pending meshes. The parser checks the actual final server
snapshot's complete coordinate set. This exercises the production streaming
path; it does not simulate physical player travel or inject OS input.

Performance runs use 2560x1440, radius 32, bindless batching, two frame slots,
seed 1337/revision 2, full PBR/POM/clouds, no fog or LOD, and native validation
disabled for timing. Separate functional runs require native graphics validation.
Before/after comparisons reject different settings, route, camera endpoint,
device, or final quad count. Executable and bundle hashes are recorded and must
remain unchanged during each measurement. The baseline package contains only
the diagnostic additions, before the FSR and delivery fixes.

From the active repository root, reproduce a run with:

```powershell
python tools/validation/benchmark_streaming.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/validation/streaming-repeat --backend dx12 --upscaler quality --radius 32 --seconds 15 --speed 32
python tools/validation/compare_streaming.py --before <before/result.json> --after <after/result.json> --output <comparison.json>
```

Use `--backend vulkan` or `--upscaler off` for the other measured configurations.
Single runs do not establish statistical confidence intervals, all-world
performance, or the absence of long-session outliers. Linux/macOS runtime and
native Metal execution remain unqualified on this Windows host.
