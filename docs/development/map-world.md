# GLB map world

The engine's default world is generated voxel terrain streamed from the
authoritative server. Map world mode replaces that content path: a
Blender-exported glTF 2.0 file (`.glb` or `.gltf`) becomes the playable world,
rendered as raw triangles with the existing HDR lighting pipeline, while the
server keeps authority over the player pose using Jolt triangle-mesh
collision. Voxel terrain generation, block gameplay, fluids and streamed world
items are bypassed in this mode.

## Selection and assets

A client bundle plays the map world when it ships `Assets/Maps/map.json` at the
game module content root (the file lives in `octaryn-basegame/Assets/Maps/`
and is declared in the module manifest):

```json
{
  "version": 1,
  "map": "main.glb",
  "spawn": [0.0, 1.8, 12.0],
  "yaw": 0.0,
  "pitch": -0.2
}
```

- `spawn` is the **eye position** in glTF world space (+Y up, meters).
- `yaw`/`pitch` are the initial view angles in radians.
- `map` is resolved relative to the manifest file and may not contain path
  separators.

Anything under `octaryn-basegame/Assets/` is bundled with the game module and
declared in `octaryn.basegame.module.json`. The placeholder generator
`octaryn-basegame/Tools/MapPlaceholder/build_placeholder_map.py`
writes a minimal playable map for testing; a Blender export ("glTF 2.0"
format, +Y up) replaces both files directly and must keep the manifest
declarations valid (same file names).

## Runtime flow

1. The client detects the manifest, parses it, and hands the GLB path to
   `open_world_renderer_load_map` right after renderer creation.
2. `LocalSession::start` spawns the bundled server with
   `OCTARYN_SERVER_MAP_MODE=1` and `OCTARYN_SERVER_MAP_MANIFEST_PATH`.
3. The server (`ModuleActivator` map branch) skips terrain generation and the
   world-generation identity check, loads the GLB into a world-space triangle
   soup (`octaryn_server_map_world` native library, fastgltf), and drives the
   authoritative player session against a cached Jolt `MeshShape`
   (`character_motion::step_on_mesh`). Player spawn comes from the manifest.
4. The pose still flows through the existing player-state/chunk-stream
   snapshot channel, so client prediction reconciliation, interpolation and
   session lifecycle are unchanged; the stream simply carries zero columns.
5. The client renders the GLB through `WorldMap.slang` into the same G-buffer
   the voxel terrain uses, so sky, sun, fog, HDR composite, FSR and the
   player avatar all work unchanged. The loading screen waits for the
   authoritative pose plus `map_ready` instead of column residency.

## Ownership

- `octaryn-client/Source/MapWorld/` - GLB import, GPU upload, map draws,
  map acceleration structure.
- `octaryn-client/Source/App/OpenWorld/MapMode.*` - manifest contract.
- `octaryn-client/Source/App/OpenWorld/MapWorldSession.cpp` - map frame loop.
- `octaryn-server/Source/World/MapWorld/` - server GLB load, manifest,
  collision step exports.
- `octaryn-shared/Source/Libraries/CharacterMotion/PlayerJoltMesh.*` -
  shared triangle-mesh walk.
- `cmake/Dependencies/GltfDependencies.cmake` - fastgltf dependency wrapper
  shared by client and server.

## Known slice-1 limits

- Material support is base color (factor + texture), alpha mask/blend and
  roughness/metallic factors; normal/emissive texture maps are not yet bound.
- The ray scene uses one BLAS over the whole map; DDGI probes and RT sun
  shadows resolve map hits through a per-primitive base color path.
- The map world and the voxel world remain selectable content modes; no voxel
  code paths were deleted, so existing voxel qualification runs unchanged.
