# OpenUSD scene import

Octaryn composes OpenUSD with the official SDK in this authoring tool. Gameplay
loads a closed cooked glTF scene through the engine scene API; it does not load
USD, run Python composition, or require the original authoring layers.

The SDK pin and wheel digest are in `cmake/Dependencies/DependencyRegistry.cmake`.
Provision the isolated cache without modifying system Python:

```powershell
python tools/build/support/openusd_sdk.py
python tools/SceneImport/usd_import.py --source path/to/world.usdc --metadata
python tools/SceneImport/usd_import.py --source path/to/world.usdc --out build/windows-x64/tools/world-cooked
python tools/SceneImport/test_usd_import.py
```

The current pinned wheel is qualified on Windows x64 / Python 3.12. `--sdk-root`
can select an already provisioned SDK. The importer itself does not install
dependencies. `setup_sdk`, `inspect`, and `cook` are also callable Python APIs
for engine authoring tools.

`--metadata` opens with `Usd.Stage.LoadNone`; it reports authored units, up axis,
composition layers, population and payload paths without reading mesh arrays.
References and sublayers are still composed by USD. An explicit cook loads
payloads in the selected population. `--population /World/Region` can be repeated.
`--payloads none` succeeds only when the selected population has no payloads to
discard. Material and PointInstancer prototype dependencies must be included in
the population. The default purposes are `default` and `render`; `--purpose`
can select a different set explicitly.

Native instance proxies share prototype geometry. PointInstancer prototypes
share geometry across point instances; authored IDs, inactive/invisible masks,
prototype transforms, positions, orientations and scales remain distinguishable
in node metadata. Mesh buffers remain in object space. Node transforms preserve
composed xforms and reset-xform semantics; one root converts authored units to
metres and Y/Z up to engine Y up. Inherited instance primvars are accounted for
when deciding whether geometry can share its attribute buffers.

The static mesh subset supports triangles and planar convex polygons up to
256 corners, right/left handedness, authored normals, indexed `primvars:st`,
`displayColor` and `displayOpacity` with constant/uniform/vertex/varying/faceVarying
interpolation. Missing normals become flat normals. USD UVs are converted from
the lower-left texture origin to glTF's upper-left origin. Unbound meshes use
display colors; a bound PreviewSurface does not implicitly consume displayColor,
so unconnected display colors remain in `_USD_DISPLAY_COLOR` rather than tinting
the bound shader accidentally.

Materials support the UsdPreviewSurface metallic workflow, constant diffuse,
emission, roughness, metallic and opacity, plus direct `UsdUVTexture` RGB PNG/JPEG
connections for diffuse/emission. Diffuse alpha can also drive opacity. Texture
coordinates must use a direct `UsdPrimvarReader_float2` for `st` with explicit
repeat/clamp/mirror wrapping and identity scale/bias. Emission above one uses
`KHR_materials_emissive_strength`. Encoded images and dimensions are bounded;
PNG structure, CRCs, inflated size and scanline filters are checked. JPEG checks
validate its container and dimensions; final JPEG pixel decoding is downstream.

Subdivision surfaces, holes, material subsets, concave/non-planar polygons,
curves, cameras, lights, skinning, normal-map decoding, other connected material
networks and PointInstancer per-instance primvars fail explicitly. Time-sampled
geometry, inherited primvars, shader inputs and transforms require `--time` for
an explicit static snapshot. This tool does not preserve runtime animation.

The default limits are 256 MiB of cooked resources/metadata, two million cooked
vertices and one million visible mesh instances. Composed prim collection has
an additional count limit. `--cancel-file` is checked during traversal, geometry
conversion, texture copying and hashing. These limits bound authored counts and
cooked allocations; they are not a hard process RSS limit on the SDK or Python
objects. An individual SDK composition/load call is not cooperatively canceled
until it returns. Source layers and images are hashed around cooking; changes
abort publication. Output is published from a unique temporary directory only
after resources and the descriptor are complete.

The engine consumes `scene-import.json` version 1. `scene` is a confined relative
path to `scene.gltf`; `files` is a closed list of cooked resources with relative
`path`, lowercase `sha256` and exact `bytes`. `source`/`sources` are informational
provenance, including the SDK version; original USD files are not runtime inputs.
The descriptor includes counts, units, time, payload and population policies.
`prepared: true` means CPU interchange preparation; it does not mean geometry
residency or GPU publication. Existing SceneCatalog v2/ScenePreparation owners
perform later native preparation and rendering admission.

The authored checks cover genuine composed USDA/USDC references and payloads,
native and point prototype reuse, transformed instances, masks/IDs, units/up axis,
indexed attributes, materials/textures, explicit snapshots, changed-source
rejection, cancellation and byte/vertex/instance bounds. Small fixture timings
do not establish AAA scene streaming or graphics performance.

Primary references: [UsdStage composition and load rules](https://openusd.org/release/api/class_usd_stage.html),
[scenegraph instancing](https://openusd.org/release/api/_usd__page__scenegraph_instancing.html),
[PointInstancer transforms and masks](https://openusd.org/release/api/class_usd_geom_point_instancer.html),
[UsdPreviewSurface](https://openusd.org/release/spec_usdpreviewsurface.html),
and [glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
