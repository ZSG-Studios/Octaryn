#include "TileCook.h"
#include "MapTextureCache.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>

using namespace octaryn::tools::tiles;
bool test_map_tile_order(const std::filesystem::path& directory) {
  const auto require=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
  MapModel model;TextureFiles textures;std::string error;
  textures.images.push_back({"../images/fixture.png"});textures.bytes["fixture"]=64;
  for(unsigned material=0;material<2;++material) {
    MapPrimitive primitive;primitive.first_index=unsigned(model.indices.size());
    primitive.material.alpha_mode=material?MapAlphaMode::Mask:MapAlphaMode::Blend;
    primitive.material.double_sided=material!=0;primitive.material.roughness=.37f;
    primitive.material.base_color[0]=.25f+material*.25f;
    primitive.material.textures[0].image=0;primitive.material.textures[0].texcoord=1;
    primitive.material.textures[0].transform[2]=.125f;
    for(unsigned triangle=0;triangle<16;++triangle) {
      const unsigned location=(triangle*7)%16;
      for(unsigned corner=0;corner<3;++corner) {
        MapVertex vertex{};vertex.position[0]=-31+float(location%4)*7+(corner==1?.5f:0);
        vertex.position[1]=material*.1f;vertex.position[2]=-31+float(location/4)*7+(corner==2?.5f:0);
        vertex.normal[1]=1;vertex.tangent[0]=1;vertex.tangent[3]=-1;
        vertex.uv[0]=float(corner);vertex.uv1[1]=.3f;vertex.color[0]=.8f;
        model.indices.push_back(unsigned(model.vertices.size()));model.vertices.push_back(vertex);
      }
    }
    primitive.index_count=unsigned(model.indices.size())-primitive.first_index;
    model.primitives.push_back(primitive);textures.materials.push_back({"fixture"});
  }
  const auto cook=[&](const char* name,const MapModel& source,PartitionOrder order) {
    const auto output=directory/name;std::filesystem::create_directories(output/"images");
    std::filesystem::copy_file(directory/"images/fixture.png",output/"images/fixture.png",std::filesystem::copy_options::overwrite_existing);
    Settings settings;settings.triangles=4;settings.order=order;
    std::vector<TileResult> results;require(partition(source,textures,settings,output,results,error),error.c_str());
    require(results.size()==8,"Morton fixture tile count changed");
    for(const auto& tile:results)require(tile.triangles==4,"Morton fixture triangle cap changed");
    return output;
  };
  const auto input=cook("order-input",model,PartitionOrder::Input);
  const auto repeat=cook("order-input-repeat",model,Settings{}.order);
  const auto morton=cook("order-morton",model,PartitionOrder::Morton);
  const auto morton_repeat=cook("order-morton-repeat",model,PartitionOrder::Morton);
  std::vector<Triangle> source_triangles;
  for(unsigned p=0;p<model.primitives.size();++p) {
    const auto& primitive=model.primitives[p];
    for(unsigned i=0;i<primitive.index_count;i+=3)source_triangles.push_back({p,primitive.first_index+i});
  }
  TileResult source_result;
  const auto source=input/"tiles/source.glb";
  require(write_tile(model,source_triangles,textures,source,source_result,error),error.c_str());
  require(compare_tile_cooks(source,morton,directory/"source-compare.json"),"source/cook attribute parity failed");
  std::filesystem::remove(source);
  bool changed=false;
  for(unsigned i=0;i<8;++i) {
    const auto file=std::filesystem::path("tiles")/(std::to_string(i)+".glb");
    const auto a=map_texture_file_digest(input/file,error),b=map_texture_file_digest(morton/file,error);
    require(a==map_texture_file_digest(repeat/file,error),"default input order changed");
    require(b==map_texture_file_digest(morton_repeat/file,error),"Morton ordering nondeterministic");changed|=a!=b;
  }
  require(changed,"Morton fixture did not reorder spatial membership");
  require(compare_tile_cooks(input,morton),"Morton changed geometry/material multiset");
  for(unsigned mutation=0;mutation<6;++mutation) {
    auto altered=model;
    switch(mutation) {
      case 0:std::swap(altered.indices[0],altered.indices[1]);break;
      case 1:altered.vertices[0].uv1[0]+=.25f;break;
      case 2:altered.vertices[0].normal[0]=.5f;break;
      case 3:altered.primitives[0].material.alpha_cutoff=.5f;break;
      case 4:altered.primitives[0].material.textures[0].transform[2]+=.25f;break;
      case 5:altered.vertices[0].color[0]=.1f;break;
    }
    const auto bad=cook("order-mutated",altered,PartitionOrder::Morton);bool rejected=false;
    try {compare_tile_cooks(input,bad);}catch(const std::runtime_error&) {rejected=true;}
    require(rejected,"geometry/material mutation escaped multiset validation");
  }
  const auto image_changed=cook("order-image-mutated",model,PartitionOrder::Morton);
  {std::ofstream image(image_changed/"images/fixture.png",std::ios::binary|std::ios::app);image.put('\0');}
  bool image_rejected=false;
  try {compare_tile_cooks(input,image_changed);}catch(const std::runtime_error&) {image_rejected=true;}
  require(image_rejected,"texture content mutation escaped multiset validation");
  // Equal centroids still have a stable source primitive/index tie-break.
  std::vector<Triangle> ties{{1,48},{0,3},{0,0}};auto same=model;
  for(auto& vertex:same.vertices)std::fill_n(vertex.position,3,-1.f);
  Settings settings;settings.order=PartitionOrder::Morton;
  order_triangles(same,ties,{-1,-1,-1},settings);
  require(ties[0].primitive==0 && ties[0].first==0 && ties[1].first==3 && ties[2].primitive==1,"Morton tie order changed");
  std::puts("map_tile_order_tests passed=1 deterministic=1 default_unchanged=1 full_attribute_material_multiset=1 mutations_rejected=7 negative_cells=1 ties=1");return true;
}
