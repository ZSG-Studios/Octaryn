# Sun history reprojection repair — 2026-09-18

## Source-proven defect

`Shaders/Shadows/Temporal.slang` projected the current world point into the
previous view, selected a nearest pixel, then required the previous pixel's
world position to lie within `max(0.025, distance * 0.0005)` of the current
point. Those are different pixel-center intersections of the same surface.
Native AA/FSR jitter therefore rejected unchanged terrain history, especially
on distant and grazing surfaces. The comparison also affected the history
confidence neighborhood.

The filter now reconstructs the selected previous pixel's ray, including the
previous projection jitter, and intersects the current geometric face plane.
History position is compared against that intersection with the original
tolerance. Material/face equality, valid depth, extent, revision, camera,
player and sunlight validity gates still apply. Sprite lighting normals are
not geometric plane normals, so sprites retain their conservative positional
test. This changes neither ambient strength nor ray count/quality.

Reference inspection: NVIDIA Falcor's
[`SVGFReproject.ps.slang`](https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/SVGFPass/SVGFReproject.ps.slang)
checks depth with a surface-footprint allowance and validates normals rather
than treating lateral pixel-center displacement as geometry motion. Octaryn's
axis-aligned voxel geometry allows an exact plane-intersection comparison;
no reference source was copied. The documented local upstream checkout and
obsolete C++ finish-plan document were absent in this workspace.

## Audit findings

- Global RT-sun history ping-pong follows rendered updates; `previous_view`
  follows the same update. Composite uses the just-written index. HDR sun
  visibility itself is frame-slot-local. No index mismatch was found.
- Raster and procedural ray candidates share `WorldGeometry.slang` and
  `WorldSprites.slang`. Both use alpha cutoff 0.35. The ray path deliberately
  samples mip zero while raster minification is filtered, so distant cutout
  differences remain possible; no evidence here justifies changing quality.
- These foliage geometry paths contain no wind/time deformation. Animated
  player shadows use the separate player acceleration structure.
- Inspected the original natural-world sequence and the debug-1 FSR-off
  sequence `visual-vulkan-1-off-jx8cebla`. The latter contains coherent gray
  visibility silhouettes. It does not establish that every dark region is
  erroneous, nor does this temporal repair explain all FSR-off differences.
  Earlier `--frames` captures allowed user movement and cannot be treated as
  matched camera comparisons.

## Verification and remaining capture

`python tools/validation/validate_shadow_reprojection.py` passes 576
CPU-executed production-Slang cases: all six face orientations, four distances,
four subpixel offsets, displaced occluders, wrong sample positions, sprites,
parallel rays and behind-eye planes. The old positional comparison rejects 54
unchanged-surface cases. Restoring that comparison as a generated-code mutation
fails the same test. The complete temporal shader compiles to SPIR-V, DXIL and
Metal. No GPU run or full native build was performed for this isolated repair.

Use serialized `capture_visual_sequence.py` runs against the same saved source
case and bundle, with `--debug 1 --upscaler off`, `--debug 1 --upscaler native`
and `--debug 12 --upscaler native`. The helper's hidden 12-second benchmark
freezes controls. Debug 1 isolates final sun visibility; debug 12 displays
history age in red (`age / 32`) and visibility in green. Compare the exact same
saved pose before/after packaging this shader. Image review and GPU validation
remain required before claiming visual acceptance.
