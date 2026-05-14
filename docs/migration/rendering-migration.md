# Rendering Migration

Rendering is client-owned. Server and basegame code must not own GPU upload, shader pipelines, render descriptors, windows, overlays, or presentation.

## Source Map

| Old source | New owner |
| --- | --- |
| `references/old-architecture/source/render/` | `octaryn-client/Source/Rendering/` |
| `references/old-architecture/source/shaders/` | `octaryn-client/Shaders/` |
| `references/old-architecture/source/world/chunks/build_mesh.*` | client mesh presentation, with server-owned authoritative chunk data |
| `references/old-architecture/source/world/chunks/upload*` | client rendering/upload |
| `references/old-architecture/source/world/runtime/render_descriptors.cpp` | client rendering/world presentation |

## Validation

Rendering changes should be validated with direct runtime runs, external GPU captures when a developer has a local capture tool installed, focused profiling logs, or targeted shader/tool checks. Do not use `ctest` unless explicitly requested.
