#include "TileCook.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

using namespace octaryn::tools::tiles;
bool test_map_tiles(const std::filesystem::path& directory) {
  const auto require=[](bool pass,const std::string& message) {if(!pass)throw std::runtime_error(message);};
  MapModel model;TextureFiles textures;std::string error;
  const std::vector<std::uint8_t> png{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,0,0,0,15,73,68,65,84,120,156,99,248,143,6,24,72,23,0,0,120,60,63,193,88,238,41,168,0,0,0,0,73,69,78,68,174,66,96,130};
  std::filesystem::create_directories(directory/"images");
  {std::ofstream image(directory/"images"/"fixture.png",std::ios::binary);image.write(reinterpret_cast<const char*>(png.data()),png.size());}
  model.images.push_back({png,"image/png"});textures.images.push_back({"../images/fixture.png"});
  for(unsigned p=0;p<2;++p) {
    MapPrimitive primitive;primitive.first_index=static_cast<unsigned>(model.indices.size());primitive.index_count=6;
    primitive.material.alpha_mode=p?MapAlphaMode::Mask:MapAlphaMode::Blend;primitive.material.double_sided=p!=0;
    primitive.material.base_color[0]=.25f;primitive.material.alpha_cutoff=.3f;primitive.material.normal_scale=.7f;
    primitive.material.emissive[0]=4;primitive.material.emissive[1]=2;primitive.material.occlusion_strength=.6f;
    primitive.material.texture=0;
    for(unsigned role=0;role<5;++role) {
      auto& texture=primitive.material.textures[role];texture.image=0;texture.texcoord=role%2;
      texture.wrap_s=33071;texture.wrap_t=33648;texture.min_filter=9984;texture.mag_filter=9728;
      const float angle=.3f*role,scale=p?-2.f:2.f;
      texture.transform[0]=std::cos(angle)*scale;texture.transform[1]=-std::sin(angle)*3;
      texture.transform[2]=.25f;texture.transform[3]=std::sin(angle)*scale;texture.transform[4]=std::cos(angle)*3;texture.transform[5]=-.125f;
    }
    for(unsigned i=0;i<4;++i) {
      MapVertex vertex{};vertex.position[0]=p*64.f+(i%2);vertex.position[2]=float(i/2);vertex.normal[1]=1;
      vertex.uv[0]=float(i%2);vertex.uv[1]=float(i/2);vertex.uv1[0]=.7f;vertex.tangent[0]=1;vertex.tangent[3]=-1;
      vertex.color[0]=.8f;model.vertices.push_back(vertex);
    }
    for(unsigned i:{0,1,2,2,1,3})model.indices.push_back(p*4+i);
    model.primitives.push_back(primitive);textures.materials.emplace_back();
  }
  Settings settings;settings.triangles=1;
  std::vector<TileResult> results;require(partition(model,textures,settings,directory,results,error),error);require(results.size()==4,"tile count");
  std::vector<std::array<float,9>> actual,expected;
  for(const auto& result:results) {
    MapModel loaded;require(load_map_model(directory/result.file,loaded,error),error);require(loaded.indices.size()==3,"triangle limit");
    std::array<float,9> positions{};
    for(unsigned i=0;i<3;++i) {
      const auto& vertex=loaded.vertices[loaded.indices[i]];
      std::copy_n(vertex.position,3,positions.data()+i*3);require(vertex.uv1[0]==.7f && vertex.tangent[3]==-1 && vertex.color[0]==.8f,"vertex attributes changed");
    }
    actual.push_back(positions);const auto& material=loaded.primitives[0].material;
    require(material.base_color[0]==.25f && material.emissive[0]==4 && material.alpha_cutoff==.3f,"material changed");
    const auto& original=model.primitives[positions[0]>=64?1:0].material;
    require(material.alpha_mode==original.alpha_mode && material.double_sided==original.double_sided &&
        material.normal_scale==original.normal_scale && material.occlusion_strength==original.occlusion_strength,"material behavior changed");
    require(loaded.images.size()==1 && loaded.images[0].bytes==png,"shared image mismatch");
    for(unsigned role=0;role<5;++role) {
      const auto& a=material.textures[role];const auto& b=original.textures[role];
      require(a.image==0 && a.texcoord==b.texcoord && a.wrap_s==b.wrap_s && a.wrap_t==b.wrap_t &&
          a.min_filter==b.min_filter && a.mag_filter==b.mag_filter,"texture sampler or UV set changed");
      for(unsigned element=0;element<6;++element)require(std::abs(a.transform[element]-b.transform[element])<1e-6f,"texture transform changed");
    }
  }
  for(size_t triangle=0;triangle<model.indices.size();triangle+=3) {
    std::array<float,9> positions{};for(unsigned i=0;i<3;++i)std::copy_n(model.vertices[model.indices[triangle+i]].position,3,positions.data()+i*3);expected.push_back(positions);
  }
  std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());require(actual==expected,"triangle conservation failed");
  const auto image_fixture=directory/results.front().file;
  const auto image_path=directory/"images"/"fixture.png";
  MapModel invalid_image;
  std::filesystem::resize_file(image_path,0);
  require(!load_map_model(image_fixture,invalid_image,error) && error.find("encoded image")!=std::string::npos,
      "empty external image accepted");
  std::filesystem::resize_file(image_path,86ull*1024*1024+1);
  require(!load_map_model(image_fixture,invalid_image,error) && error.find("encoded image")!=std::string::npos,
      "oversized external image accepted");
  {std::ofstream image(image_path,std::ios::binary|std::ios::trunc);image.write(reinterpret_cast<const char*>(png.data()),png.size());}
  require(load_map_model(image_fixture,invalid_image,error) && invalid_image.images[0].bytes==png,
      "valid external image failed after bounded read rejection");
  MapLoadLimits limits;
  limits.source_bytes=1;
  require(!load_map_model(image_fixture,invalid_image,error,limits) && error.find("size bound")!=std::string::npos,
      "source limit was not enforced before parsing");
  limits={};limits.encoded_bytes=png.size()-1;
  require(!load_map_model(image_fixture,invalid_image,error,limits) && error.find("aggregate")!=std::string::npos,
      "encoded aggregate limit was not enforced");
  limits={};limits.accessor_elements=2;
  require(!load_map_model(image_fixture,invalid_image,error,limits) && error.find("accessor")!=std::string::npos,
      "accessor allocation bound was not enforced");
  limits={16ull*1024*1024,64ull*1024*1024,16384,2048,49152};
  require(load_map_model(image_fixture,invalid_image,error,limits),"valid bounded tile rejected: "+error);
  std::atomic_bool cancelled{true};limits.cancel=&cancelled;
  require(!load_map_model(image_fixture,invalid_image,error,limits) && error.find("cancelled")!=std::string::npos,
      "cancelled bounded tile read continued");
  limits.cancel=nullptr;
  settings.max_tiles=1;require(!partition(model,textures,settings,directory,results,error),"tile count limit not enforced");
  settings.max_tiles=4096;textures.materials[0].insert("large");textures.bytes["large"]=settings.texture_budget+1;
  require(!partition(model,textures,settings,directory,results,error),"texture limit not enforced");
  MapModel dense;dense.vertices.assign(model.vertices.begin(),model.vertices.begin()+3);
  MapPrimitive primitive;primitive.index_count=16385*3;primitive.material=model.primitives[0].material;
  dense.primitives.push_back(primitive);
  for(unsigned i=0;i<16385;++i)dense.indices.insert(dense.indices.end(),{0,1,2});
  TextureFiles untextured;untextured.materials.resize(1);dense.primitives[0].material=MapMaterial{};
  const auto dense_dir=directory/"dense";
  require(partition(dense,untextured,Settings{},dense_dir,results,error),error);
  require(results.size()==2,"default 16k build units did not split dense primitive");
  unsigned conserved=0;
  for(const auto& result:results) {
    require(result.triangles<=16384,"BLAS triangle cap exceeded");conserved+=result.triangles;
    MapModel loaded;require(load_map_model(dense_dir/result.file,loaded,error,limits),error);
    require(loaded.indices.size()/3==result.triangles,"split geometry count differs");
    for(size_t i=0;i<loaded.indices.size();++i)
      require(std::memcmp(loaded.vertices[loaded.indices[i]].position,dense.vertices[i%3].position,12)==0,
          "16k split changed triangle position or winding");
  }
  require(conserved==16385,"16k split lost triangles");
  std::printf("map_tile_tests passed=1 exact_triangles=16389 vertex_attributes=1 materials=1 limits=1 cancellation=1\n");return true;
}
