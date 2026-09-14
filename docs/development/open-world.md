# Interactive world

The default native client now starts a continuous SDL window, a supervised local
server, and a bounded terrain stream. The old three-frame rendering diagnostic
is selected explicitly with `--diagnostic`.

From the repository root in PowerShell:

```powershell
.\tools\build\windows.ps1 -Action build
.\tools\build\windows.ps1 -Action run-client
```

Controls: WASD moves, left click captures the mouse, mouse movement looks around,
Space jumps, Ctrl sprints, F/F5 toggles flight, E/Space and Q/Shift move vertically
in flight, Escape opens/closes settings, Z cycles 1/2/4 zoom, F3 toggles the HUD,
and F11 toggles fullscreen. With the mouse captured, left/right click break/place,
middle click picks a block and the wheel cycles selection. Closing the
window requests server shutdown and a final player save.

The default world lives in `saves/open-world`. Set `OCTARYN_CLIENT_WORLD_PATH`
to an absolute path for an isolated validation world. Server logs are in
`logs/server/local-session.log`; client frame samples are in
`logs/client/open-world.csv`. A relocated bundle without the repository uses
the platform's Octaryn application-data directory for saves and logs.

Terrain generator revision 2 requires a new world or matching generator metadata.
Existing unversioned saves are rejected to protect their edits. For the new
landforms and caves, set `OCTARYN_CLIENT_WORLD_PATH` to an absolute
`saves/terrain-v2` path before launching. See
[terrain-generation.md](terrain-generation.md) for commands and verification.

`Octaryn.Client.exe --frames 300` runs a bounded interactive rendering check.
`Octaryn.Client.exe --diagnostic` preserves the original diagnostic path.
`Octaryn.Client.exe --benchmark-seconds 15` measures a fixed initial view after
full residency and five warmup seconds. Gameplay input is disabled during the
measurement. `--show-settings` displays the original menu for CLI-driven capture.
VSync is disabled and there is no active-window frame cap. Minimized windows
yield CPU time while retaining their session.

Set `OCTARYN_CLIENT_CAPTURE_PATH` to an absolute BMP filename to capture the
render target copied into the swapchain after the complete requested neighborhood
loads. A `.quads.bin` sidecar records GPU face data for geometry diagnosis. This
uses renderer readback and requires no app control. The original camera owner's
90-degree vertical field of view is used.

The Windows CSV writer permits concurrent readers. It records frame metrics,
session/stream/render costs, retained/drawn geometry and authoritative pose/time.
The eight slowest measured frames are printed at exit with event/profiling-tail
attribution. Live metrics also appear in the original HUD and window title.
CPU/GPU utilization and memory providers are not connected and display N/A.

## Scope

- Native server/Jolt owns player motion. Client input uses latest-state commands
  with a stale-input timeout. Presentation uses monotonic source timestamps,
  bounded history, one-times playback, and hold/refill on underrun.
- The local process-file protocol is connected; internet multiplayer transport
  is not implemented by this integration.
- A bounded session I/O worker owns pose, input, window and edit files. Frame
  updates exchange mailboxes and advance interpolation without filesystem waits;
  unsent input is latest-only and stale input is not replayed.
- The initial supported radius is four columns: up to 81 columns, each 32 by 32 blocks and
  512 blocks deep. A bounded background worker reconstructs the current default
  seed terrain from the same generator used by the server and applies edits.
- Vulkan compute produces exposed unit faces in five indirect ranges. Original
  atlas/mips/animation, sky/day-night, torch/flower geometry, glass, UI and server
  block interaction are connected. Existing Camera frustum culling is reused.
  Neighbor-boundary culling, greedy merging, larger distances, original fluid
  shading/flow, full G-buffer/HDR, clouds and player-model rendering remain open.
- All new shaders are Slang. The API remains Slang GFX; standalone slang-rhi and
  ray tracing are separate unfinished integrations.
- Windows is the verified platform. Linux/macOS branches require qualification.

Use CLI tooling and renderer image captures for verification. The user prohibits
app control and UI automation.
