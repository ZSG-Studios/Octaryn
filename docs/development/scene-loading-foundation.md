# Scene import and runtime preparation

The foundation has two owners. Authoring tools compose source formats and produce
a closed cooked package. The runtime accepts a module's declared asset ID,
verifies the package on the existing native job scheduler, and retains an
immutable metadata/resource snapshot. GPU decode, admission, upload, publication
and fence retirement remain renderer work.

## Import contract

`tools/SceneImport` uses the official registry-pinned OpenUSD 26.08 SDK at import
time. The qualified wheel is Windows x64 / Python 3.12. It composes references
and payloads and preserves native and point instances, authored transforms,
units, indexed attributes and a documented static material subset. Population
masks and payload loading are explicit. Unsupported visible semantics fail.

`scene-import.json` version 1 records `scene`, a closed `files` array of relative
paths/exact bytes/SHA256, source provenance, counts and import policies. Runtime
inputs are the cooked resources. USD layers are not required to validate a
relocated package.

The loader also accepts plain external-resource glTF and SceneCatalog version 3. Legacy version 2 catalogs and cooked geometry are rejected after the weighted map vertex/material ABI change.
GLB requires an import/cook route; it is not a direct v1 module-loader input.
Catalog preparation verifies complete primitive/part coverage, source tree
digests and cooked part headers. Metadata acceptance is not complete geometry
decode or raster/ray residency admission.

## Runtime contract

The managed contract is [host.scene](host-scene-preparation.md), with a versioned
C ABI table for hosts and a private `octaryn_scene_loading` DLL for the production
managed service. Only declared model/scene asset IDs are accepted. Each module
activation owns eight generation-checked tickets; stale or foreign tickets fail.
Cancellation and release suppress late publication. Owner disposal joins active
work before the existing scheduler is destroyed.

Each owner runs one scene worker at a time. Working admission reserves 32 MiB,
with a global 64 MiB ledger; retained snapshots keep their charge until their
last internal lease is released. These are conservative admission estimates,
not a hard allocator/process RSS limit. JSON files are capped at 4 MiB with a
structural parser estimate, metadata objects are bounded, and source verification
is capped at 16 GiB per resource / 64 GiB total. Hashing checks cancellation
between blocks. Source IO and SDK composition calls may block inside an operation.

`CpuPrepared` exposes metadata/resource identity, progress and retained bytes.
The v1 service reports `Unpublished`. A renderer adapter can retain the internal
immutable snapshot through `snapshot(owner, ticket)`; module code never receives
native pointers, scheduler handles or filesystem access.

## Selected game packaging

Set `OCTARYN_GAME_PROJECT` to an external `.csproj` and optionally set
`OCTARYN_GAME_CONTENT_ROOT` to its local prepared `Assets` / `Data/Content`
package. A fresh game bundle is included in both host bundles. External games
own their UI/items; basegame presentation assets are included only when basegame
is selected. The selected manifest and its compiled registration must match.

## Evidence and remaining qualification

`octaryn_scene_loading_probe` covers 199 native assertions: instance reuse,
resource revisions, hashes, relocation, traversal, remote URIs, cancellation,
ticket generations/capacity, catalog coverage and byte bounds. Genuine USD packages
passed. `HostContentProbe` exercised the production native
bridge, capability/ID scope, status, cancel/release and disposal. USD authoring
checks cover composed ASCII/binary stages and explicit static snapshots.

Tiny USD fixtures measured request submission below one millisecond and CPU
preparation in roughly 8–30 milliseconds in this session. OS cache was already
warm; these are observations rather than cold-cache or AAA qualification. Logs
are local under `logs/tools`, with USD fixtures under the tools build directory.

Large workloads require separate cook, cold/warm open, full-resource verification,
decode, first GPU publication, full residency, memory pressure and movement
measurements. Shader warmup, visual/material parity, unload/reload and stable
authoritative collision need actual captures and runtime evidence. This work
does not complete the [NVRHI renderer rewrite](nvrhi-renderer-rewrite.md).

Module scene activation, GPU capture and profiling of a cooked scene package are
not yet qualified in this repo. Existing DX12 timestamp readback can block
indefinitely; the exact blocked call has not been established.

Shared catalog dispatch, catalog admission and cooked-window header admission all require generation 3. The isolated `build/windows-x64/tools/scene-loading-generations-v2/result.json` passes 201 preparation/verifier assertions plus stale catalog rejection. Its `cook-version-v2/result.json` separately accepts an exact generation 3 cooked header and rejects otherwise identical generations 2 and 4. The first run exposed an absent direct-verifier progress callback; that failure remains preserved under v1.
