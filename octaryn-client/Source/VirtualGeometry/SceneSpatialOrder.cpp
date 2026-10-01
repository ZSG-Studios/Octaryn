#include "SceneOrder.h"
#include "ScenePreparationInternal.h"
#include "ScenePreparationLock.h"
#include "SceneResourceHash.h"
#include "MapTextureCache.h"
#include "FilePath.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
bool triangle_records(assets::GltfTriangleReader& reader,const ScenePrimitive& primitive,std::uint64_t first,
    std::span<scene_geometry::SpatialTriangle> output,std::string& error) {
  assets::GltfTriangleWindow window;
  if(!reader.read(primitive.mesh,primitive.primitive,first,std::uint32_t(output.size()),window,error))return false;
  if(window.indices.size()!=output.size()*3) {error="spatial source triangle window is incomplete";return false;}
  for(std::size_t triangle=0;triangle<output.size();++triangle) {
    auto& record=output[triangle];const auto extent_limit=std::numeric_limits<float>::max();
    record.bounds={extent_limit,extent_limit,extent_limit,-extent_limit,-extent_limit,-extent_limit};
    for(unsigned axis=0;axis<3;++axis) {
      double sum{};
      for(unsigned corner=0;corner<3;++corner) {
        const auto value=window.positions.at(std::size_t(window.indices[triangle*3+corner])*3+axis);sum+=value;
        record.bounds[axis]=std::min(record.bounds[axis],value);record.bounds[axis+3]=std::max(record.bounds[axis+3],value);
      }
      record.centroid[axis]=float(sum/3);
    }
  }
  return true;
}
}
bool prepare_scene_spatial_order(const std::filesystem::path& catalog_path,std::uint64_t first,std::uint64_t count,
    std::string& error,const std::atomic_bool* cancel,bool multipart_only,
    std::function<void(std::uint64_t,std::uint64_t)> notify) {
  try {
    ScenePreparationLock lock(catalog_path);ScenePreparationWork work;work.path=catalog_path;work.cancel=cancel;
    if(!read_scene_catalog(catalog_path,work.catalog,error))return false;
    if(first>work.catalog.primitives.size())throw std::runtime_error("scene spatial primitive range is invalid");
    count=std::min<std::uint64_t>(count,work.catalog.primitives.size()-first);work.verify_resources();
    std::uint64_t completed{},total{};
    for(auto i=first;i<first+count;++i)total+=!multipart_only || work.catalog.primitives[i].part_count>1;
    if(notify)notify(0,total);
    assets::GltfTriangleReader reader;
    if(!reader.open(preparation_path(work.catalog.source),catalog_path.parent_path()/"scratch",error,cancel))return false;
    for(std::uint64_t index=first;index<first+count;++index) {
      work.check();auto& primitive=work.catalog.primitives[index];
      if(multipart_only && primitive.part_count<=1)continue;
      if(!primitive.triangle_order.empty()) {if(notify)notify(++completed,total);continue;}
      const auto config=scene_order_config(work.catalog,primitive);
      const auto identity=work.catalog.source_hash+":spatial-order:1:"+std::to_string(primitive.mesh)+":"+std::to_string(primitive.primitive);
      const auto key=map_texture_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
      const auto relative=std::filesystem::path("orders")/(key+".triangles");std::filesystem::path output;
      if(!scene_geometry::spatial_triangle_order_path(catalog_path,relative,output,error))return false;
      scene_geometry::SpatialOrderResult result;
      if(!std::filesystem::exists(content::file_io_path(output))) {
        const auto read=[&](auto offset,auto records,auto& message){return triangle_records(reader,primitive,offset,records,message);};
        if(!scene_geometry::write_spatial_triangle_order(output,config,read,result,error,cancel))return false;
      } else {
        if(!scene_geometry::validate_spatial_triangle_order(output,config,error,cancel))return false;
        for(unsigned part=0;part<primitive.part_count;++part) {
          work.check();const auto& current=work.catalog.parts[primitive.first_part+part];std::vector<std::uint64_t> ids;
          if(!scene_geometry::read_spatial_triangle_order(output,config,current.first_triangle,std::uint32_t(current.triangle_count),ids,error))return false;
          assets::GltfTriangleWindow window;if(!reader.read(primitive.mesh,primitive.primitive,ids,window,error))return false;
          result.parts.push_back({current.first_triangle,current.triangle_count,window.bounds});
        }
      }
      if(result.parts.size()!=primitive.part_count)throw std::runtime_error("spatial order changed source triangle coverage");
      const auto hash=scene_resource_hash(output,error,cancel);if(hash.empty())return false;
      for(unsigned part=0;part<primitive.part_count;++part) {
        auto& current=work.catalog.parts[primitive.first_part+part];const auto& ordered=result.parts[part];
        if(current.first_triangle!=ordered.first_triangle || current.triangle_count!=ordered.triangle_count)
          throw std::runtime_error("spatial order changed part coverage");
        current.bounds=ordered.bounds;current.bounds_prepared=true;
        current.geometry.clear();current.hash.clear();current.clusters=current.pages=current.root_pages=0;
      }
      primitive.triangle_order=relative.generic_string();primitive.triangle_order_hash=hash;work.modified=true;work.checkpoint(true);
      std::printf("scene_spatial_order_prepared primitive=%llu mesh=%u source_primitive=%u triangles=%llu parts=%u peak_scratch=%llu complete_source=1 geometry_ready=0\n",
          static_cast<unsigned long long>(index),primitive.mesh,primitive.primitive,static_cast<unsigned long long>(primitive.triangles),primitive.part_count,
          static_cast<unsigned long long>(result.peak_scratch_bytes));
      if(notify)notify(++completed,total);
    }
    error.clear();return true;
  }catch(const std::exception& failure){error=failure.what();return false;}
}
}
