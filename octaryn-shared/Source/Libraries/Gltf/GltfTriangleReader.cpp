#include "GltfTriangleReader.h"
#include "FilePath.h"
#include "GltfMappedViews.h"
#include <fastgltf/tools.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace octaryn::assets {
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
std::array<float,6> empty_bounds() {
  const auto far=std::numeric_limits<float>::max();return {far,far,far,-far,-far,-far};
}
void grow(std::array<float,6>& bounds,const float* position) {
  for(unsigned axis=0;axis<3;++axis) {
    require(std::isfinite(position[axis]),"collision source position is not finite");
    bounds[axis]=std::min(bounds[axis],position[axis]);bounds[axis+3]=std::max(bounds[axis+3],position[axis]);
  }
}
}
struct GltfTriangleReader::State {
  fastgltf::MappedGltfFile source;
  fastgltf::Asset asset;
  GltfMappedViews views;
  const std::atomic_bool* cancel;
  std::uint32_t mesh{UINT32_MAX};
  State(const std::filesystem::path& parent,const std::filesystem::path& scratch,const std::atomic_bool* cancellation)
      :views(parent,scratch,512ull*1024*1024,cancellation),cancel(cancellation) {}
  template<class Visitor> void visit(std::uint32_t mesh_id,std::uint32_t primitive_id,std::uint64_t first,
      std::uint32_t count,std::span<const std::uint64_t> order,Visitor&& visitor) {
    require(count && count<=65536,"collision triangle window must contain 1..65536 triangles");
    require(mesh_id<asset.meshes.size() && primitive_id<asset.meshes[mesh_id].primitives.size(),"collision primitive reference invalid");
    if(mesh!=mesh_id) {views.clear();mesh=mesh_id;}
    const auto& primitive=asset.meshes[mesh].primitives[primitive_id];
    require(primitive.targets.empty(),"static collision cannot discard morph targets");
    const auto position=primitive.findAttribute("POSITION");
    require(position!=primitive.attributes.end() && position->accessorIndex<asset.accessors.size(),"collision positions missing");
    const auto& positions=asset.accessors[position->accessorIndex];
    require(positions.type==fastgltf::AccessorType::Vec3 && positions.count<=UINT32_MAX,"collision position accessor invalid");
    require(!primitive.indicesAccessor || *primitive.indicesAccessor<asset.accessors.size(),"collision index accessor reference invalid");
    const auto* indices=primitive.indicesAccessor?&asset.accessors[*primitive.indicesAccessor]:nullptr;
    require(!indices || indices->type==fastgltf::AccessorType::Scalar,"collision index accessor invalid");
    const auto elements=indices?indices->count:positions.count;
    require(elements>=3,"collision primitive contains no triangles");
    const bool list=primitive.type==fastgltf::PrimitiveType::Triangles;
    require(list || primitive.type==fastgltf::PrimitiveType::TriangleStrip || primitive.type==fastgltf::PrimitiveType::TriangleFan,
        "collision requires triangle topology");
    require(!list || elements%3==0,"collision index count is not triangular");
    const auto triangles=list?elements/3:elements-2;
    require(first<=triangles && count<=triangles-first,"collision triangle window exceeds source");
    for(const auto triangle:order)require(triangle<triangles,"collision triangle gather exceeds source");
    const auto index=[&](std::uint64_t offset) {
      return indices?fastgltf::getAccessorElement<std::uint32_t>(asset,*indices,std::size_t(offset),views):std::uint32_t(offset);
    };
    for(std::uint64_t offset=0;offset<count;++offset) {
      const auto triangle=order.empty()?first+offset:order[offset];
      require(!cancel || !cancel->load(std::memory_order_relaxed),"collision source preparation canceled");
      std::array<std::uint32_t,3> ids;
      if(list)ids={index(triangle*3),index(triangle*3+1),index(triangle*3+2)};
      else if(primitive.type==fastgltf::PrimitiveType::TriangleStrip)
        ids={index(triangle),index(triangle+1+triangle%2),index(triangle+2-triangle%2)};
      else ids={index(0),index(triangle+1),index(triangle+2)};
      for(const auto id:ids) {
        require(id<positions.count,"collision index exceeds positions");
        const auto point=fastgltf::getAccessorElement<fastgltf::math::fvec3>(asset,positions,id,views);
        const float values[3]{point[0],point[1],point[2]};visitor(id,values);
      }
    }
  }
};
GltfTriangleReader::GltfTriangleReader()=default;
GltfTriangleReader::~GltfTriangleReader()=default;
bool GltfTriangleReader::open(const std::filesystem::path& source,const std::filesystem::path& scratch,
    std::string& error,const std::atomic_bool* cancel) {
  try {
    require(std::filesystem::file_size(content::file_io_path(source))<=64ull*1024*1024,"collision source metadata exceeds 64MiB");
    auto next=std::make_unique<State>(source.parent_path(),scratch,cancel);
    auto mapped=fastgltf::MappedGltfFile::FromPath(content::file_io_path(source));
    require(mapped.error()==fastgltf::Error::None,"collision source metadata cannot be mapped");
    next->source=std::move(mapped.get());
    fastgltf::Parser parser(fastgltf::Extensions::EXT_meshopt_compression | fastgltf::Extensions::KHR_texture_transform |
        fastgltf::Extensions::KHR_materials_emissive_strength);
    auto parsed=parser.loadGltf(next->source,source.parent_path(),fastgltf::Options::None);
    require(parsed.error()==fastgltf::Error::None,"collision source metadata invalid");next->asset=std::move(parsed.get());
    validate_gltf_accessors(next->asset);
    require(fastgltf::validate(next->asset)==fastgltf::Error::None,"collision source validation failed");
    require(next->asset.meshes.size()<=1000000,"collision source mesh limit exceeded");
    state_=std::move(next);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool GltfTriangleReader::read(std::uint32_t mesh,std::uint32_t primitive,std::uint64_t first,
    std::uint32_t count,GltfTriangleWindow& output,std::string& error) {
  try {
    require(bool(state_),"collision source reader is not open");
    GltfTriangleWindow result;result.bounds=empty_bounds();
    std::unordered_map<std::uint32_t,std::uint32_t> remap;
    state_->visit(mesh,primitive,first,count,{},[&](std::uint32_t id,const float* position) {
      auto [entry,fresh]=remap.try_emplace(id,std::uint32_t(result.positions.size()/3));
      if(fresh) {grow(result.bounds,position);result.positions.insert(result.positions.end(),position,position+3);}
      result.indices.push_back(entry->second);
    });
    output=std::move(result);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool GltfTriangleReader::read(std::uint32_t mesh,std::uint32_t primitive,std::span<const std::uint64_t> triangles,
    GltfTriangleWindow& output,std::string& error) {
  try {
    require(bool(state_),"collision source reader is not open");require(triangles.size()<=65536,"collision triangle gather exceeds window bound");
    GltfTriangleWindow result;result.bounds=empty_bounds();std::unordered_map<std::uint32_t,std::uint32_t> remap;
    state_->visit(mesh,primitive,0,std::uint32_t(triangles.size()),triangles,[&](std::uint32_t id,const float* position) {
      auto [entry,fresh]=remap.try_emplace(id,std::uint32_t(result.positions.size()/3));
      if(fresh) {grow(result.bounds,position);result.positions.insert(result.positions.end(),position,position+3);}
      result.indices.push_back(entry->second);
    });
    output=std::move(result);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool GltfTriangleReader::bounds(std::uint32_t mesh,std::uint32_t primitive,std::uint64_t first,
    std::uint32_t count,std::array<float,6>& output,std::string& error) {
  try {
    require(bool(state_),"collision source reader is not open");auto result=empty_bounds();
    state_->visit(mesh,primitive,first,count,{},[&](std::uint32_t,const float* position){grow(result,position);});
    output=result;error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool transform_gltf_triangles(GltfTriangleWindow& window,const std::array<float,16>& m,std::string& error) {
  try {
    require(window.positions.size()%3==0 && window.indices.size()%3==0,"collision window topology invalid");
    for(const auto index:window.indices)require(index<window.positions.size()/3,"collision window index invalid");
    for(const auto value:m)require(std::isfinite(value),"collision instance transform is not finite");
    const double determinant=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-
        double(m[4])*(double(m[1])*m[10]-double(m[2])*m[9])+double(m[8])*(double(m[1])*m[6]-double(m[2])*m[5]);
    require(m[3]==0 && m[7]==0 && m[11]==0 && m[15]==1 && std::isfinite(determinant) && std::abs(determinant)>1e-20,
        "collision instance transform must be invertible affine");
    auto result=window;result.bounds=empty_bounds();
    for(std::size_t vertex=0;vertex<result.positions.size();vertex+=3) {
      const float x=window.positions[vertex],y=window.positions[vertex+1],z=window.positions[vertex+2];
      for(unsigned axis=0;axis<3;++axis)result.positions[vertex+axis]=m[axis]*x+m[4+axis]*y+m[8+axis]*z+m[12+axis];
      grow(result.bounds,result.positions.data()+vertex);
    }
    if(determinant<0)for(std::size_t index=0;index<result.indices.size();index+=3)std::swap(result.indices[index+1],result.indices[index+2]);
    window=std::move(result);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
