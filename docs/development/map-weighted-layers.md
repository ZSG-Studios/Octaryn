# Weighted map material layers

The generic scene contract is `material.extras.octaryn_material_layers` with
`version: 1` and 1–8 ordered `layers`. Each layer declares a glTF `texture`
index and optional `normal_texture` index, `texcoord` (0 or 1), and row-major
six-float affine `transform`. Every layer shares an authored UV basis so one
surface tangent frame can evaluate its normal maps. This contract contains no
game identities, terrain record types, textures, or original-client code.

Geometry supplies float32 VEC4 `_OCTARYN_BLEND0` and `_OCTARYN_BLEND1`, including
the first layer's explicit weight. Lit opaque layered materials require both
attributes. Imports reject negative or nonfinite values, nonzero unused lanes,
wrong shape/type, and declarations without corresponding attributes. Weights
are not vertex colors or opacity. The import preserves the supplied values;
source-specific normalization belongs to the producer.

The surface shader adds each diffuse sample multiplied by its interpolated
weight. It adds each decoded normal multiplied by the same weight, normalizes
the result, then evaluates it in the common tangent basis. An absent normal
map means the flat tangent-space normal. There is no layer lerp chain, weight
saturation, or per-fragment weight renormalization. Vertex color and material
factors still multiply the final diffuse value. Opaque material alpha is 1.
Raster, virtual-geometry resolve, direct ray hits and deferred reflections
retain both interpolated weight vectors.

Map vertices now occupy 112 bytes, packed ray vertices 104 bytes, and material
records 1072 bytes: the unchanged 64-byte header and 21 48-byte texture slots.
The five standard roles precede eight diffuse/normal pairs. Material flag bit 0
still means unlit; bits 8 onward store the layer count. G-buffer UNORM encoding
retains 1 for unlit and 0 for lit. Geometry/catalog format version 3 and animation
cache version 2 and map LOD cache version 2 reject incompatible older cache payloads. Simplification and
exact vertex deduplication retain all eight weights as independent attributes.
Texture preparation chooses diffuse sRGB and normal linear roles for each pair;
geometry, ray and forward-buffer budgets account for the changed strides.
The exact tile writer emits the custom attributes only for layered materials.

`tools/validation/check_map_layers.py` compiles production schema/material,
flattened map and bounded streaming imports, and the exact tile writer. Its
fixtures exercise source identity, catalog roundtrip, independent vertex alpha,
weight preservation and malformed admission. Affected Slang programs compile
for DXIL and SPIR-V using the registered SDK. Isolated production syntax checks
cover the cache, simplification, material upload and animation consumers. These
checks do not prove original-game lighting parity or runtime GPU presentation;
the canonical build and captured world validation remain separate gates.

The frozen isolated qualification is `build/windows-x64/tools/map-layer-v5/result.json`: 66 authored CPU assertions, 50 DXIL/SPIR-V shader cases, and 12 production source syntax checks passed. `final-source-identity.json` captures the related consumer and tooling source identities. Earlier failed fixtures remain in v1 and v3; v4 predates the shared UV-frame correction for a layer whose base normal is absent.
