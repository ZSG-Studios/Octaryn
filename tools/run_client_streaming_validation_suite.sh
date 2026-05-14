#!/usr/bin/env bash
set -euo pipefail

preset="${1:-debug-linux}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

targets=(
  octaryn_validate_cmake_targets
  octaryn_validate_client_chunk_mesh_plan_probe
  octaryn_validate_client_empty_world_mesh_probe
  octaryn_validate_server_player_simulation_native_probe
  octaryn_validate_server_block_store_native_probe
  octaryn_validate_server_world_blocks_probe
)

for target in "${targets[@]}"; do
  "${repo_root}/tools/build/cmake_build.sh" "${preset}" --target "${target}"
done

"${repo_root}/tools/run_client_movement_stream_probe.sh" "${preset}"
