# Interactive world

The native client starts a continuous SDL3 window, a supervised local server and
bounded terrain streaming. The server owns Jolt movement, block edits, time,
world items and saves. Source-time interpolation and a bounded I/O worker keep
filesystem waits out of ordinary frame updates.

Use the [native build and run guide](../build/README.md). Packaged Windows launchers
and prerequisites are described in [release packaging](release-packaging.md).
The ordinary client starts the world; `--diagnostic` is explicitly separate.

## Controls

WASD moves, mouse looks, Space jumps/ascends, Left Ctrl sprints, F/F5 toggles flight,
F4 toggles third-person, V switches shoulder, and Q/Left Shift descends. Click the world to capture the mouse. Captured left/right
click break/place; middle click picks a block. Number keys 1–0 and the wheel select
the hotbar. I/E opens inventory, B opens the creative catalog, T tosses one item,
and Ctrl+T tosses a stack. Escape opens/closes menus, Z cycles zoom, F3 toggles the
HUD and F11 toggles fullscreen. Menus do not stop world simulation.

## World and settings

The checkout default is `saves/open-world-v3`. Set `OCTARYN_CLIENT_WORLD_PATH` to an
absolute path for an isolated world. Generator revision 3 requires matching
metadata; incompatible/unversioned worlds are rejected. Do not relabel old saves.
A relocated package uses the platform's Octaryn application-data directory.
Closing the client or Save & quit requests server shutdown and final persistence.

Render distance is selectable up to 32 columns outward, with 32-block column
width: 1,024 blocks outward and 2,080 blocks across the inclusive square. Full
voxel geometry is retained with no LOD. Loading and moving-center hitches remain
separate performance work.

## Rendering and validation

Every active first-party pass uses Slang and standalone Slang RHI. DX12 is the
Windows default; Vulkan is an explicit Windows alternative and Linux default.
Metal is the macOS target. Windows GPU evidence does not establish native
Linux/Metal execution. HDR scene processing currently produces SDR display output.

[Runtime checks](../validation/runtime-runs.md) document bounded frames, benchmark
mode and renderer captures. [Feature status](feature-parity.md) records the current
pipeline and gameplay limits. Internet/LAN multiplayer is not integrated.
