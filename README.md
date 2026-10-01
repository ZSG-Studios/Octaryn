# Octaryn

Octaryn is the native C/C++ and managed C# game development platform maintained
at [ZSG-Studios/Octaryn](https://github.com/ZSG-Studios/Octaryn).
The name of a local checkout directory does not change the project identity.

The platform includes GLB/glTF map worlds, custom virtual geometry, ray-traced
shadows and reflections, a world library with independent saves, retained UI,
client audio, Box3D character motion, and dedicated-server authority.

## Renderer direction

The active rewrite moves GPU execution from standalone slang-rhi to NVRHI,
while retaining Slang shader authoring and the custom geometry pipeline.
DX12 and Vulkan are the first targets; macOS/Metal is deferred.

The [NVRHI rewrite plan](docs/development/nvrhi-renderer-rewrite.md) is the
persistent implementation and qualification checklist. It remains active until
its completion gates pass. The current renderer still uses slang-rhi.
Full Zorah loading and AAA workload performance are not yet qualified; see
[scene geometry scaling](docs/development/scene-geometry-scaling.md).

## Native Windows development

Use native Windows with Visual Studio C++ tools, the Windows SDK, .NET SDK,
Python and Git. The build scripts provision the pinned CMake/Ninja tools.
Restore the local Bistro fixture before building its bundle; see the
[map source instructions](octaryn-client/Assets/Maps/README.md). Large imported
scenes are distributed separately from the source repository.
While the current renderer remains active, prepare its graphics dependency,
configure, and build with:

```powershell
python tools/build/windows.py --action rhi --preset release-windows
python tools/build/windows.py --action configure --preset release-windows
python tools/build/windows.py --action build --preset release-windows --target octaryn_all
```

Launch the packaged client or dedicated server:

```powershell
python tools/build/windows.py --action run-client --preset release-windows
python tools/build/windows.py --action run-server --preset release-windows
```

Ordinary client startup opens the world library. Add or locate a GLB/glTF source,
choose its world and save, and load it through the loading screen. Large sources
can require preparation before opening. Imported source payloads and personal
saves remain local. See [world library](docs/development/world-library.md).

## Owners and verification

- `octaryn-client`: presentation, rendering, UI, audio and local prediction.
- `octaryn-server`: authority, simulation, saves, collision and hosting.
- `octaryn-shared`: contracts and focused native libraries.
- `octaryn-basegame`: bundled gameplay and product UI declarations.
- `cmake`, `tools`, `docs`: build policy, developer operations and documentation.

Build outputs belong under `build/<preset>/<owner>` and logs under `logs/<owner>`.
The verification standard is `octaryn_all`, a map smoke with stable authoritative
pose, inspected GPU captures, and listen/connect checks for networking changes.
Qualify graphics backends separately and keep incomplete work explicit.
