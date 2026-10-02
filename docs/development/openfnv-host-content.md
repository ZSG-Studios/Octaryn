# Declared module content

`host.content` lets a selected game consume generated metadata through the host.
The API is available to client and server modules that request the capability.
It reads a content declaration ID from the validated module manifest, never a
filesystem path. Existing module validation still applies before activation.

`IHostContentApi.TryRead(id, out ReadOnlyMemory<byte> data, out string error)`
returns a fresh owned copy, with a 4 MiB limit per operation. IDs must belong to
the active module, paths must stay under its declared `Data/` directory, and
symbolic links and junctions are rejected. Failed reads return no data and a
reason. Hosts pass the selected bundle root to `HostModuleContext.Create`;
ordinary bundled activation uses the application bundle root.

OpenFNV's native tools own Bethesda archive, record and model import. They write
generated metadata declared by the OpenFNV game. The game requests `host.content`
and consumes those declarations. Original game archives remain outside this
module-facing API. Geometry preparation continues through the existing scene
catalog and virtual geometry tools; this API does not load a renderer or swap
world authority.

The C ABI defines domain 10, version 1, a 24-byte `octaryn_host_content_api`
table with `read_data(module_id, content_id, buffer, capacity, out_bytes)`.
The host callback must independently validate the module and declaration and
respect the advertised read bound. A null buffer and zero capacity query the
length; the second call copies bytes into caller-owned memory. The C# projection
rejects malformed tables, oversized lengths and changed lengths. Native clients
do not currently vend this table: their managed module hosts provide declared
content instead. Table definitions and a passing projection fixture do not prove
that a native content callback exists.

Focused validation:

```powershell
dotnet run --project tools/validation/HostContentProbe/HostContentProbe.csproj -p:OctarynBuildPresetName=debug-windows
```

The probe covers declared reads, capability denial, module scope, traversal,
missing files, independent copies, empty files, oversized input, native layout,
missing callbacks, changed lengths, and oversized native length reports.
