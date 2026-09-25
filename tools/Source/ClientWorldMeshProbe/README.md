# Production world mesh qualification

Build `octaryn_client_world_mesh_probe`; run the isolated headless GPU target
`octaryn_validate_client_world_mesh`. The target stages source atlas/catalog/shaders
beside its own executable and requires Vulkan validation. It never opens a window.

The surface oracle independently enumerates catalog-visible unit faces. It compares
the exact sorted multiset to expanded production GPU rectangles, validates packed
IDs/directions/extents/pass ranges and indirect arguments, and independently checks
all emitted fluid corner heights and flow records. Signed coordinates, full 512-block height,
partial 32-block height bands, mixed materials, holes, halos and special geometry are covered.

Raster comparisons use production atlas textures, vertex/fragment entry points,
pipeline states and WorldDraw. A CPU unit-face buffer is diagnostic reference
geometry only; the game still meshes entirely on GPU. Six directions, near/distant
views, PBR/POM, concave geometry and the complete leaf slab are compared. Leaves
remain unit geometry because merging alpha-tested faces changes cutout sampling.
The two-column binding test compares actual WorldDraw with separately bound GPU
buffers against the same packed geometry concatenated into one buffer: all four
MRTs must be byte-identical and depth identical, with both materials visible.

## Numeric bounds

Exact geometric surface equality is mandatory. Rasterization of differently sized
triangles can round interpolants differently at point-sampler and silhouette edges.
The following bounds are explicit qualification tolerances, not production geometry
expansion and not claims about a queried hardware subpixel precision limit.

- Material/layer/direction bytes must match for every jointly covered pixel.
- Depth must differ by at most 0.000002. Relative-position half floats allow 0.0625
  or one binary16 ULP at the component magnitude, whichever is larger.
- Color differs by at most 0.008 and material channels by at most 2/255, except
  individually proven sampling cases below. Diagnostic Slang reads exact
  float32 UV/gradients; a constant-per-mip texture measures the actual hardware
  mip choice instead of assuming a particular LOD approximation. UV differences
  must be within 1/256 pixel of measured gradient plus 32 float epsilons, and must
  straddle a different nearest texel or adjacent mip. Mip changes additionally
  require gradient-derived LOD difference <= 0.01. No unclassified pixel is allowed;
  classified pixels are capped at floor(covered/100)+1 per view.
- With filtered albedo, a point-data boundary cannot excuse an albedo mismatch.
  Magnified albedo's point component can use its own nearest-texel boundary.
  For minification, both post-POM UV and every derivative component must remain
  within the same tiny interpolation envelope. A separate compute dispatch then
  samples the production helper and real atlas at each measured UV/gradient pair;
  BOTH original G-buffer RGB values must reproduce within one binary16 ULP and
  ambient alpha must match exactly. The same sparse pixel cap remains mandatory.
- A coverage difference must be within 1/256 pixel of an exactly projected face
  edge and next to a pixel uncovered in BOTH images. This prevents accepting
  interior T-junction cracks. The same 1% + 1 cap applies. Cutout differences are not
  waived merely because a texel lies near its alpha threshold.

On 2026-09-13 the RX 9070 XT run passed 14 surface fixtures, 22 raster comparisons and
2 binding views with 0 validation errors. Point-boundary differences affected 14–70
pixels in applicable views; silhouette differences were one pixel, 0.000859–0.001064
pixel from the projected edge. The full 4704-face leaf fixture was pixel-identical.
Ten Shader-OutputNotConsumed warnings were logged; this is not a zero-warning claim.
Evidence: `build/no-lod-mesh-parity-final.log`. Subsequent cleanup removed temporary
hard-coded pixel/phase diagnostics; compilation was rerun without another GPU run.

This is bounded fixture coverage, not exhaustive all-camera/all-GPU image identity.
It does not measure frame performance or validate forward fluid shading anew.

The mip-filtered pass is qualified separately in
`docs/development/atlas-filtering.md`. Three near-POM differences reproduced from
their measured sampler inputs with maximum error 0.000118732 in
`build/mip-batch-complete-gpu.log`; no base raster tolerance was increased.

The complete leaf slab remains unit geometry and must match all four MRTs and
depth byte-for-byte. Its independent CPU face set is consumed by full face key
in production primitive order; missing, duplicate and unconsumed keys fail.
Sorting these intersecting faces changes exact-depth winners: an isolated
reversal test found three changed directions at identical depths, all within
the same atlas layer (`build/mip-cutout-isolated.log`). Reversal is checked
separately and cannot change depth or material layer. Preserving reference
primitive order removes that ambiguous tie expectation without relaxing any
pixel equality assertion.

## Culling and relative-coordinate regressions

`Culling.cpp` retains 25 actual signed-coordinate columns with varied vertical
bounds. It compares all four G-buffer targets and depth byte-for-byte with CPU
frustum culling on/off over 33 pitched/yawed views, including square, wide and
portrait/zoom projections. The fixture uses actual map keys and viewport sizes.

`RelativePrecision.cpp` invokes the same callable vertex body used by the
production Slang vertex entry. Actual meshed torch, water and lava records must
preserve all relative/clip components exactly after translations of +/-16,777,216
in X/Z, at both ends of the supported vertical interval. An isolated staged-shader
run restoring the old world-first expression fails this regression; production
source remains untouched during that negative test.

Both regressions pass on RX 9070 XT in `build/voxel-repair-final-build.log`:
33 culling views (32 with substantial coverage, 30 rejecting columns), and
3,456 vertex scalar comparisons. Negative GPU evidence is
`build/voxel-relative-before-gpu.log`. These are finite fixture proofs, not
all-camera/all-world or all-platform guarantees.

`HaloLifecycle.cpp` additionally exercises actual retained-column update, priority
refresh and eviction for water and lava across the signed (-1,-1) four-column
corner. Each arrival, diagonal level/above edit and unload is followed by complete
independent surface/fluid verification of all settled retained meshes. The shared
corner must actually change, become full height with fluid above, and return to
its standalone height when neighbors unload. Final combined GPU evidence:
`build/voxel-halo-lifecycle-gpu.log`; zero validation errors, ten existing
Shader-OutputNotConsumed warnings. This checks settled correctness, not zero-latency
publication of all adjacent meshes in a single frame.

## Watertight edge coverage

`Seams.cpp` exercises 256 subpixel camera poses around a continuous signed-column
terrain fixture with different greedy partitions and stepped heights. A fully
covered 3x3 unit-reference neighborhood must never expose background in the
production draw. An independent camera-ray/solid-floor oracle additionally tests
coverage without relying on the reference mesh. Both gates allow zero holes.

GPU patch tables and the second indirect-argument region are checked exhaustively
against each decoded rectangle's exact patch domain. The first indirect region
and compact face ABI remain independently checked. The diagnostic buffer helper
declares CopySource because combined reference buffers are read back as well.

Before topology repair: 71 interior holes. Final evidence:
`build/voxel-seams-analytic-gpu.log`, with zero holes in 2,288,695 reference pixels
plus the analytic gate. `--seams-only` runs this case for targeted investigation;
full qualification runs all preceding surface/material/fluid/culling tests too.
