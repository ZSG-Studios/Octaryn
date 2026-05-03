# Runtime Runs

Runtime validation should use direct executable launches and focused logs, not smoke-test wrappers.

## Current New-Architecture Runtime Artifact

The current root build produces managed client and server bundles:

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_client_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_server_bundle
```

The bundles are staged under `build/debug-linux/client/bundle/` and `build/debug-linux/server/bundle/` with owner assemblies, `Octaryn.Basegame.dll`, `Octaryn.Shared.dll`, runtimeconfig/deps files, and approved runtime dependencies.

## Transitional Native Runtime

The old native runtime remains under `old-architecture/` as source material and transitional host validation only. Do not run old-architecture targets as part of normal new-architecture validation unless a task explicitly touches that bridge.

## Acceptance Signals

- Client bundle contains `Octaryn.Client.runtimeconfig.json`.
- Native bridge validation resolves all required client/server managed exports through hostfxr before the first owner frame or tick.
- Bridge facades no longer return not-loaded status after successful initialization; invalid inputs must reach the managed validation paths.
- Bundled singleplayer server readiness runs with `tools/build/cmake_build.sh debug-linux --target octaryn_client_server_app_launch_probe` and logs under `logs/server/octaryn_client_server_app_launch_probe-debug-linux.log`.
- Direct owner launch probes run with `tools/build/cmake_build.sh debug-linux --target octaryn_validate_owner_launch_probes`.
- Individual owner probe helpers run with `tools/build/cmake_build.sh debug-linux --target octaryn_run_client_launch_probe` and `tools/build/cmake_build.sh debug-linux --target octaryn_run_server_launch_probe`.
- Client launch probe logs under `logs/client/octaryn_client_launch_probe-debug-linux.log`:

```text
crash_marker=/tmp/octaryn-crash-...
tick_before_initialize=-1
apply_server_snapshot_before_initialize=-1
initialize=0
tick=0
apply_server_snapshot=0
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
