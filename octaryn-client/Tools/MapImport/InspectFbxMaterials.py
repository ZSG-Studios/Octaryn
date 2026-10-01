"""Read source FBX geometry and selected material alpha without exporting."""
import argparse
import json
from pathlib import Path
import sys

import bpy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fbx', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(args.fbx.resolve()), use_anim=False,
                             use_custom_props=False, use_image_search=False)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    triangles = 0
    for obj in meshes:
        obj.data.calc_loop_triangles()
        triangles += len(obj.data.loop_triangles)
    print('fbx_geometry=' + json.dumps(dict(objects=len(meshes), triangles=triangles)))
    for material in bpy.data.materials:
        if not any(key in material.name for key in ('Table_cloth', 'LiquorBottle_01_Labels', 'Glass_Dirty')):
            continue
        print('fbx_material=' + json.dumps(dict(name=material.name,
            diffuse=list(material.diffuse_color), nodes=[dict(type=n.type,
            image=n.image.filepath if n.type == 'TEX_IMAGE' and n.image else None,
            inputs={s.name: dict(default=list(s.default_value) if hasattr(s.default_value, '__len__')
                    else s.default_value, links=[l.from_node.name + ':' + l.from_socket.name for l in s.links])
                    for s in n.inputs if s.name in ('Alpha', 'Base Color') and hasattr(s, 'default_value')})
            for n in material.node_tree.nodes])))


if __name__ == '__main__':
    main()
