#!/usr/bin/env bash
set -euo pipefail

preset="${1:-debug-linux}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
client_app="${repo_root}/build/${preset}/client/bundle/Octaryn.Client"
routes="${OCTARYN_CLIENT_MOVEMENT_PROBE_ROUTES:-straight-after-edits diagonal-box wide-box reverse-box long-run}"

"${repo_root}/tools/build/cmake_build.sh" "${preset}" --target octaryn_client_bundle

mkdir -p "${repo_root}/logs/client"

for route in ${routes}; do
  log_path="${repo_root}/logs/client/octaryn_client_movement_stream_probe-${preset}-${route}.log"
  session_root="${repo_root}/build/${preset}/client/validation/client-movement-stream-session-${route}"
  settings_path="${session_root}/client-settings.json"

  rm -f "${log_path}"
  rm -rf "${session_root}"
  mkdir -p "${session_root}"
  cat > "${settings_path}" <<'JSON'
{
   "version": 8,
   "fogEnabled": false,
   "fullscreen": false,
   "displayName": "",
   "displayIndex": 0,
   "displayModeWidth": 0,
   "displayModeHeight": 0,
   "displayModeRefreshRate": 0,
   "cloudsEnabled": false,
   "skyGradientEnabled": false,
   "windowWidth": 1280,
   "windowHeight": 720,
   "renderDistance": 32,
   "starsEnabled": false,
   "sunEnabled": false,
   "moonEnabled": false,
   "pomEnabled": false,
   "pbrEnabled": false,
   "presentModeIndex": 0
}
JSON

  OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES="${OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES:-0}" \
  OCTARYN_CLIENT_APP_EXIT_AFTER_SECONDS="${OCTARYN_CLIENT_APP_EXIT_AFTER_SECONDS:-20}" \
  OCTARYN_CLIENT_APP_INPUT_PROBE=1 \
  OCTARYN_CLIENT_APP_MOVEMENT_PROBE=1 \
  OCTARYN_CLIENT_APP_MOVEMENT_PROBE_ROUTE="${route}" \
  OCTARYN_CLIENT_APP_PROFILE_PHASES=1 \
  OCTARYN_CLIENT_APP_VALIDATE_PIXELS=1 \
  OCTARYN_CLIENT_APP_LOG_PATH="${log_path}" \
  OCTARYN_CLIENT_SETTINGS_PATH="${settings_path}" \
  OCTARYN_CLIENT_SINGLEPLAYER_SESSION_ROOT="${session_root}" \
  OCTARYN_CLIENT_SERVER_STREAM_MESH_COLUMNS_PER_FRAME="${OCTARYN_CLIENT_SERVER_STREAM_MESH_COLUMNS_PER_FRAME:-1}" \
  "${client_app}"

  python3 "${repo_root}/tools/validation/validate_client_movement_stream_probe_log.py" \
    --log-file "${log_path}" \
    --server-log "${session_root}/server_live.log"

  python3 "${repo_root}/tools/validation/validate_client_post_edit_streaming_log.py" \
    --log-file "${log_path}" \
    --server-log "${session_root}/server_live.log" \
    --route "${route}"
done
