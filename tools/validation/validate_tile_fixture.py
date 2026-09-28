"""Run generated GLBs through the production map loader and tile manifest parser."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


SOURCE = r'''
#include "MapModel.h"
#include "TileSet.h"
#include <cmath>
#include <cstdio>
using namespace octaryn::client;
int main(int argc,char** argv) {
  if(argc!=3)return 1;
  app::TileSet set;
  const bool loaded=set.load(argv[1]);
  if(argv[2][0]=='0')return loaded?2:0;
  if(!loaded || set.tile_count()!=17)return 3;
  std::vector<unsigned char> shared;
  for(unsigned i=0;i<set.tile_count();++i) {
    rendering::MapModel model;std::string error;
    if(!rendering::load_map_model(set.payload_directory()/set.tile(i)->file,model,error)) {
      std::fprintf(stderr,"fixture_load_failed tile=%u reason=%s\n",i,error.c_str());return 4;
    }
    if(model.vertices.size()!=24 || model.indices.size()!=36 || model.primitives.size()!=1 || model.images.size()!=1)return 5;
    if(i && shared!=model.images[0].bytes)return 6;
    shared=model.images[0].bytes;
    if(model.primitives[0].material.textures[0].image!=0)return 7;
    for(const auto& vertex:model.vertices)
      if(vertex.position[0]<float(i*24)-12 || vertex.position[0]>float(i*24)+12)return 8;
    for(unsigned face=0;face<6;++face) {
      const auto& a=model.vertices[model.indices[face*6]];
      const auto& b=model.vertices[model.indices[face*6+1]];
      const auto& c=model.vertices[model.indices[face*6+2]];
      float u[3],v[3];for(unsigned k=0;k<3;++k){u[k]=b.position[k]-a.position[k];v[k]=c.position[k]-a.position[k];}
      const float alignment=(u[1]*v[2]-u[2]*v[1])*a.normal[0]+(u[2]*v[0]-u[0]*v[2])*a.normal[1]+(u[0]*v[1]-u[1]*v[0])*a.normal[2];
      if(alignment<=0)return 9;
    }
  }
  std::puts("tile_fixture_production_load tiles=17 triangles=204 material_images_shared=1 winding=passed");
  return 0;
}
'''


def main():
    output = ROOT / 'build/release-windows/tools/tile-fixture'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    test = output / 'tile_fixture_test.cpp'
    test.write_text(SOURCE)
    subprocess.run([sys.executable, str(Path(__file__).with_name('make_tile_fixture.py')),
                    str(output), '--tiles', '17'], check=True)
    maps = ROOT / 'octaryn-client/Source/MapWorld'
    tiles = ROOT / 'octaryn-client/Source/WorldStreaming'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++latest',
               '/I' + str(maps), '/I' + str(tiles),
               '/I' + str(ROOT / 'build/dependencies/src/fastgltf/include'),
               '/I' + str(ROOT / 'build/dependencies/src/glaze/include'), str(test),
               str(maps / 'MapModel.cpp'), str(maps / 'MapMaterials.cpp'), str(tiles / 'TileSet.cpp'),
               '/Fe:tile_fixture_test.exe', '/link',
               str(ROOT / 'build/release-windows/deps/build/fastgltf/fastgltf.lib')]
    subprocess.run(command, cwd=output, check=True)
    executable = output / 'tile_fixture_test.exe'
    manifest = json.loads((output / 'map.json').read_text())
    subprocess.run([str(executable), str(output / 'map.json'), '1'], check=True)
    variants = [('escape', {'tile_files': ['../escape.glb'] + manifest['tile_files'][1:]}),
                ('cache_escape', {'texture_cache': '../escape'}),
                ('bounds', {'tiles': [[12, 0, 0, -12, 1, 1]] + manifest['tiles'][1:]}),
                ('mismatch', {'tiles': manifest['tiles'][:-1]}),
                ('version', {'version': 2})]
    for name, changes in variants:
        path = output / f'{name}.json'
        path.write_text(json.dumps(manifest | changes))
        subprocess.run([str(executable), str(path), '0'], check=True)
    print('tile_manifest_rejections passed=5')


if __name__ == '__main__':
    main()
