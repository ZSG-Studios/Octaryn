#include "SceneOrder.h"
#include "MapSourceReader.h"

namespace octaryn::client::rendering::virtual_geometry {
bool load_scene_part(MapSourceReader& reader,const std::filesystem::path& catalog_path,const SceneCatalog& catalog,
    const ScenePart& part,MapModel& model,std::string& error) {
  const auto& primitive=catalog.primitives.at(part.primitive);
  std::vector<std::uint64_t> order;if(!scene_order_triangles(catalog_path,catalog,part,order,error))return false;
  return order.empty()?reader.load(primitive.mesh,primitive.primitive,part.first_triangle,std::size_t(part.triangle_count),model,error):
      reader.load(primitive.mesh,primitive.primitive,order,model,error);
}
}
