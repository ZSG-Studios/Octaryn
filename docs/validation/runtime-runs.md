# Runtime runs

The client starts an interactive world and supervised authoritative local server.
It does not first open a multiplayer/world-selection frontend. Internet/LAN
transport is not integrated.

For native Windows, build and launch through `tools/build/windows.py`. The
executable is `build/release-windows/client/bundle/Octaryn.Client.exe`.
`OCTARYN_CLIENT_GRAPHICS_API` selects `dx12` or `vulkan`; unset uses the native
default. Linux and Metal need independent native runtime qualification.

## Isolated checks

Set `OCTARYN_CLIENT_WORLD_PATH` to an absolute scratch-world path. Keep the user's
saved world/settings separate from tests. Close the client gracefully and verify
server readiness, shutdown and final persistence; a live process alone is not
proof of successful session startup.

- `--frames 300`: bounded interactive rendering.
- `--diagnostic`: explicit finite renderer diagnostic.
- `--benchmark-seconds 15`: fixed-view measurement after residency and warmup.
- `OCTARYN_CLIENT_CAPTURE_PATH`: absolute BMP output for renderer readback after
  the requested neighborhood becomes resident.

Client metrics are under `logs/client`, server session logs under `logs/server`.
A relocated package uses the platform's application-data directory. Read the
actual image and relevant validation messages; a produced file alone is not
visual correctness. Retain API/build/configuration metadata with every report.

## Scope

Measure cold load, boundary completion, settled rendering and moving-center
streaming separately. A short stationary benchmark does not establish travel
performance, long-session resource bounds, platform parity or multiplayer.
See [validation](README.md) and [profiling](tracy.md).
