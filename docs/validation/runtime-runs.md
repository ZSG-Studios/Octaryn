# Runtime Runs

Runtime validation should use direct executable launches and focused logs, not generic wrapper-only checks.

## Current New-Architecture Runtime Artifact

The current root build produces managed client and server bundles:

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_client_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_server_bundle
```

The bundles are staged under `build/debug-linux/client/bundle/` and `build/debug-linux/server/bundle/` with owner assemblies, `Octaryn.Basegame.dll`, `Octaryn.Shared.dll`, runtimeconfig/deps files, approved runtime dependencies, and the client-owned native graphical launcher at `build/debug-linux/client/bundle/Octaryn.Client`.

## Transitional Native Runtime

The old native runtime remains under `references/old-architecture/` as source material and transitional host validation only. Do not run old-architecture targets as part of normal new-architecture validation unless a task explicitly touches that bridge.

## Acceptance Signals

- Client bundle contains `Octaryn.Client.runtimeconfig.json`.
- Native bridge validation resolves all required client/server managed exports through hostfxr before the first owner frame or tick.
- Bridge facades no longer return not-loaded status after successful initialization; invalid inputs must reach the managed validation paths.
- Bundled singleplayer server readiness runs with `tools/build/cmake_build.sh debug-linux --target octaryn_client_server_app_launch_probe`, requires the server app to emit `octaryn_server_ready=1` after activation and one host tick, emit `octaryn_server_shutdown=1` after disposal, initialize `build/debug-linux/server/validation/client-server-app-launch-probe-world/world_blocks.json`, and log under `logs/server/octaryn_client_server_app_launch_probe-debug-linux.log`.
- Direct owner launch probes run with `tools/build/cmake_build.sh debug-linux --target octaryn_validate_owner_launch_probes`.
- Individual owner probe helpers run with `tools/build/cmake_build.sh debug-linux --target octaryn_run_client_launch_probe` and `tools/build/cmake_build.sh debug-linux --target octaryn_run_server_launch_probe`.
- Graphical client launcher probe runs with `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe`, opens the SDL client window through the native launcher with a two-frame limit, applies the validation-only presentation snapshot, validates native-drained block presentation plus SDL readback-visible clear/block pixels in the log, initializes/ticks/shuts down the managed client host bridge, and logs under `logs/client/octaryn_client_app_launch_probe-debug-linux.log`.
- `Octaryn.Client.dll` remains the managed client host payload. The directly runnable graphical client artifact is the native `Octaryn.Client` executable staged in the client bundle.
- Client launch probe logs under `logs/client/octaryn_client_launch_probe-debug-linux.log`:

```text
crash_marker=/tmp/octaryn-crash-...
tick_before_initialize=-1
apply_server_snapshot_before_initialize=-1
drain_presentation_updates_before_initialize=-1
initialize=0
tick=0
apply_server_snapshot=0
drain_presentation_updates=0 count=1 x=-4 y=5 z=6 block=7
drain_presentation_updates_empty=0 count=0
apply_server_snapshot_invalid=-2
reinitialize=0
tick_after_reinitialize=0
shutdown=0
```

- Server launch probe logs under `logs/server/octaryn_server_launch_probe-debug-linux.log`:

```text
crash_marker=/tmp/octaryn-crash-...
tick_before_initialize=-1
initialize=0
tick=0
reinitialize=0
tick_after_reinitialize=0
submit_client_commands=0
submit_client_commands_set_block_array=0
tick_after_submit=0
submit_client_commands_invalid=-1
drain_server_snapshots=0
drain_server_snapshots_block_changes=1
drain_server_snapshots_empty=0
shutdown=0
```

- Failed hostfxr load, missing runtimeconfig/deps, missing export, ABI version mismatch, and managed initialization failure should be logged under `logs/client/` or `logs/server/` when real owner-native runtime launchers replace the probes.
- Root CMake bundle rebuilds are dirty-correct.
- Direct runtime launch checks should continue to record logs under owner-specific log paths as probes graduate into real client/server runtime targets.
