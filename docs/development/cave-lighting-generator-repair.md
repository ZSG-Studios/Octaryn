# Cave lighting and one natural generator

The old ordinary launch path opened `saves/open-world-v2`, selecting the
vegetation-free generator despite the newer foliage implementation. Both native
startup and `tools/run-client.ps1` now use `saves/open-world-v3`. The only built-in
generator is natural terrain revision 3: six climate/material classifications,
enclosed caves, coast/river-valley terrain, trees, bushes and four flower species.
Old flat, empty and vegetation-free implementations and the duplicate chunk-local
feature recipe are removed. Compiled biome/feature descriptors match the kernel.
Existing saves remain on disk; unsupported identity fails before edit cleanup or
import writes. No old terrain is silently regenerated underneath player edits.

The reported camera-following dark cave region had two concrete causes:

- Recursive probe lighting used a different volume hierarchy from final shading.
  Both volumes now trace the same previous-frame hierarchy before either updates.
- Beyond the finite GI field, final shading restored unoccluded ambient even
  underground. The ray-capable path now tests sky visibility there. Fully covered
  pixels do not pay this extra cost; DDGI-disabled rendering retains its fallback.

Fine-volume blending distinguishes not-yet-updated probes from confirmed dark or
embedded probes. Pending cells leave valid coarse illumination available;
confirmed occlusion still suppresses leaks. Deterministic visibility moments
update when the edited acceleration structure becomes available.

Integrated screenshots also exposed a missing copy of the new HDR ray pipeline
into the second in-flight frame slot. Both slots now share it. Capture stride is
odd and validation requires both frame parities, preventing an even capture
interval from concealing alternating rendering paths. Earlier results under
`logs/client/cave-repair-final/` are explicitly marked superseded for this defect;
the final integrated runs are under `logs/client/cave-repair-verified/`.

The sun-shadow filter compares short illumination history with the current
matching-surface neighborhood so coherent moving shadows lower history reuse.
The historical local-light filter used by this report's captures has since been
removed; current [direct local lighting](local-lighting.md) has no temporal history.
See [temporal response](lighting-temporal-response.md) for scoped GPU signal tests.

## References

- [JCGT production DDGI](https://jcgt.org/published/0010/02/01/paper.pdf):
  multiresolution volumes, temporal updates and visibility-aware interpolation.
- [RTXGI DDGI volumes](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/DDGIVolume.md):
  scrolling probe state, relocation and classification.
- [SVGF paper](https://research.nvidia.com/sites/default/files/pubs/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A/svgf_preprint.pdf)
  and [NRD integration](https://github.com/NVIDIA-RTX/NRD/blob/master/README.md):
  surface-valid temporal filtering, short histories and illumination confidence.
- [Godot SDFGI limitations](https://docs.godotengine.org/en/stable/tutorials/3d/global_illumination/using_sdfgi.html):
  cascade shifts and the distinction between sky contribution and interior GI.

These informed original Octaryn Slang/RHI changes; NRD, RTXGI and Godot renderers
were not installed or substituted for Octaryn's runtime.

## Verification

Native/managed persistence and generation probes pass. Rejected legacy saves are
checked byte-for-byte unchanged. The cache matches 4,194,304 scalar-oracle voxels;
the client stream compares 700,320 authoritative samples, including 466 trunk
blocks, 1,684 leaves, 642 bushes, all four flowers and 206 seam leaves. Server
vegetation coverage includes 2,217,483 checks. Logs:
`logs/build/single-generator-native-retry.log`,
`logs/build/single-generator-managed.log`,
`logs/build/single-generator-persistence.log`, and
`logs/client/single-generator-world-stream.log`.

The descriptor validator passes and rejects eight stale/mutated variants:
`logs/validation/single-generator-worldgen-content.json`.

The cave fixture is at signed Y=-34 with a fully enclosed 19-block-wide hall.
The camera sweeps eight metres while its far wall crosses the coarse probe fade.
A separate one/two-block-wide underground fixture removes and replaces a wall
through server-authoritative commands. Spawn shafts are closed by accepted server
commands before captures; no OS input is injected. Baseline evidence is under
`logs/client/cave-repair-baseline/`.

Final Windows RX 9070 XT qualification:

- DX12 and Vulkan each passed a 1,000-frame underground lateral sweep with 32
  captures spanning both frame slots. The sealed far-wall ROI stayed below
  0.002991 and 0.002399 display luminance respectively (limit 0.02); the incorrect
  fallback measured about 0.153. These are fixture pixels, not global GI error.
- DX12 one-block and Vulkan two-block underground tunnels each passed 1,200
  frames, 48 captures, authoritative roof closure, removal and replacement.
  Open/restored frames were inspected. Last-16 receiver variation is mixed:
  DX12 floor/left-wall pair changes decrease while the right wall increases from
  0.000365 to 0.000434. This is not an all-region noise-reduction claim.
- A 600-frame 1280x720 DX12 natural forest run has zero authored terrain blocks;
  its inspected capture shows trees, bushes, all flower types and leaf cutouts.
- Production-shader temporal response passes eleven cases on each API. The
  DDGI GPU oracle checks density ordering, energy, continuous boundaries,
  pending-fine handoff, embedded probes and valid darkness. Twenty-one scheduler
  cases and twelve DDGI/HDR shader target compilations pass.
- No graphics validation warnings/errors were reported in the final GPU runs.
  The native bundle build includes seven existing `getenv` deprecation warnings
  in `PathPolicy.cpp`; managed builds report zero warnings/errors.

`logs/client/cave-repair-verified/bundle-manifest.json` records the executable,
all 100 first-party shader hashes, exact case paths and per-pass GPU timings.
The normal bundle executable matches the final native build. Preliminary timing
figures recorded before the frame-slot correction are superseded. Final capture
medians range from 0.3459 to 0.4998 ms for cave composition, 0.1969 to 0.9585 ms for
local denoising, and 0.0712 to 0.4592 ms for sun filtering at 960x540. The forest
case is 1280x720 and has different costs. These are diagnostic captures with
readbacks, not a matched FPS benchmark or a hardware-wide performance promise.

## Scope and cost

DDGI remains an approximate finite irradiance cache. The uncovered-surface sky
fallback uses at most four deterministic directions (two on vertical voxel faces,
zero on downward faces); small openings can be undersampled. It is visibility-
tested sky irradiance, not an unlimited indirect-light solution. Graphics results
on the Windows RX 9070 XT do not establish other-platform hardware support.

The captured results above predate the replacement of the local direct-light
sampler. The current implementation uses deterministic tiled light lists and
per-contributor RT visibility, with full-list evaluation on tile overflow. The
engine still allows 4,096 nearby block lights, including exposed lava, so dense
light cost needs separate measurement. See [current local lighting](local-lighting.md)
and [AMD's tiled lighting implementation](https://gpuopen.com/learn/tiledlighting11-directx-11-sdk-sample/).
