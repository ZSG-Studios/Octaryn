# Current architecture

Octaryn keeps four explicit owners. The native C17/C++23 core and C#/.NET 10
module/host systems both participate in the working application.

| Owner | Responsibility |
| --- | --- |
| `octaryn-client` | SDL3 window/input, Slang/RHI rendering, RmlUi, camera and presentation, bounded local-session I/O. |
| `octaryn-server` | Authoritative player/Jolt simulation, block validation, fluid work, world-item transactions, time and persistence. |
| `octaryn-shared` | Commands, IDs, snapshots, manifests, approved module contracts and validation. |
| `octaryn-basegame` | Registered game rules, terrain/material definitions, original assets and bundled module. |

## Graphics

Standalone Slang RHI owns every active first-party GPU pass. Slang supplies
SPIR-V for Vulkan, DXIL for DX12, and Metal source. Native defaults are DX12 on
Windows, Vulkan on Linux and Metal on macOS. SDL3 supplies platform window/input
services; it is not a second graphics renderer. RmlUi uses the same Slang/RHI path.
See [pipeline](../development/pipeline-parity.md) and
[FSR 2.2.1](../development/fsr2-integration.md).

Terrain uses exact exposed-surface/greedy compute meshing, retained GPU resources,
culling and capability-gated indirect batching with no LOD. Sky, HDR scene
lighting, fluids, clouds, skinned local player, world items and UI share the active
pipeline. The display output is SDR. Current source adds shared procedural voxel
AS, RT sun shadows, DDGI, ReSTIR and raster fallbacks; the
[lighting report](../development/lighting-architecture.md) records behavior,
Windows DX12/Vulkan evidence and coverage limits. The tagged preview predates
this lighting integration.

## Authority and persistence

The client supervises a separate local server. Bounded I/O mailboxes keep
process-file reads/writes out of the frame update; latest-state inputs expire
rather than replaying after stalls. Presentation uses authoritative source time
with bounded interpolation history. This is not internet/LAN multiplayer.

Client reconstruction and server queries share deterministic seed terrain.
Generated blocks are transient. Server-owned edits and metadata persist, including
explicit air overrides. Generator identity/revision protects existing saves.
World-item toss reservations, pickup credits and durable acknowledgements preserve
inventory counts across retries. See [world items](../development/world-items.md).

## Module boundary

Modules use explicit shared contracts and approved packages; they do not receive
rendering, persistence, transport or native host internals. Manifest/API/package
validation happens before activation. Native bridge exports are host integration,
not a mod API. Scheduling remains host-owned. The actual bundled module and shared
source contracts are the reference for changes to the evolving API.

Keep source responsibilities focused and code files below 500 physical lines.
Build outputs belong under `build/<preset>/<owner>`, dependencies under
`build/dependencies`, and logs under `logs/<owner>`.

## Qualification

[Current capabilities and gaps](../development/feature-parity.md) separates actual
integration from planned systems. [Validation](../validation/README.md) separates
static checks, CPU logic, GPU execution and package verification. Linux software
and radius-32 results for the preview baseline do not qualify newer lighting.
Its Linux/Metal execution needs independent evidence; Windows results do not
establish cross-platform support.
