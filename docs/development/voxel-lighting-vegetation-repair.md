# Voxel lighting and vegetation repair — 2026-09-14

The rebuilt client connects natural vegetation and resident emissive blocks to
the existing Slang RHI voxel renderer. This repair targets observed integration
defects; it does not claim every DDGI scene is free of approximation artifacts.

## Corrections

- Scene refreshes retain DDGI history and relocation. Only new scrolling cells
  and relocation reject stale irradiance. Fixed geometry rays stabilize probe
  relocation; visibility distances are limited to the local probe neighborhood.
- Interpolation applies trilinear weights after visibility suppression. Invalid
  probes inside the volume cannot reintroduce bright unoccluded ambient. Bright
  stochastic samples keep temporal filtering.
- Edited voxel columns retain their immutable ray geometry until the replacement
  acceleration structure publishes. Actual unloads and empty columns remove it.
- Seven colored torches and all lava levels declare emission in the block
  catalog. Sparse, cached emitter discovery updates on source publication and
  removal. ReSTIR and DDGI share these lights, material emission and source-aware
  visibility. The source voxel cannot shadow itself; neighboring walls still can.
- New revision-3 natural worlds generate deterministic trees, bushes and four
  flower species through the same server/client kernel. Halo anchors reproduce
  canopies across signed column boundaries; edits apply last. Existing revision-2
  worlds retain their original terrain. Export/import preserves the revision and
  rejects attempts to import into a different revision before writing game state.

See [DDGI design and references](ddgi.md) and [terrain generation](terrain-generation.md).

## Verification

All paths below are repository-relative. Windows hardware was AMD Radeon RX 9070 XT.

- Build: `logs/build/ddgi-verified-build.log` passed client bundle and GPU mesh
  probe. Native and managed persistence validation passed in
  `logs/build/ddgi-repair-final-build.log`, including revision-2/3 export/import.
- Scheduler: 11 actual C++ regression cases; trace/update/composite compile to
  SPIR-V and DXIL. Metal shader emission is compilation evidence only.
- Terrain: 85,920 authority/client comparisons, 466 logs, 1,684 leaves, 642 bushes,
  all four flowers and 206 leaves on signed seams, with removal/replacement and
  unchanged revision-2 checks. See `logs/server/terrain-vegetation-generation.log`
  and the client stream probe.
- Both DX12 and Vulkan passed production block publication/removal/unload checks
  and the ray-geometry edit lifecycle oracle, with no graphics validation
  warnings/errors: `logs/client/ddgi-repair/block-lights-*.log` and
  `ray-retention-*.log`. Sparse column scanning also passed all 65,536 block IDs.
- Final packaged high-quality lighting: 900 frames per API, radius 4 (81 columns),
  960×540 Native AA, three actual torch voxels, 16 captures. Both runs exited 0
  with no graphics validation warnings/errors. All 2,048 probes remained valid
  across the sampled frames. Largest mean receiver brightness difference was
  0.167% DX12 / 0.148% Vulkan; largest pairwise p99 was 1.260% / 1.205%.
  These are normalized display-luminance changes over a fixed floor region,
  including convergence and day/night changes, not moving-camera measurements.
- Final Vulkan medium-quality run passed 600 frames with real torches and raster
  local shadows, without hardware local-visibility rays.
- Grass/bush and all four flowers also cast two-sided alpha-tested shadows in
  the existing RT and raster paths. Angled 09:00 sun captures (600 frames,
  1280×720, high RT and medium raster on Vulkan) visibly show each ground shadow;
  evidence is under `logs/client/vegetation-shadows/`. At exact noon the vertical
  crossed cards have almost no projected ground area. Raster filtering softens
  thin stems at the finite shadow-map resolution.
  Each API additionally passed 9,000 rays against 4,500 real atlas texels across
  both card sides: 1,646 opaque hits shadowed and 7,354 transparent-hole rays
  transmitted in any-hit, sun and local-light queries. Logs:
  `logs/client/ddgi-repair/vegetation-ray-{dx12,vulkan}.log`.
- Natural forest GPU captures show generated trees, bushes, flowers, leaf alpha
  cutouts and ground shadows, with zero authored terrain blocks. DX12 and Vulkan
  each passed 600 frames at 1280×720 and 81 columns; captures were inspected.
- Block-catalog policy, native ABI policy, packaged shader comparison and touched
  source line limits passed. No touched source file exceeds 500 lines.

Final lighting/forest captures, JSON measurements and build hashes are under
`logs/client/ddgi-repair-final/`. The final GPU runs used an isolated copy of the
rebuilt bundle to avoid the active game's shared server-log handle. Earlier
failed Vulkan captures retain the unused vertex-output warning that was fixed;
they are not passing evidence. Unrelated time-controls edits in the shared
workspace are not attributed to this repair.

## Limits

DDGI remains one finite-resolution scrolling volume. Thin-wall approximation,
lighting convergence, moving-camera streaming and radius-32 lighting need wider
scene qualification. Replacement AS publication briefly retains prior geometry.
At most 4,096 nearby block emitters are selected, alongside explicit API lights.
Forward player/items remain outside the new lighting/AS path. Linux hardware and
macOS runtime were not tested here. Vegetation does not silently retrofit old
revision-2 saves; a new revision-3 world enables it.
