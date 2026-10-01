# Tiled map residency

The tiled path is selected by a version-1 map manifest containing matching
`tiles` bounds and `tile_files` arrays. The existing monolithic map path remains
the native comparison reference. Geometry uses baked world coordinates; each
resident tile has its own geometry, materials and compact static BLAS. A shared
texture pool deduplicates matching cooked image identities across tile owners.
`texture_cache` optionally names a shared relative cooked-texture directory.

`WorldStreaming/TileSession` owns preparation, upload admission, publication and
eviction. Two CPU jobs parse assets and prepare independent Box3D mesh BVHs.
One staged GPU builder advances per frame. Uploads are submitted through the
normal frame encoder and their final fence must complete before publication.
Static BLAS creation and compaction also complete before ray-enabled publication.
Collision installation precedes visibility. The renderer's published map vector
and ray snapshots retain shared tile owners until their GPU consumers retire.

The default camera load/keep radii are 128/160 metres. The real player anchor
adds a 24-metre protected region with an eight-metre retention margin. Camera
fixtures cannot move that anchor. Jobs outside the wanted region are cancelled;
already submitted uploads finish safely before their result is discarded.
Eviction removes only unneeded collision and keeps GPU ownership until the last
submitted frame fence completes and retained ray snapshots release the tile.
The application drains the queue and releases
ray snapshots before destroying a session or switching maps.

## Budgets and present limits

- GPU staging has an enforced eight-MiB byte budget per pump and a one-ms soft
  CPU budget checked between upload operations. A driver allocation can exceed
  that time; the limit is not a preemption guarantee.
- Tiled sources are limited to 64 MiB, 131,072 triangles and 2,048 primitives per
  tile. BLAS construction advances one tile per frame; hardware cost still needs
  measurement on representative tiles.
- GPU admission queries actual backend BLAS/TLAS build sizes from prepared
  geometry before allocating tile GPU buffers. A second check before the ray build
  covers build scratch and simultaneous source/compacted BLAS ownership. Ready,
  retired and reserved geometry are counted with shared texture payloads once.
  This is conservative ownership accounting, not driver residency or heap overhead.
  `OCTARYN_CLIENT_TILE_GPU_BUDGET_MIB` accepts 64 through 32,768 MiB; the default
  is 2,048 MiB. An insufficient ray working set fails with required/budget bytes.
- Prepared asset data is limited to 256 MiB per job. Parser, decoder and collision
  BVH scratch are additional transient allocations.
- Build10 authority evidence uses preload-all collision (`authority_streaming=0`).
  New server source adds bounded collision residency with player/awake-item
  protections; it still requires separate runtime qualification. Camera-only
  captures report the isolated server's actual residency marker and cannot prove
  authoritative movement, eviction or readiness under transport loss.
- A camera-only route qualifies renderer residency, not authoritative movement,
  prediction under network loss, or a complete large-world release.

## Reproducible evidence

`tools/validation/validate_tile_fixture.py` compiles the production GLB loader and
tile parser against 17 generated 24-metre cells. It checks all geometry loads,
material image identity, winding and malformed manifests without using a GPU.

`tools/validation/capture_tile_world.py` drives the bundled client with a bounded,
hidden, watchdog-supervised camera route spanning 384 metres. The real authority
anchor remains at spawn. Initial cuts exercise cancellation; traversal and holds
exercise publication, hysteresis and eviction. Its result records actual staged
bytes, CPU upload durations, all published tile IDs and settled ownership samples.
Use `--seconds 1800 --timeout 2100` for a 30-minute rendering route. Run DX12 and
Vulkan separately and inspect the generated image and logs.

Pass `--manifest path/to/map.json` to use an existing tiled world without copying
its assets. `--gpu-budget-mib` selects an explicit ownership budget. The capture
records manifest, GLB, LOD companion and shared cooked-metadata hashes. Hashing can
warm filesystem caches, so these runs do not establish cold loading time. The
384-metre route remains unchanged; results identify frames outside manifest bounds.
Generated-fixture coverage assertions do not apply to arbitrary real worlds.

The first tiled runtime exposed a nested same-executor wait in NativeJobs with
two saturated workers. The cooperative worker execution repair must be present
before accepting runtime results. Clean compilation and CPU fixture success do
not establish GPU correctness or memory stability.
