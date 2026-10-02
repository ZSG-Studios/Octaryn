# Module scene preparation

`host.scene` prepares module-declared `model` or `scene` assets. A module calls
`BeginPrepare(assetId, out ticket, out error)`, polls `TryGetStatus`, and explicitly
calls `Cancel` or `Release`. IDs must belong to the manifest and resolve to regular
files under the selected bundle's `Assets/` root. Raw paths, renderer handles,
scheduler handles and native objects never appear on the module API.

Each activation owns at most eight tickets. A ticket carries an ID and generation;
foreign, stale and released tickets are rejected before invoking the backend.
Release invalidates the public ticket immediately; native workers retain their
private operation until cancellation and retirement complete. Module activators
dispose every scene API on failed activation and shutdown, before disposing the
host's existing scheduler. No extra worker pool is created for a game module.

Preparation states are `Queued`, `Running`, `CpuPrepared`, `Failed`, and `Canceled`.
`CpuPrepared` means a verified immutable CPU snapshot of metadata and resource
identity. It does not mean decoded geometry, cooked virtual geometry, uploaded
textures, GPU readiness, or a published renderer scene. Progress exposes completed
work, total work and retained bytes. Publication has a separate field: the initial
CPU service always reports `Unpublished`, and its projection rejects a claimed GPU
publication. Future publication requires renderer-owned admission and submission
lifetime contracts.

The managed host invokes `octaryn_scene_loading` through a private native bridge.
If the library, its exports or a host scheduler are unavailable, the API remains
unavailable. It never returns a successful placeholder ticket. Tools can use the
same native lifecycle directly with a host-owned bundle root and scheduler.
OpenUSD conversion belongs to CPU content tooling; the runtime verifies the
prepared descriptor and glTF resources before accepting a snapshot.

The C host ABI defines domain 11/version 1, a 48-byte table, 16-byte ticket and
32-byte progress record. Native table callbacks resolve module/asset declaration
IDs independently and guarantee globally unique ticket identities. The managed
projection independently confines declarations and tracks tickets by activation.

Validation uses `tools/validation/HostContentProbe`: it checks declaration and
capability boundaries, layout, generation reuse, cancellation, ticket admission,
release, disposal, and CPU/publication separation with native callback fixtures.
The fixture does not establish live native preparation, GPU presentation or an
AAA throughput claim. Those require separate runtime and measured workload tests.

The same probe can exercise the actual managed-to-native DLL backend:

```powershell
dotnet run --project tools/validation/HostContentProbe/HostContentProbe.csproj -p:OctarynBuildPresetName=debug-windows -- --native-scene <scene-loading-library> --native-jobs <native-jobs-library> --scene-root <prepared-module-package>
```

The probe defaults to `Assets/Scene/scene-import.json`, then `scene.gltf` when the
descriptor is absent; pass `--asset-relative` for another declared asset. It checks
CPU preparation, unpublished status, actual cancellation, release, owner scope,
and disposal on the existing native scheduler. Its printed preparation time is
one fixture observation, not an AAA performance qualification.
