# Octaryn

Octaryn is an owner-split platform with a native C/C++ core first, plus managed C# gameplay and networking where the boundary is explicit and validated.

Client presentation, server authority, shared contracts, and bundled basegame logic are separate owners.

## Repository Layout

- `octaryn-client/`: windowing, input, rendering, shaders, overlays, future prediction, and client host integration.
- `octaryn-server/`: authoritative simulation, validation, persistence, server ticks, future replication, and transport hosting.
- `octaryn-shared/`: implementation-free contracts, IDs, commands, snapshots, module manifests, capability IDs, host API IDs, scheduling contracts, and validation policy.
- `octaryn-basegame/`: bundled default game module with gameplay rules, content declarations, texture-pack import tools, assets, data, and basegame-specific tools.
- `tools/`: repo-wide build, validation, profiling, launch, and developer operations.
- `cmake/`: build policy, owner target construction, dependency wrappers, platform facts, and toolchains.
- `docs/`: API, architecture, build tooling, validation, and GitHub Pages documentation.

No new top-level `engine/`, `octaryn-engine/`, generic `runtime/`, `common`, `helpers`, or catch-all owner is part of the target layout.

## Documentation

- [Documentation](https://zsg-studios.github.io/Octaryn/)
- [API Reference](https://zsg-studios.github.io/Octaryn/api/)
- [Examples](https://zsg-studios.github.io/Octaryn/api/examples/)
- [Architecture](https://zsg-studios.github.io/Octaryn/architecture/)
- [Build Tooling](https://zsg-studios.github.io/Octaryn/build/)
- [Validation](https://zsg-studios.github.io/Octaryn/validation/)
- [Texture Packs](https://zsg-studios.github.io/Octaryn/texture-packs/)
- [Third-Party Notices](docs/THIRD_PARTY_NOTICES.md)

Page sources live under `docs/`.

## Build

Configure and build the owner-target scaffold:

```sh
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux
```

The build helpers are the public entrypoints. On Linux, if host CMake is not
available they route the action through the Octaryn Podman builder; if Podman is
missing, they run the repo-managed Linux host setup first. Compilers, CMake,
Ninja, .NET, and graphics/audio build libraries stay inside the Arch builder
image.

Build a specific owner target:

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_client_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_server
tools/build/cmake_build.sh debug-linux --target octaryn_basegame
tools/build/cmake_build.sh debug-linux --target octaryn_shared
```

Run owner launch probes through the public helper targets:

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_validate_owner_launch_probes
tools/build/cmake_build.sh debug-linux --target octaryn_run_client_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_run_server_launch_probe
```

Run managed validation probes directly when you need the same checks CMake uses:

```sh
dotnet run --project tools/validation/Octaryn.ModuleManifestProbe/Octaryn.ModuleManifestProbe.csproj --configuration Debug -- octaryn-basegame
dotnet run --project tools/validation/Octaryn.OwnerModuleValidationProbe/Octaryn.OwnerModuleValidationProbe.csproj --configuration Debug
```

Windows is a Linux-hosted cross-build path. Use the Linux launcher and `tools/build/podman_build.sh` for `debug-windows` or `release-windows`.

## Validation

Use focused validation for the area you changed, then run the aggregate target before broad changes are considered ready:

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_validate_all
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_owner_boundaries
tools/build/cmake_build.sh debug-linux --target octaryn_validate_module_source_api
```

Performance-sensitive work needs runtime evidence, not only a clean build. Keep logs under `logs/<owner>/`, confirm bounded client chunk streaming and mesh upload batches, and verify server persistence only writes authoritative edit data.

## Texture Packs

Texture packs are imported through basegame-owned tooling, declared as module assets, and rendered by the client. Shared only exposes the manifest contract.

Current atlas import path:

```sh
python3 octaryn-basegame/Tools/build_atlas_from_pack.py \
  --pack /path/to/resource-pack.zip \
  --sha256 PACK_SHA256 \
  --pack-name "PACK_NAME" \
  --pack-source-url PACK_SOURCE_URL \
  --pack-license-name "PACK_LICENSE" \
  --pack-license-url PACK_LICENSE_URL \
  --pack-license-file "path/to/included/license.txt" \
  --pack-credit "PACK_AUTHOR_OR_TEAM" \
  --pack-credits-url PACK_CREDITS_URL \
  --cache-dir build/dependencies/texture-packs \
  --output build/debug-linux/basegame/assets/atlases/basegame-color.png \
  --normal-output build/debug-linux/basegame/assets/atlases/basegame-normal.png \
  --specular-output build/debug-linux/basegame/assets/atlases/basegame-specular.png \
  --animation-output build/debug-linux/basegame/assets/atlases/basegame-animation.png \
  --animation-manifest build/debug-linux/basegame/assets/atlases/basegame-animation.txt \
  --tile-size 32 \
  --layer-count 29
```

The tool records pack source, credit, license, and license-file metadata in the generated atlas sidecar. Third-party packs need license permission before assets are committed, packaged, or published.

If the default pack source is used, the imported work is Classic Faithful 32x Jappa under the Faithful License Version 3. That license requires clear credit, a link back to Faithful, no monetization of content containing their work, no misleading official branding, and the unmodified license file included with any distributed content that contains their work.

## Output Layout

- CMake driver state: `build/<preset>/cmake/`
- Owner products: `build/<preset>/<owner>/`
- Managed outputs: `build/<preset>/<owner>/managed/`
- Managed intermediates: `build/<preset>/<owner>/managed-obj/`
- Native binaries: `build/<preset>/<owner>/native/bin/`
- Native libraries: `build/<preset>/<owner>/native/lib/`
- Logs: `logs/<owner>/`, `logs/build/`, and `logs/tools/`

## API Direction

`octaryn-shared/` is the API boundary. It exposes contracts that client, server, and game modules can share without leaking implementation:

- module manifests and validation reports
- capability and host API IDs
- host scheduling declarations
- frame, tick, and time contracts
- command and snapshot message shapes
- world IDs, positions, block edits, and chunk snapshots

Game modules compile against `octaryn-shared` plus explicitly approved package and framework API groups. They do not receive direct filesystem, networking, process, reflection, native interop, raw threading, or client/server implementation access unless a future allowlist grants a narrow contract.

Basegame is the first bundled game module, not a privileged internal bucket. Future games and mods should use the same manifest, capability, scheduling, and validation path.

## Threading Model

The host owns all threading:

- one main thread
- one coordinator thread
- a worker pool with at least two workers that can scale to available cores

Gameplay systems run through coordinator-scheduled work and declared read/write access. Modules must not create raw threads, private schedulers, timers, or worker pools.

## Ownership Rules

- client presentation/rendering stays in `octaryn-client/`
- server authority, simulation, persistence, and validation stay in `octaryn-server/`
- product gameplay and content stay in `octaryn-basegame/`
- contracts stay implementation-free in `octaryn-shared/`
- repo-wide developer operations stay in `tools/`
- build policy and toolchains stay in `cmake/`

Behavioral changes should be explicit. Temporary compatibility paths, duplicate wrappers, and dead code should be removed as owners are cleaned up.
