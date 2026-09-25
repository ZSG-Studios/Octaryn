"""Blender script: import FBX map sources, relink converted textures, export GLB.

Run headless:
  blender -b --python ImportFbxMap.py -- --fbx a.fbx b.fbx \
      --textures <converted-dir> --alpha-modes <json> \
      --output <map.glb> --report <stats.json>

Imports each FBX at meter scale, swaps every texture reference for the
converted raster in --textures-dir (matched by basename), wires base color
plus alpha (MASK via greater-than, BLEND direct), partitions exact triangles
spatially by material, and exports one GLB with tangents and embedded images.
"""

import argparse
import json
import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from MapSourceSelection import select_sources
from BistroMaterials import wire_bistro_material
from BistroAlpha import alpha_mode
from MapMeshPartition import partition_meshes


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fbx", nargs="+", required=True, type=Path)
    parser.add_argument("--bistro-interior", choices=("original", "wine"), default="wine",
                        help="Select one alternative when both Bistro interiors are supplied")
    parser.add_argument("--textures", required=True, type=Path)
    parser.add_argument("--alpha-modes", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--max-extent", type=float, default=400.0)
    parser.add_argument('--cell-size', type=float, default=16.0)
    parser.add_argument("--unit-scale", type=float, default=1.0,
                        help="Uniform object scale applied before the extent check")
    return parser.parse_args(argv)


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_fbx(paths: list[Path]) -> None:
    for path in paths:
        print(f"importing {path.name}", flush=True)
        bpy.ops.import_scene.fbx(
            filepath=str(path),
            use_anim=False,
            use_custom_props=False,
            use_image_search=False,
        )


def keep_only_meshes(unit_scale: float) -> list[bpy.types.Object]:
    meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    parented = [obj for obj in meshes if obj.parent is not None]
    print(f"meshes_with_parents={len(parented)}", flush=True)
    if parented:
        # Flatten hierarchies in place while parents still exist: both
        # transform_apply and the scale normalization below only see local
        # transforms, so parented children must carry their world transform.
        for obj in meshes:
            obj.select_set(obj.parent is not None)
        bpy.context.view_layer.objects.active = parented[0]
        bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
        for obj in meshes:
            obj.select_set(False)
    for obj in list(bpy.data.objects):
        if obj.type != "MESH":
            bpy.data.objects.remove(obj, do_unlink=True)
    # FBX unit conversion is inconsistent across sources: some arrive with raw
    # centimeter data at scale 1, others pre-scaled. Normalize to one unit.
    for obj in meshes:
        if max(abs(obj.scale[k]) for k in range(3)) > unit_scale * 2:
            obj.scale = tuple(obj.scale[k] * unit_scale for k in range(3))
    bpy.context.view_layer.update()
    return meshes


def apply_transforms(meshes: list[bpy.types.Object]) -> None:
    # Instanced FBX geometry shares mesh datablocks; transform_apply needs
    # single-user data, so split instances before baking.
    for obj in meshes:
        if obj.data.users > 1:
            obj.data = obj.data.copy()
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    result = bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    print(f"transform_apply: {result}", flush=True)
    for obj in meshes:
        obj.select_set(False)


def scene_bounds(meshes: list[bpy.types.Object]) -> tuple[list[float], list[float]]:
    minimum = [float("inf")] * 3
    maximum = [float("-inf")] * 3
    for obj in meshes:
        for corner in obj.bound_box:
            world = obj.matrix_world @ Vector((corner[0], corner[1], corner[2]))
            for axis in range(3):
                value = world[axis]
                minimum[axis] = min(minimum[axis], value)
                maximum[axis] = max(maximum[axis], value)
    return minimum, maximum


def build_texture_index(directory: Path) -> dict[str, Path]:
    index: dict[str, Path] = {}
    for path in sorted(directory.iterdir()):
        if path.suffix.lower() in (".png", ".jpg"):
            index.setdefault(path.stem, path)
    return index


def principled_node(material: bpy.types.Material):
    for node in material.node_tree.nodes:
        if node.type == "BSDF_PRINCIPLED":
            return node
    return None


def ensure_material_nodes(material: bpy.types.Material):
    material.use_nodes = True
    return principled_node(material)


def link_image_node(material, image, position_x):
    node = material.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = image
    node.location = (position_x, 0)
    node.width = 140
    node.height = 100
    return node


def relink_material(
    material: bpy.types.Material,
    textures: dict[str, Path],
    alpha_modes: dict[str, str],
) -> str:
    """Point the material at converted textures. Returns basecolor stem or ''."""
    tree = material.node_tree
    bsdf = ensure_material_nodes(material)
    if bsdf is None:
        return ""

    # Replace every loadable texture with its converted raster; drop the rest.
    used_stem = ""
    for node in list(tree.nodes):
        if node.type != "TEX_IMAGE":
            continue
        stem = Path(node.image.filepath).stem if node.image else ""
        converted = textures.get(stem)
        if converted is None:
            tree.nodes.remove(node)
            continue
        image = bpy.data.images.load(str(converted), check_existing=True)
        image.colorspace_settings.name = "sRGB"
        node.image = image
        if not used_stem:
            used_stem = stem

    # Fall back to the material-name convention when nothing loaded.
    if not used_stem:
        for candidate in (f"{material.name}_BaseColor", material.name):
            converted = textures.get(candidate)
            if converted is not None:
                image = bpy.data.images.load(str(converted), check_existing=True)
                image.colorspace_settings.name = "sRGB"
                node = link_image_node(material, image, -460)
                tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
                used_stem = candidate
                break

    if not used_stem:
        return ""

    # Re-wire base color from the first surviving image node if unlinked.
    if not bsdf.inputs["Base Color"].is_linked:
        for node in tree.nodes:
            if node.type == "TEX_IMAGE" and node.image:
                tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
                break

    mode = alpha_mode(material.name, alpha_modes.get(used_stem))
    if mode == "OPAQUE":
        alpha = bsdf.inputs["Alpha"]
        if alpha.is_linked:
            for link in list(alpha.links):
                tree.links.remove(link)
            alpha.default_value = 1.0
        # An unlinked alpha below one is an authored material factor.
        return used_stem
    if mode is None or not bsdf.inputs["Base Color"].is_linked:
        return used_stem
    color_node = bsdf.inputs["Base Color"].links[0].from_node
    alpha_socket = color_node.outputs.get("Alpha")
    if alpha_socket is None:
        return used_stem
    if mode == "MASK":
        cutoff = tree.nodes.new("ShaderNodeMath")
        cutoff.operation = "GREATER_THAN"
        cutoff.inputs[1].default_value = 0.5
        cutoff.location = (-300, -200)
        tree.links.new(alpha_socket, cutoff.inputs[0])
        tree.links.new(cutoff.outputs[0], bsdf.inputs["Alpha"])
    else:
        tree.links.new(alpha_socket, bsdf.inputs["Alpha"])
    return used_stem


def count_triangles(meshes: list[bpy.types.Object]) -> int:
    triangles = 0
    for obj in meshes:
        obj.data.calc_loop_triangles()
        triangles += len(obj.data.loop_triangles)
    return triangles


def export_glb(output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=str(output),
        export_format="GLB",
        export_yup=True,
        # export_apply re-evaluates FBX-imported objects at their original
        # scale; transforms are already baked, so export raw mesh data.
        export_apply=False,
        export_materials="EXPORT",
        export_image_format="AUTO",
        export_texcoords=True,
        export_normals=True,
        export_tangents=True,
        export_skins=False,
        export_animations=False,
        export_cameras=False,
        export_lights=False,
        export_extras=False,
        export_draco_mesh_compression_enable=False,
        use_selection=False,
    )


def main() -> int:
    args = parse_args()
    args.fbx, excluded = select_sources(args.fbx, args.bistro_interior)
    for path in excluded:
        print(f"skipping alternative interior: {path.name}; selected {args.bistro_interior}", flush=True)
    args.textures = args.textures.resolve()
    args.alpha_modes = args.alpha_modes.resolve()
    args.output = args.output.resolve()
    args.report = args.report.resolve()
    alpha_modes = json.loads(args.alpha_modes.read_text())
    textures = build_texture_index(args.textures)
    print(f"converted textures indexed: {len(textures)}", flush=True)

    reset_scene()
    import_fbx(args.fbx)
    meshes = keep_only_meshes(args.unit_scale)
    print(f"mesh objects: {len(meshes)}", flush=True)

    minimum, maximum = scene_bounds(meshes)
    extent = [maximum[i] - minimum[i] for i in range(3)]
    print(f"bounds min={minimum} max={maximum} extent={extent}", flush=True)
    if max(extent) > args.max_extent:
        print(f"ERROR: extent exceeds {args.max_extent}m; check FBX units", flush=True)
        return 1

    apply_transforms(meshes)
    unapplied = [obj.name for obj in meshes
                 if any(abs(obj.scale[k] - 1.0) > 1e-6 for k in range(3))]
    if unapplied:
        print(f"ERROR: {len(unapplied)} objects were not transform-applied: "
              f"{unapplied[:5]}", flush=True)
        return 1

    linked = 0
    pbr_slots = {}
    untextured: list[str] = []
    for material in bpy.data.materials:
        stem = relink_material(material, textures, alpha_modes)
        if stem:
            for slot, present in wire_bistro_material(material, textures, stem).items():
                pbr_slots[slot] = pbr_slots.get(slot, 0) + int(present)
        if stem:
            linked += 1
        else:
            untextured.append(material.name)
    print(f"materials textured: {linked}/{len(bpy.data.materials)}", flush=True)
    if untextured:
        print(f"untextured: {', '.join(sorted(untextured)[:20])}", flush=True)

    meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    joined, partition = partition_meshes(meshes, args.cell_size)
    bpy.context.view_layer.update()
    triangles = count_triangles(joined)
    images = {img.filepath for img in bpy.data.images if img.packed_file is None}
    print(f"joined objects: {len(joined)} triangles: {triangles}", flush=True)

    minimum, maximum = scene_bounds(joined)
    report = {
        "sources": [str(path) for path in args.fbx],
        "excluded_alternatives": [str(path) for path in excluded],
        "pbr_slots": pbr_slots,
        "spatial_partition": partition,
        "bounds_min": minimum,
        "bounds_max": maximum,
        "objects": len(joined),
        "triangles": triangles,
        "materials": len(bpy.data.materials),
        "images": len(images),
    }
    args.report.write_text(json.dumps(report, indent=2))

    export_glb(args.output)
    size_mb = args.output.stat().st_size / 1e6
    print(f"exported {args.output.name}: {size_mb:.1f} MB", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
