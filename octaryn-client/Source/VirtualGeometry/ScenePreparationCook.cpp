#include "ScenePreparationInternal.h"
#include "GeometryCook.h"
#include "GeometryCache.h"
#include "MapTextureCache.h"
#include "SceneOrder.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
std::string part_hash(const SceneCatalog& catalog,const ScenePart& part) {
  const auto& primitive=catalog.primitives[part.primitive];
  const auto identity=catalog.source_hash+":"+std::to_string(geometry_version)+":"+std::to_string(primitive.mesh)+":"+
      std::to_string(primitive.primitive)+":"+std::to_string(part.first_triangle)+":"+std::to_string(part.triangle_count)+
      (primitive.position_only?":flat-position":":full-attributes")+
      (primitive.triangle_order.empty()?"":":order="+primitive.triangle_order_hash);
  return map_texture_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
}
void metadata(ScenePart& part,const GeometryAsset& asset,const std::filesystem::path& relative) {
  part.geometry=relative.generic_string();part.hash=asset.source_hash;
  part.clusters=unsigned(asset.clusters.size());part.pages=unsigned(asset.pages.size());
  std::vector<bool> pinned(asset.pages.size());
  for(const auto root:asset.roots) {
    const auto& group=asset.groups[root];
    for(unsigned i=0;i<group.page_count;++i)pinned[asset.group_pages[group.first_page+i]]=true;
  }
  part.root_pages=unsigned(std::count(pinned.begin(),pinned.end(),true));
}
}
void ScenePreparationWork::cook(std::span<const std::uint32_t> parts) {
  bounds(parts);bounds_reader.reset();
  std::uint64_t completed{};report(ScenePreparationStage::Geometry,0,parts.size());
  for(const auto id:parts) {
    check();auto& part=catalog.parts.at(id);const auto& primitive=catalog.primitives[part.primitive];
    const auto hash=part_hash(catalog,part);const auto relative=std::filesystem::path("geometry")/(hash+".vgeom");
    GeometryAsset asset;std::string error;
    if(!read_geometry_cache(path.parent_path()/relative,hash,asset,error,true) || asset.space!=GeometrySpace::Object ||
        asset.source_triangles!=part.triangle_count || asset.material_count!=1) {
      if(!reader) {
        reader=std::make_unique<MapSourceReader>();
        if(!reader->open(preparation_path(catalog.source),path.parent_path()/"scratch",error,cancel))throw std::runtime_error(error);
      }
      MapModel model;
      if(!load_scene_part(*reader,path,catalog,part,model,error))throw std::runtime_error(error);
      if(model.indices.size()/3!=part.triangle_count || model.primitives.size()!=1)throw std::runtime_error("scene source window is incomplete");
      check();if(!cook_geometry(model,hash,asset,error,{primitive.position_only}))throw std::runtime_error(error);
      check();asset.space=GeometrySpace::Object;
      if(!write_geometry_cache(path.parent_path()/relative,asset,error))throw std::runtime_error(error);
    }
    metadata(part,asset,relative);modified=true;checkpoint();report(ScenePreparationStage::Geometry,++completed,parts.size());
  }
  checkpoint(true);
}
}
