# Map unlit materials

The generic map import and rendering paths admit `KHR_materials_unlit`, including
when it is a required glTF extension. Material catalog serialization preserves
the flag; catalogs without that field retain the lit default.

The [Khronos extension](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_unlit/README.md)
specifies base-color factor multiplied by vertex color and the base-color texture.
Lighting-only material textures are not admitted as consumed resources for these
materials. Authored alpha mode, alpha cutoff and double-sidedness still apply.
Existing scene fog and final exposure/tone mapping remain presentation operations.

The GPU material now contains 1072 bytes with textures at offset 64. Its existing second
padding word stores the unlit flag in bit 0 and the weighted-layer count in bits 8 onward; the first retains triangle identity. Map
G-buffer resolve writes a material alpha marker of 1 (lit maps retain 0), within RGBA8 UNORM. Lighting tests that
marker only for map surfaces, so the dormant voxel emission channel keeps its
existing meaning. Opaque virtual geometry, forward transparency, local lighting
and both direct/deferred ray-reflection consumers preserve the distinction.
Unlit receivers do not schedule reflection work; reflecting lit surfaces can
still see an unlit object's base color. Depth, alpha coverage and geometry stay
unchanged.

`tools/validation/check_map_unlit.py` compiles actual production material parsing,
GPU record construction and scene-material Glaze serialization against the pinned
libraries. It checks lit/unlit OPAQUE, MASK and BLEND cases, required-extension
admission, base texture identity, lighting-only texture exclusion, catalog
roundtrips, old-catalog defaults and the unchanged GPU layout. It also invokes the
registered Slang SDK for DXIL and SPIR-V versions of the affected raster,
composite, local-lighting and ray/reflection entries. These are CPU compiler and
contract checks, not a rendered-image or full backend qualification.

Preserved receipts live under `build/windows-x64/tools/map-unlit-v*`. Version 1
had an invalid entry-name fixture; version 3 exposed the probe's older C++ mode,
and version 4 used an unsupported clang-cl spelling. Those failed receipts remain
intact. Version 5 passed compiler checks but a subsequent actual attachment-format audit
found its negative marker would clamp in RGBA8 UNORM. That is corrected in
version 7, which also checks the production format and map-only marker. Version 6 exposed a missing generated SDK include in the extended probe.
Version 7 uses the production C++23-equivalent clang-cl mode and is the
qualification receipt for the original unlit slice. The subsequent weighted-layer ABI
retains these semantics and rechecks them in `map-layer-unlit-v2`. Source hashes change shader pipeline keys: the
canonical bundle must compile/warm the new shaders and retain the existing
watchdog thresholds before runtime claims. This slice does not implement the
NVRHI cutover or original-game shader falloff/environment semantics.
