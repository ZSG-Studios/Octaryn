# Working state — 2026-09-13

The original Octaryn engine has been restored to this repository root.
Development now focuses on repairing this engine in its existing owner layout.
The recent wholesale rewrite plan is superseded.

The complete previous workspace, including networking/interpolation fixes,
profiling evidence, dependencies, build outputs, and Git metadata, is preserved:

C:\Users\Rose-X\Documents\Octaryn-Backups\2026-09-13-before-old-engine\workspace

- [Current repairs and next integration steps](docs/development/repair-progress.md)
- [Feature parity and actual runtime gaps](docs/development/feature-parity.md)
- [Restoration, verification, and first build repairs](docs/development/restoration.md)
- [Networking fixes to recover](docs/development/networking-recovery.md)
- [Active working rules](AGENTS.md)

The Windows client now launches an interactive world with a local authoritative
server, streamed terrain, mouse/keyboard controls, and frame profiling.
[Run commands, controls, and remaining scope](docs/development/open-world.md).
Its renderer uses Slang shaders through standalone Slang RHI. Windows defaults
to DX12; Vulkan is also runtime-validated on Windows. Metal shader source is
generated, but native macOS/Linux execution remains unqualified.

The current client includes RmlUi inventory and creative block menus, server-owned
item drops/pickups, and FSR 2.2.1 with Native AA, quality presets, custom scale,
sharpening and GPU-timed dynamic resolution. Terrain streaming uses exact cave
noise caching without LOD or geometry reduction.

- [Current source update and qualification](docs/development/github-source-update.md)
- [FSR player settings](docs/development/fsr-player-settings.md)
- [Presentation integration](docs/development/presentation-integration.md)
- [Terrain streaming improvements](docs/development/terrain-streaming-cache.md)

The archived commands and documentation below are historical reference and
require platform validation before being described as working on this PC.

---
# Octaryn

Octaryn is an owner-split game platform with a native C/C++ core first and
managed C# gameplay where the API boundary is explicit and validated.

## Documentation Center

GitHub Pages publishes from `docs/`:

- Documentation: <https://zsg-studios.github.io/Octaryn/>
- API: <https://zsg-studios.github.io/Octaryn/api/>
- Architecture: <https://zsg-studios.github.io/Octaryn/architecture/>
- Build Tooling: <https://zsg-studios.github.io/Octaryn/build/>
- Validation: <https://zsg-studios.github.io/Octaryn/validation/>
- Texture Packs: <https://zsg-studios.github.io/Octaryn/texture-packs/>

Keep detailed architecture, build, validation, API, runtime, and content notes
in the documentation center instead of this README.

## Repository Map

- `octaryn-client/`: presentation, input, rendering, shaders, overlays, and client host code.
- `octaryn-server/`: authority, validation, persistence, simulation, ticks, and server host code.
- `octaryn-shared/`: implementation-free contracts, IDs, commands, snapshots, manifests, and validation policy.
- `octaryn-basegame/`: bundled default game module, gameplay rules, content, assets, data, and basegame tools.
- `tools/`: repo-wide build, validation, profiling, launch, and developer operations.
- `cmake/`: build policy, owner targets, dependencies, platforms, and toolchains.
- `docs/`: GitHub Pages documentation source.

## Common Commands

```sh
tools/build/cmake_configure.sh debug-linux
tools/build/cmake_build.sh debug-linux --target octaryn_client_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe
```

Use the build helpers as the public entrypoints. They handle the repo-managed
build environment and avoid manual dependency setup.
