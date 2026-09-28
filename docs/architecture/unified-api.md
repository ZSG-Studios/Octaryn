# Octaryn Unified Host API — Audit and Design

Updated 2026-09-26. This document audits the current ABI/API surface and defines
the unified host API that all engine libraries are translated into for module and
engine development in C# and C++.

## 1. Audit of the current surface

### 1.1 C ABI (`octaryn-shared/Source/HostAbi`)

- `octaryn_host_abi.{h,c}`: `octaryn_host_abi_version()`, min worker threads,
  plus fixed-size versioned wire structs with `_Static_assert` layout pinning:
  `octaryn_host_command` (96 B), input/frame timing/frame snapshots,
  `octaryn_client_command_frame`, `octaryn_server_snapshot_header`,
  `octaryn_replication_change`, `octaryn_network_message_header`.
- Native host API structs: `octaryn_client_native_host_api` (16 B, one function:
  `enqueue_command`) and `octaryn_server_native_host_api` (48 B, three functions:
  `enqueue_host_command`, `publish_server_snapshot`, `poll_client_commands`,
  plus two reserved slots).
- Shared native libraries already exist and are compiled in-repo: crash
  diagnostics, native logging, native profiling, native memory, native jobs
  scheduling, and Box3D character motion (CollideMover/SolvePlanes/CastMover).
  None of them are reachable through the host ABI.

### 1.2 Native↔managed bridge

- Per-owner `HostBridge/NativeLoading/ManagedBridge.c` loads the managed
  assembly via hostfxr and resolves `UnmanagedCallersOnly` exports
  (`initialize`, `tick`, `shutdown`, plus client snapshot/remote functions).
- The only current callers of the native host API structs are the two
  `LaunchProbe.c` harnesses. The production server (`Host.Run`) activates
  modules against a managed `ConsoleCommandSink` and never touches the ABI.

### 1.3 Module-facing managed API (`octaryn-shared`)

- The entire module API today is `ModuleHostContext(IHostCommandSink Commands)`:
  enqueueing opaque 96-byte `HostCommand`s. `ModuleFrameContext` carries
  delta seconds, frame index, and a world-time snapshot.
- Gating machinery exists and is enforced: `HostApiIds` string IDs,
  `GameModuleManifest.RequestedHostApis`, schedule write declarations, and
  `HostModuleContext.Create`, which swaps in a denied sink when a module did
  not request and schedule-declare the API. `HostApiAllowlist` currently only
  allows `host.commands` and `host.frame`.
- Manifest, validation, sandbox, and framework allowlists are complete and
  working (`GameModules/Validation`, `ModuleSandbox`, `FrameworkAllowlist`).

### 1.4 What modules cannot do today (the gap)

Every engine system below exists in owner code but has **no** module-facing
API behind a `HostApiIds` entry:

| Domain | Existing owner code | Module access today |
| --- | --- | --- |
| Physics | client `PhysicsWorld` (Box3D), shared `CharacterMotion` library | none |
| UI | client RmlUi runtime, debug overlay, display menu | none |
| Audio | client `ActionAudio` | none |
| Networking | LiteNetLib remote transport, replication contracts, snapshots | none |
| Input | client `PlayerControl`, ABI input snapshot | read-only via frame |
| World | server map world, persistence, client world streaming | none |
| Time | server `WorldTimeClock`, shared `WorldTime` | read-only via frame |
| Diagnostics | native log/profile/crash libraries | none |
| Scheduling | native jobs runtime, schedule declarations | none |

`HostApiIds` also declares `host.scheduling`, `host.client_commands`,
`host.server_snapshots`, and `host.replication` with no implementation behind
them, and `HostApiAllowlist` does not even admit them.

## 2. Unified API architecture

### 2.1 Core mechanism: versioned API tables queried by ID

One canonical mechanism covers every domain, in both languages:

- The native host API structs gain `octaryn_host_api_query_fn query_host_api`:
  `const void* query(uint32_t api_id, uint32_t min_version)`. Each domain is a
  C table struct with a `{version, size}` header followed by function pointers.
  Query returns `NULL` when the host does not provide that ID/version.
- The server struct reuses its existing `reserved` slot at offset 32 (size and
  other offsets unchanged). The client struct grows from 16 B to 24 B; its size
  constant bumps and managed validation catches stale hosts.
- Canonical C headers live in `octaryn-shared/Source/HostAbi/` and are the
  single source of truth for table layout, with `_Static_assert` pinning in
  `octaryn_host_abi.c`. C++ engine code includes them directly — no second C++
  API definition.
- C# projections live in `octaryn-shared/Source/Host/Api/`: unsafe table
  structs that mirror the C layout exactly, wrapped by safe interfaces
  (`IHostTimeApi`, `IHostDiagnosticsApi`, …). Managed hosts without a native
  backend vend managed implementations of the same interfaces, so modules see
  one API regardless of host flavor.

### 2.2 Capability gating (unchanged philosophy, real backends)

