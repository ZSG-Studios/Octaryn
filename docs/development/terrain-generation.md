# Terrain generation

## Natural vegetation and revision 3

The only built-in generator is natural terrain revision 3. `TerrainVegetation.h` supplies the same
deterministic tree, bush and flower stage to server collision/edit queries and
client reconstruction. Tree candidates use seed-1337 world-coordinate cells;
forest climate increases tree density. The canopy silhouette follows the
recovered original feature implementation, while population uses Octaryn's
current climate and coordinate hash. It is not Pumpkin/Minecraft seed parity.

Bulk generation fills terrain first, then evaluates anchors in a one-block
neighbor halo. Scalar queries evaluate the same anchors, including negative
coordinates and chunk edges. Vegetation cannot replace terrain or water;
generated trunks take priority over leaves, and authoritative edits, including
air removals, apply last. Generated vegetation remains transient seed data.

Normal startup uses `saves/open-world-v3`. The old `open-world-v2` default was
the cause of missing vegetation in ordinary play. Revision-2, flat and empty
generation paths are removed; older saves remain on disk and are rejected
without changing their metadata or edits. The active world revision travels
through native generation rules and stream snapshots and participates in client
cache identity. Save import/export must preserve that revision. The obsolete
chunk-local feature recipe is removed; the world-coordinate vegetation stage is
the single built-in feature implementation.

The focused client stream probe covers revision-3 acceptance and legacy rejection, authority/bulk agreement
through the vegetation band, signed canopy seams, all flower IDs, preserved
solid/water cells, tree removal/replacement and shuffled regeneration. Runtime
and platform evidence remains separate from these deterministic checks.

## Pumpkin research

Inspected on 2026-09-13 from `ref/Pumpkin`, commit
`927b7fccd734e33c18c2c409e4caa61c4696de44`. This is a reference for designing
Octaryn's own generator. It does not establish Minecraft seed parity or runtime
performance. Pumpkin's GPL-3.0 source stays in its separate reference checkout;
the Octaryn implementation must use original code and its existing owners.

The useful distinction is between terrain shape, climate, materials, and later
features. Combining more octaves into one height map does not reproduce that
pipeline.

