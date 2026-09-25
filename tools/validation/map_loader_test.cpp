#include "MapModel.h"
#include "MapSceneGeometry.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
namespace {
unsigned checks{};
void require(bool value,const char* message) {
  ++checks;
  if(!value)throw std::runtime_error(message);
}
bool near(float a,float b) {return std::abs(a-b)<0.0001f;}
void inspect(const std::filesystem::path& root,const char* name,bool flat=false,
    bool mirrored=false,bool rgb=false) {
  MapModel model;std::string error;
  require(load_map_model(root/(std::string(name)+".glb"),model,error),error.c_str());
  require(model.primitives.size()==1,"primitive count");
  require(model.indices.size()==6,"triangle topology expansion");
  require(model.vertices.size()==(flat?6u:4u),"flat normals split vertices");
  const auto& material=model.primitives[0].material;
  require(near(material.metallic,1)&&near(material.roughness,1),"glTF missing-material defaults");
  require(material.texture==-1,"missing base color texture");
  for(const auto& vertex:model.vertices) {
    require(near(vertex.normal[2],1),"normal transform or generated winding");
    require(near(vertex.uv[0],0)&&near(vertex.uv[1],0),"missing UV defaults");
    require(near(vertex.uv1[0],.25f)&&near(vertex.uv1[1],.75f),"UV1 import");
    require(near(vertex.color[0],.2f)&&near(vertex.color[2],.6f),"vertex color import");
    require(near(vertex.color[3],rgb?1.f:.8f),"RGB/RGBA alpha default");
    require(near(vertex.tangent[3],flat?0.f:(mirrored?-1.f:1.f)),"tangent handedness or regeneration marker");
    require(near(vertex.position[2],4),"node translation");
  }
  for(size_t i=0;i<model.indices.size();i+=3) {
    const auto& a=model.vertices[model.indices[i]];
    const auto& b=model.vertices[model.indices[i+1]];
    const auto& c=model.vertices[model.indices[i+2]];
    const float winding=(b.position[0]-a.position[0])*(c.position[1]-a.position[1])-
        (b.position[1]-a.position[1])*(c.position[0]-a.position[0]);
    require(winding>0,"front winding must match transformed normal");
  }
}
}
int main(int argc,char** argv) {
  if(argc!=2&&argc!=3)return 2;
  try {
    const std::filesystem::path root=argv[1];
    inspect(root,"triangles");inspect(root,"flat",true);
    inspect(root,"strip");inspect(root,"fan");
    inspect(root,"mirrored",false,true);inspect(root,"color3",false,false,true);
    MapModel model;std::string error;
    require(!load_map_model(root/"invalid-index.glb",model,error),"invalid index rejected");
    require(!error.empty(),"invalid geometry error reported");
    require(!load_map_model(root/"morph.glb",model,error),"unsupported morph rejected");
    require(error.find("morph targets")!=std::string::npos,"explicit morph error reported");
    octaryn::server::map_world::MapTriangleSoup soup;
    require(octaryn::server::map_world::load_map_triangle_soup(root/"triangles.glb",soup),"server triangle fixture accepted");
    soup={};
    require(!octaryn::server::map_world::load_map_triangle_soup(root/"morph.glb",soup),"server unsupported morph rejected");
    require(load_map_model(root/"uv1-texture.glb",model,error),"referenced UV1 accepted");
    require(model.primitives[0].material.textures[0].texcoord==1,"material selects UV1");
    require(!load_map_model(root/"missing-uv-texture.glb",model,error),"missing referenced UV rejected");
    require(load_map_model(root/"pbr.glb",model,error),"PBR texture fixture loaded");
    const auto& material=model.primitives[0].material;
    require(near(material.metallic,.7f)&&near(material.roughness,.8f),"PBR factors");
    require(near(material.base_color[0],.2f)&&near(material.base_color[3],.6f),"linear base factor");
    require(near(material.emissive[0],.4f)&&near(material.emissive[2],1.2f),"emission strength");
    require(near(material.normal_scale,.4f)&&near(material.occlusion_strength,.5f),"normal and AO strength");
    require(material.alpha_mode==MapAlphaMode::Mask&&near(material.alpha_cutoff,.25f)&&material.double_sided,"alpha and culling");
    for(const auto& texture:material.textures) {
      require(texture.image==0&&texture.texcoord==1,"five slots and transform UV override");
      require(texture.wrap_s==33071&&texture.wrap_t==33648&&texture.min_filter==9728&&texture.mag_filter==9728,"sampler semantics");
      require(near(texture.transform[0],0)&&near(texture.transform[1],-3)&&near(texture.transform[2],.2f)&&
          near(texture.transform[3],2)&&near(texture.transform[4],0)&&near(texture.transform[5],.3f),"texture rotation scale offset");
    }
    MapModel catalog;
    require(load_map_texture_catalog(root/"pbr.glb",catalog,error),"texture-only catalog loaded");
    require(catalog.vertices.empty() && catalog.indices.empty(),"texture catalog allocates no geometry");
    require(catalog.images.size()==model.images.size() && catalog.images[0].bytes==model.images[0].bytes,"catalog encoded images match production");
    require(catalog.primitives.size()==model.primitives.size() && near(catalog.primitives[0].material.base_color[3],material.base_color[3]),"catalog material factors match production");
    require(!load_map_texture_catalog(root/"morph.glb",catalog,error),"catalog retains unsupported morph rejection");
    if(argc==3) {
      require(load_map_model(argv[2],model,error),error.c_str());
      std::printf("map_asset_loaded vertices=%zu triangles=%zu primitives=%zu images=%zu\n",
          model.vertices.size(),model.indices.size()/3,model.primitives.size(),model.images.size());
    }
    std::printf("map_loader_checks=%u passed\n",checks);
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"map_loader_failed checks=%u reason=%s\n",checks,error.what());
    return 1;
  }
}
