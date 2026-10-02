#include "MapSourceReader.h"
#include "FilePath.h"
#include "MapMaterials.h"
#include "MapZeroBasis.h"
#include "GltfMappedViews.h"
#include <fastgltf/tools.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
using namespace fastgltf;
using namespace fastgltf::math;
void require(bool valid,const char* reason) {if(!valid)throw std::runtime_error(reason);}
fvec3 normal(const fvec3& value) {
  const auto maximum=std::max({std::abs(value[0]),std::abs(value[1]),std::abs(value[2])});
  require(std::isfinite(maximum),"nonfinite source normal");
  if(maximum==0)return {0,1,0};
  const auto scaled=value/maximum;return scaled/std::sqrt(dot(scaled,scaled));
}
const Accessor* attribute(const Asset& asset,const Primitive& primitive,const char* name) {
  const auto found=primitive.findAttribute(name);if(found==primitive.attributes.end())return nullptr;
  require(found->accessorIndex<asset.accessors.size(),"source attribute accessor out of range");
  return &asset.accessors[found->accessorIndex];
}
template<class T> T element(const Asset& asset,const Accessor& accessor,std::size_t index,
    AccessorType type,const octaryn::assets::GltfMappedViews& views) {
  require(accessor.type==type && index<accessor.count,"source accessor shape or index invalid");
  return getAccessorElement<T>(asset,accessor,index,views);
}
struct VertexReader {
  const Asset& asset;const octaryn::assets::GltfMappedViews& views;
  const Accessor *positions,*normals,*uv0,*uv1,*tangents,*colors,*blend0,*blend1,*raw_t,*raw_b;
  bool zero_basis{};
  unsigned layer_count{};
  VertexReader(const Asset& data,const Primitive& primitive,const octaryn::assets::GltfMappedViews& buffers,unsigned layers,bool zero):asset(data),views(buffers),
      positions(attribute(data,primitive,"POSITION")),normals(attribute(data,primitive,"NORMAL")),
      uv0(attribute(data,primitive,"TEXCOORD_0")),uv1(attribute(data,primitive,"TEXCOORD_1")),
      tangents(attribute(data,primitive,"TANGENT")),colors(attribute(data,primitive,"COLOR_0")),
      blend0(attribute(data,primitive,"_OCTARYN_BLEND0")),blend1(attribute(data,primitive,"_OCTARYN_BLEND1")),
      raw_t(attribute(data,primitive,"_OCTARYN_SOURCE_TANGENT")),raw_b(attribute(data,primitive,"_OCTARYN_SOURCE_BITANGENT")),zero_basis(zero),layer_count(layers) {
    require(zero?(raw_t && raw_b && normals && uv0 && !tangents && !layers):(!raw_t && !raw_b),"declared source basis attributes and material must agree");
    for(const auto* a:{raw_t,raw_b})if(a)require(a->type==AccessorType::Vec3 && a->componentType==ComponentType::Float && !a->normalized,"source basis requires float32 vec3");
    require(layers?(blend0 && blend1):(!blend0 && !blend1),"weighted layer attributes and material must be declared together");
    for(const auto* a:{blend0,blend1})if(a)require(a->type==AccessorType::Vec4 && a->componentType==ComponentType::Float && !a->normalized,"layer weights require float32 vec4");
    require(positions && positions->type==AccessorType::Vec3,"source primitive has no positions");
    for(const auto* a:{normals,uv0,uv1,tangents,colors,blend0,blend1,raw_t,raw_b})require(!a || a->count==positions->count,"source attribute count mismatch");
  }
  MapVertex operator()(std::size_t index) const {
    MapVertex vertex{};const auto p=element<fvec3>(asset,*positions,index,AccessorType::Vec3,views);
    const auto n=normals?normal(element<fvec3>(asset,*normals,index,AccessorType::Vec3,views)):fvec3(0,1,0);
    for(unsigned axis=0;axis<3;++axis) {
      require(std::isfinite(p[axis]),"nonfinite source position");vertex.position[axis]=p[axis];vertex.normal[axis]=n[axis];
    }
    for(unsigned set=0;set<2;++set)if(const auto* accessor=set?uv1:uv0) {
      const auto uv=element<fvec2>(asset,*accessor,index,AccessorType::Vec2,views);auto* target=set?vertex.uv1:vertex.uv;
      for(unsigned axis=0;axis<2;++axis) {require(std::isfinite(uv[axis]),"nonfinite source UV");target[axis]=uv[axis];}
    }
    if(tangents && normals) {
      const auto t=element<fvec4>(asset,*tangents,index,AccessorType::Vec4,views);
      require(t[3]==-1 || t[3]==1,"invalid source tangent handedness");
      const auto direction=fvec3(t[0],t[1],t[2]);const auto unit=normal(direction-n*dot(n,direction));
      for(unsigned axis=0;axis<3;++axis)vertex.tangent[axis]=unit[axis];vertex.tangent[3]=t[3];
    }
    if(zero_basis) {
      const auto t=element<fvec3>(asset,*raw_t,index,AccessorType::Vec3,views),b=element<fvec3>(asset,*raw_b,index,AccessorType::Vec3,views);
      const auto original=element<fvec3>(asset,*normals,index,AccessorType::Vec3,views);
      require(dot(original,original)>=1e-16f,"zero basis requires nonzero authored normal");
      for(unsigned axis=0;axis<3;++axis) {require(t[axis]==0 && b[axis]==0,"zero basis requires exact zero source T/B");vertex.tangent[axis]=t[axis];vertex.blend0[axis]=b[axis];}
    }
    if(colors) {
      fvec4 value;
      if(colors->type==AccessorType::Vec3) {
        const auto rgb=element<fvec3>(asset,*colors,index,AccessorType::Vec3,views);value={rgb[0],rgb[1],rgb[2],1};
      } else value=element<fvec4>(asset,*colors,index,AccessorType::Vec4,views);
      for(unsigned axis=0;axis<4;++axis) {require(std::isfinite(value[axis]),"nonfinite source color");vertex.color[axis]=value[axis];}
    }
    if(layer_count)for(unsigned half=0;half<2;++half) {
      const auto value=element<fvec4>(asset,*(half?blend1:blend0),index,AccessorType::Vec4,views);
      for(unsigned lane=0;lane<4;++lane) {
        require(std::isfinite(value[lane]) && value[lane]>=0 && (half*4+lane<layer_count || value[lane]==0),"invalid authored layer weight");
        (half?vertex.blend1:vertex.blend0)[lane]=value[lane];
      }
    }
    return vertex;
  }
};
}
struct MapSourceReader::State {
  fastgltf::MappedGltfFile source;
  fastgltf::Asset asset;
  MapLayerImport layers;
  MapSourceInfo info;
  octaryn::assets::GltfMappedViews buffers;
  const std::atomic_bool* cancel{};
  std::size_t current_mesh{SIZE_MAX};
  State(const std::filesystem::path& parent,const std::filesystem::path& scratch,const std::atomic_bool* cancellation)
      :buffers(parent,scratch,512ull*1024*1024,cancellation),cancel(cancellation) {}
};
MapSourceReader::MapSourceReader()=default;
MapSourceReader::~MapSourceReader()=default;
const MapSourceInfo& MapSourceReader::info() const {
  if(!state_)throw std::runtime_error("source reader is not open");return state_->info;
}
bool MapSourceReader::open(const std::filesystem::path& path,const std::filesystem::path& scratch,std::string& error,
    const std::atomic_bool* cancel) {
  try {
    require(!cancel || !cancel->load(),"source preparation canceled");
    auto next=std::make_unique<State>(path.parent_path(),scratch,cancel);
    if(!inspect_map_source(path,next->info,error))return false;
    auto source=fastgltf::MappedGltfFile::FromPath(content::file_io_path(path));require(source.error()==Error::None,"cannot map source metadata");
    next->source=std::move(source.get());
    Parser parser(Extensions::EXT_meshopt_compression | Extensions::KHR_texture_transform | Extensions::KHR_materials_emissive_strength | Extensions::KHR_materials_unlit);
    next->layers.bind(parser);auto loaded=parser.loadGltf(next->source,path.parent_path(),Options::None);
    require(loaded.error()==Error::None,"cannot parse source metadata");next->layers.validate();next->asset=std::move(loaded.get());
    octaryn::assets::validate_gltf_accessors(next->asset);
    require(validate(next->asset)==Error::None,"invalid source metadata");
    state_=std::move(next);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool MapSourceReader::load(std::size_t mesh,std::size_t primitive,std::uint64_t first,
    std::size_t count,MapModel& output,std::string& error) {
  return load_triangles(mesh,primitive,first,count,{},output,error);
}
bool MapSourceReader::load(std::size_t mesh,std::size_t primitive,std::span<const std::uint64_t> triangles,
    MapModel& output,std::string& error) {
  return load_triangles(mesh,primitive,0,triangles.size(),triangles,output,error);
}
bool MapSourceReader::load_triangles(std::size_t mesh,std::size_t primitive,std::uint64_t first,std::size_t count,
    std::span<const std::uint64_t> order,MapModel& output,std::string& error) {
  try {
    require(bool(state_),"source reader is not open");auto& state=*state_;
    require(count>0 && count<=65536,"source triangle window must contain 1 to 65536 triangles");
    require(mesh<state.asset.meshes.size() && primitive<state.asset.meshes[mesh].primitives.size(),"source primitive out of range");
    const auto& source=state.asset.meshes[mesh].primitives[primitive];
    require(source.targets.empty(),"source cook requires static geometry");
    const auto info=std::find_if(state.info.primitives.begin(),state.info.primitives.end(),[&](const auto& p){return p.mesh==mesh && p.primitive==primitive;});
    require(info!=state.info.primitives.end() && first<=info->triangles && count<=info->triangles-first,"source triangle range out of bounds");
    for(auto triangle:order)require(triangle<info->triangles,"source triangle gather index out of bounds");
    if(state.current_mesh!=mesh) {state.buffers.clear();state.current_mesh=mesh;}
    MapModel model;MapPrimitive draw;
    draw.material=load_map_material(state.asset,source,&state.layers);
    VertexReader read(state.asset,source,state.buffers,draw.material.layer_count,draw.material.zero_basis);draw.index_count=static_cast<std::uint32_t>(count*3);
    for(const auto& texture:draw.material.textures)require(texture.image<0 || (texture.texcoord==0?read.uv0:read.uv1),"source texture has no UV attribute");
    std::fill_n(draw.bounds_min,3,1e30f);std::fill_n(draw.bounds_max,3,-1e30f);
    std::map<std::uint32_t,std::uint32_t> remap;
    const auto index=[&](std::uint64_t offset) {
      if(!source.indicesAccessor) {require(offset<=UINT32_MAX,"source vertex index exceeds 32-bit range");return std::uint32_t(offset);}
      return element<std::uint32_t>(state.asset,state.asset.accessors[*source.indicesAccessor],std::size_t(offset),AccessorType::Scalar,state.buffers);
    };
    for(std::uint64_t offset=0;offset<count;++offset) {
      const auto triangle=order.empty()?first+offset:order[offset];
      require(!state.cancel || !state.cancel->load(std::memory_order_relaxed),"source preparation canceled");
      std::array<std::uint32_t,3> indices;
      if(source.type==PrimitiveType::Triangles)indices={index(triangle*3),index(triangle*3+1),index(triangle*3+2)};
      else if(source.type==PrimitiveType::TriangleStrip)indices={index(triangle),index(triangle+1+triangle%2),index(triangle+2-triangle%2)};
      else {require(source.type==PrimitiveType::TriangleFan,"source topology is not triangles");indices={index(0),index(triangle+1),index(triangle+2)};}
      std::array<MapVertex,3> corners;
      for(unsigned corner=0;corner<3;++corner)corners[corner]=read(indices[corner]);
      if(draw.material.zero_basis) {
        const auto* a=corners[0].uv;const auto* b=corners[1].uv;const auto* c=corners[2].uv;
        require((double(b[0])-a[0])*(double(c[1])-a[1])-(double(c[0])-a[0])*(double(b[1])-a[1])==0,"zero basis requires degenerate source UV triangle");
      }
      if(!read.normals) {
        const auto point=[&](unsigned corner){const auto* p=corners[corner].position;return fvec3(p[0],p[1],p[2]);};
        const auto n=normal(cross(point(1)-point(0),point(2)-point(0)));
        for(auto& vertex:corners)for(unsigned axis=0;axis<3;++axis)vertex.normal[axis]=n[axis];
      }
      for(unsigned corner=0;corner<3;++corner) {
        const auto& vertex=corners[corner];auto target=static_cast<std::uint32_t>(model.vertices.size());bool fresh=true;
        if(read.normals) {auto [entry,inserted]=remap.emplace(indices[corner],target);target=entry->second;fresh=inserted;}
        if(fresh)model.vertices.push_back(vertex);model.indices.push_back(target);
        for(unsigned axis=0;axis<3;++axis) {
          draw.bounds_min[axis]=std::min(draw.bounds_min[axis],vertex.position[axis]);
          draw.bounds_max[axis]=std::max(draw.bounds_max[axis],vertex.position[axis]);
        }
      }
    }
    model.primitives.push_back(draw);output=std::move(model);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