| Responsibility | Verified reference behavior | Pinned source |
| --- | --- | --- |
| Density composition | Seeded noise, shifted noise, splines, arithmetic, gradients and range selection compose the density graph. A prepared router is adapted into chunk-local evaluators. | [proto_noise_router.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/noise/router/proto_noise_router.rs) |
| Sampling cost | Cache wrappers reuse coordinate/volume results; interpolation has separate horizontal and vertical cell sizes. Density buffers use a capped thread-local pool. These are implementation mechanisms, not measured speed claims. | [chunk_density_function.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/noise/router/chunk_density_function.rs), [density_volume.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/noise/router/density_volume.rs) |
| Climate | Six channels sample temperature, humidity, continentalness, erosion, depth and weirdness. The sampler supports scalar queries and prefilled volumes. Biome coordinates are converted to block coordinates consistently. | [multi_noise_sampler.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/noise/router/multi_noise_sampler.rs) |
| Aquifers | Nonpositive-density locations consult nearby deterministic fluid sites. Differences in site fluid levels and density barriers can preserve rock between fluid bodies. Floodedness/spread noise and estimated surface height influence fluid levels. Fluid-update scheduling is separate from choosing a block. | [aquifer_sampler.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/noise/aquifer_sampler.rs) |
| Carvers | Cave generation creates rooms and seeded branching tunnels using ellipsoid carving; the canyon implementation is separate. Carvers write through a bounded output abstraction and consult carve/material rules. These are distinct from density-noise caves. | [cave.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/carver/cave.rs), [carver/mod.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/carver/mod.rs) |
| Surface materials | Rules use biome, temperature, steepness, water, vertical gradients, noise, and stone depth above/below. Surface treatment follows solid density population instead of defining all geometry through a grass-height threshold. | [surface/mod.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/surface/mod.rs), [surface/rule.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/surface/rule.rs) |
| Stage boundaries | Chunk generation exposes separate biome, noise, surface, carver and feature stages. Surface evaluation requires biome neighborhood coverage, demonstrating that a chunk boundary must not become an algorithm boundary. | [proto_chunk.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/proto_chunk.rs), [generator/mod.rs](https://github.com/Pumpkin-MC/Pumpkin/blob/927b7fccd734e33c18c2c409e4caa61c4696de44/crates/pumpkin-world/src/generation/generator/mod.rs) |

This sparse inspection covers algorithm implementations, not the generated
`pumpkin-data` biome tables, full Minecraft density presets, assets, or every
feature stage. Do not describe selected lessons as a complete Pumpkin port.

## Applying the design to Octaryn

Keep the deterministic generation kernel with basegame terrain ownership so
the authoritative server and client reconstruction evaluate identical seed
terrain. Keep persistence, edits, networking, GPU meshing and scheduling in their
existing owners. Generated blocks remain transient; authoritative overrides,
including edited air, apply after generation.

Use independently seeded world-coordinate fields for broad land/ocean shape,
erosion, ridge character and climate. Compose landform elevation and 3D density
separately so overhangs and subsurface voids are representable. Material selection
should use climate, coast/river context and local exposure, while fluids should
have an explicit rule instead of filling every underground empty voxel.

Cache horizontal fields once per column and evaluate expensive 3D fields on a
world-aligned lattice where interpolation preserves the chosen appearance.
Scalar server queries and bulk client generation must share the same lattice
and rounding rules, including negative coordinates. Chunk order and worker
count must not affect results. Keep caches bounded by active generation work.

Generator changes also change the terrain under edit-only saved worlds. Record
generator identity in the world/session contract or explicitly reject a
mismatched identity; preserving a seed alone does not preserve terrain.

Acceptance requires exact scalar/bulk comparison over signed coordinates and
chunk boundaries, reproducibility, measured full-column
generation cost, visible landforms and caves, material/fluid invariants, and
actual server/client reconstruction agreement after edited-air overrides.
Renderer builds, live visuals, persistence and platform qualification must be
reported separately. Runtime implementation and verification should be recorded
below when they are completed; this research section alone claims neither.

## Terrain foundation

The native basegame terrain foundation consists of three focused headers under
`octaryn-basegame/Source/Gameplay/Terrain`:

- `TerrainNoise.h`: deterministic coordinate hashing, quintic value noise and
  independently seeded octave fields. Double coordinates preserve interpolation
  at large positive and negative world coordinates.
- `TerrainColumn.h`: warped continentalness, erosion, ridges and river valleys;
  temperature/humidity and altitude select ocean, beach, plains, forest, desert
  or alpine classifications. Horizontal fields are sampled once per column.
- `TerrainDensity.h`: chamber and intersecting tunnel density, four solid bottom
  layers, an eight-block protected roof, four layers of surface fill over stone,
  and explicit sea-level water above submerged terrain. Underground voids are air.

The server's existing terrain C ABI and client `WorldStream/GenerateColumn.cpp`
call this same block sampler. Authority collision queries and edit cleanup use
the server callback. Client blocks feed the existing GPU occupancy/meshing path;
authoritative overrides, including removed blocks represented by air, apply last.
The existing bounded stream queue and owner boundaries remain in place.

Managed `TerrainColumnSample` now carries already sampled height/climate rather
than the old three-noise recipe. Basegame material planning matches the native
thresholds. Vegetation placement uses the shared native stage; the unused managed
feature recipe is removed. Biome and feature JSON schema v2 describes all six
compiled biomes, their selection order and material overrides, and the current
tree/bush/flower rules. These descriptors are not runtime tuning input; the content
validator rejects stale generator revisions, recipes, and biome lists.

The generator retains the engine's fixed seed 1337. It is an original Octaryn
height envelope with enclosed 3D caves, not Minecraft/Pumpkin seed parity.
Exposed overhangs, cave entrances, aquifers, ore veins and river flow simulation
are not implemented. Forest climate drives tree density through the shared
vegetation stage described above. Configurable seeds and
cross-platform numerical reproducibility need separate implementation/qualification.

## World compatibility

Server-owned `world_generation.json` records generator, revision, seed and mode
before any overrides are loaded or cleaned. Only natural revision 3 is accepted;
older identities fail before any terrain-dependent edit cleanup occurs.
Missing identity is accepted only for a new directory without saved artifacts;
incompatible or unversioned existing saves are rejected without rebasing edits.
Keep unversioned saves for explicit migration; do not invent identity metadata.

Stream schema 2 carries generator mode/revision; the client rejects mismatched
versions or modes. Save bundle schema 2 embeds and validates the natural-world
identity before import writes. Old bundles and flat/empty exports are rejected;
flat and empty native generation no longer exist.

From `C:\Users\Rose-X\Documents\Octaryn`, open a new revision-3 world:

```powershell
python tools/build/windows.py --action run-client --preset release-windows
```

## Historical revision-2 verification on Windows

Native and managed client/server bundles built successfully through the existing
Windows entrypoint. Direct native terrain, client stream and persistence probes
passed, as did full managed world-generation and persistence probes and the
worldgen-content, native ABI, native owner-boundary and CMake-policy validators.

- 139,392 volume samples: cave voids, deep stone, water and protected surface/bottom.
- 1,694 shuffled samples: deterministic results, signed integer extremes and bounds.
- 24,480 exact client/authority comparisons: signed neighboring chunks and vertical
  boundaries, whole-column regeneration in shuffled order, edited air and placements.
- 20 full columns (32 x 512 x 32): mean 18.26 ms, maximum 23.14 ms in the recorded
  release probe. This is a local measurement, not a platform-wide performance promise.
- 1,050,625 diagnostic columns over x/z [-2048, 2048], spacing 4: all six biomes,
  heights 3..193. The x/y section at z=0 contains 23,686 cave voxels.
- Packaged standalone slang-rhi/Vulkan client launched from `C:\Windows` with a
  fresh save: 81 full-depth columns, 4,286,930 GPU faces, 98,088,560 GPU bytes,
  300 fully resident frames and `open_world_exit code=0`. RHI debug validation
  was enabled; its log contains no warnings/errors. This does not independently
  establish Vulkan Core/Synchronization layer enablement or all renderer parity.
- Reopened the same revision-2 save successfully: player state loaded, zero
  generated block records persisted or streamed as edits, 81 columns reconstructed,
  8,139 fully resident frames, and process exit 0. At 1280 x 720 in the fixed-view
  benchmark, the reported mean was 1.195 ms over 3,952 measured frames after warmup.
  The first capture used 2560 x 1440; those runs are not a resolution-matched comparison.

Evidence is under `logs/server/terrain-*.log` and `logs/client/terrain-*.log`.
`logs/client/terrain-v2-world.bmp` is an actual renderer capture; the maps under
`logs/server/terrain-diagnostic/` are direct sampler diagnostics, not screenshots.
Recreate those maps and coverage report with:

```powershell
python tools/validation/terrain-diagnostic.py
```

Build and run the focused native checks directly (no ctest):

```powershell
python tools/build/windows.py --action build --preset release-windows --target octaryn_server_terrain_generation_probe octaryn_client_world_stream_probe octaryn_server_world_persistence_probe
$env:PATH = (Join-Path (Get-Location) 'build/release-windows/server/native/bin') + ';' + $env:PATH
.\build\release-windows\tools\native\bin\octaryn_server_terrain_generation_probe.exe
.\build\release-windows\tools\native\bin\octaryn_client_world_stream_probe.exe
.\build\release-windows\tools\native\bin\octaryn_server_world_persistence_probe.exe
```
