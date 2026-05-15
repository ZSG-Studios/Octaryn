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
