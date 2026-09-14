# Greedy mesh seam repair - 2026-09-13

The reported white flicker was reproduced as exposed background inside solid
terrain, with PBR and POM disabled. The production greedy renderer left 71 interior
pixels uncovered across 256 moving oblique views of nine signed-coordinate columns.
The corresponding unit-face reference covered each pixel and its entire 3x3
neighborhood. Existing exact surface tests had not detected these raster cracks.

The cause is greedy T-junction topology: a long triangle edge meets several shorter
edges. Precision-only and integer-coordinate experiments left all 71 holes, so
those experiments were not installed. Primary reference:
https://github.com/cgerikj/binary-greedy-meshing#t-junctions
Its suggested face expansion is not used here.

## Production repair

WorldFaces and WorldGreedy still generate the same compact 16-byte face records.
A GPU-generated uint patch table maps each raster instance to its face and a
six-bit patch ordinal. Small/thin rectangles use unit quads; larger rectangles
use center fans whose perimeter consists entirely of unit-length segments.
The cheaper pattern needs min(width*height,width+height) two-triangle patches.
Neighboring faces therefore share matching edge segments, including chunk borders.
No geometry expansion, LOD, CPU production mesh or optional GPU feature is added.

WorldPatches.slang owns the topology; WorldRaster resolves each patch while
preserving material directions, tiled UVs, winding, sprite geometry and fluid
deformation. The first 80 indirect-argument bytes retain face metadata. The next
80 contain actual patch draw ranges. The SlangRHI draw owner binds the patch buffer
and submits those ranges. GPU memory accounting includes both argument regions and
the patch buffer. Cutouts, sprites, glass and fluids retain unit-face triangulation.

## Verification

- Before: build/voxel-seams-before-gpu.log fails with 71 exposed interior pixels.
- Final: build/voxel-seams-analytic-gpu.log passes all 256 views, 2,288,695 reference
  interior pixels, and an independent analytic floor-ray coverage check with zero
  exposed background. The analytic check does not depend on the unit reference.
- GPU readback verifies every patch mapping, ordinal, material pass, count and
  indirect range, without accepting duplicates or missing records.
- Exact surface/fluid oracles, 22 material raster comparisons, cutout identity,
  3,456 precision comparisons, two binding views, 33 culling views, and real
  water/lava corner arrival/edit/unload lifecycle tests pass.
- CPU camera/halo/draw checks and the standalone raster-culling probe pass.
- Final headless suite: zero validation errors; ten existing unused-output warnings.
  An earlier probe-only readback failed CopySource validation; its diagnostic
  buffers were corrected and the complete suite rerun successfully.
- Installed package passes isolated terrain and validated fluid runs with normal
  exit, all 81 columns, all eight fluid levels and 18 fluid fixture cases. The
  fluid run has zero validation errors and two existing unused-output warnings.
- Installed payload/owner/render graph/500-line checks: 807 code files, 153 payload
  files. Production saves and protected backups are preserved.

## Cost and limits

The radius-4 terrain still uses 320,431 face records for 1,483,600 unit surfaces.
It now draws 977,708 two-triangle patches. Patch storage plus larger argument
buffers adds 3,917,312 bytes: tracked mesh/frame storage rises from 41,999,968 to
45,917,280 bytes. These counters are not total VRAM.

Short unprofiled radius-4 runs averaged 0.575 ms before and 0.754 ms after, an
observed increase of 0.179 ms. This correctness repair has a raster-work cost;
compact storage and some triangle reduction remain. These samples are not a
statistical performance guarantee.

The distance-32 run completed with all 4,225 columns, a drained mesh queue and
normal exit. Its five-second measurement averaged 12.849 ms and tracked 601,064,656
mesh/frame bytes. Window size was not recorded, so this is not a controlled
comparison with earlier distance-32 FPS claims. Source loading still includes
synchronous GPU waits and a 4.074-second outlier; seam correctness does not resolve
streaming hitches or complete broader engine/platform parity.

Runtime evidence: logs/client/voxel-seams-{terrain,fluid,distance-32}.log,
logs/client/voxel-seams-{terrain,fluid}-parity.json, and
logs/client/voxel-seams-qualification.log. Shader sampling tolerances from the
earlier material test remain; seam coverage itself allows zero interior holes.