- A module must list the string ID in `RequestedHostApis`, the ID must pass
  `HostApiAllowlist`, and write access still requires a schedule declaration.
- `HostModuleContext.Create` resolves each requested ID through an
  `IHostApiProvider` and places typed accessors on `ModuleHostContext`;
  unrequested APIs are absent or denied stubs, never silently available.

### 2.3 Naming and ownership

- C: `octaryn_host_<domain>_api` table structs, `OCTARYN_HOST_API_<DOMAIN>` IDs.
- C#: `Octaryn.Shared.Host.Api` namespace, `IHost<Domain>Api` interfaces.
- Modules never see raw table structs, function pointers, Box3D/RmlUi/
  LiteNetLib types, sockets, or native handles — only the safe interfaces.

## 3. Domain table catalog and status

All nine domain tables are defined in `octaryn_host_api.h` with pinned layouts
and C# projections in `octaryn-shared/Source/Host/Api/`. Every domain is gated
by `RequestedHostApis` + `HostApiAllowlist` and surfaces on `ModuleHostContext`
only when requested and available.

| ID | Table | C# interface | Server backend | Client backend | Status |
| --- | --- | --- | --- | --- | --- |
| 1 | `host.time` | `IHostTimeApi` | Stopwatch + live tick | Stopwatch | live, verified |
| 2 | `host.diagnostics` | `IHostDiagnosticsApi` | LiveDebugLog | Debug output | live, verified |
| 3 | `host.physics` | `IHostPhysicsApi` | Box3D closest ray cast + kinematic character move via `octaryn_server_map_world_raycast`/`_step` | pending prediction mirror | live on server, verified (table v2) |
| 4 | `host.world` | `IHostWorldApi` | map world spawn pose + triangle count | pending | live on server, verified |
| 5 | `host.input` | `IHostInputApi` | latest authoritative frame input | latest frame input | live, verified |
| 6 | `host.scheduling` | `IHostSchedulingApi` | NativeScheduleRuntime main/worker | same | live, verified |
| 7 | `host.audio` | `IHostAudioApi` | n/a (authority never mixes) | native `play_action_audio` via the client module host | live on client, verified headless |
| 8 | `host.ui` | `IHostUiApi` | n/a | native `GameUi` notification toast (`#module-toast`, auto-hide) via the client module host | live on client, verified headless |
| 9 | `host.replication` | `IHostReplicationApi` | LES `SessionEntity` module-event broadcast RPC (`ModuleEventData`, 32 B blittable); false until a peer attaches | receive event landed on client wire copy | live on server, verified without peer |

The production client now drives managed modules every frame:
`octaryn-client/Source/Host/ModuleHost.cpp` owns the native host API tables
(time, diagnostics, input, audio, UI), calls the client bridge
initialize/tick/shutdown from `run_map_world_session`, and is linked into
`octaryn_client_app` when .NET hosting is available. Verified on a headless
`--frames 60` run: `module_host active=1`, module activation through the
native query path, live input poll, and module diagnostics reaching the
client log. The client validator's stale replication deny was removed;
publishing stays authority-only (client provider returns null). The stale
`block_transport`/`world_plant_*` voxel references blocking the client boot
were completed for the map-only path (`world_gbuffer_count`), with voxel
shader modules kept intact as live shader contracts.

Runtime evidence (2026-09-26, release-windows): server launch probe EXIT=0;
one-shot and dedicated `--listen` servers over the 4.15 M-triangle Bistro map
show the module raycast hitting the floor at y=0.324, a module-driven character
falling 4 m and landing (`ground=1`, eye y=1.934 = floor + 1.62 eye offset),
world facts, input poll, a scheduled worker job, and the replication channel
attached with correct no-peer `false` reporting; client launch probe passes
including reinitialize; production headless client runs 60 frames with modules
ticking (`module_host active=1`).

Phase map for what remains inside each domain:

- Physics: shape casts, overlap queries, per-entity body handles. Client gets
  a prediction-scoped mirror of raycast/move.
- UI: full retained declarations per AGENTS.md (models, surfaces, anchors,
  actions) on top of the landed notification toast and action polling.
- Audio: positional routing and module-declared sound assets; the current
  table maps hashed ids onto the four synthesized action sounds.
- Networking: module-declared replicated state (LES sync vars owned by module
  entities) on top of the landed fixed-size event broadcast; variable byte
  payloads stay on the native mailbox table. End-to-end client receipt is
  compiled in and awaits a connected-client run.
- World/input/scheduling: map streaming intents, input action mapping, and
  declarative module system schedules on the native jobs runtime.

## 4. Versioning rules

- Every table starts with `{uint32 version; uint32 size}`. Hosts may implement
  any version ≥ the module's minimum; consumers must check `size` before
  reading beyond the fields their version knows.
- Adding fields appends to the table and bumps its version; existing offsets
  never move. The `_Static_assert` block in `octaryn_host_abi.c` is updated in
  the same commit as any layout change.
- `GameModuleManifest` minimum/maximum host API versions gate activation.
