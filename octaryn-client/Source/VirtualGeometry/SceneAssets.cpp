#include "SceneAssets.h"
#include "FilePath.h"
#include "SceneResourceHash.h"
#include "SceneBudget.h"
#include "SceneOrder.h"
#include "SceneForward.h"
#include "WorldGeometry.h"
#include "WorldGeometryRay.h"
#include "SceneMemoryLedger.h"
#include "GeometryCoarse.h"
#include "../MapWorld/MapAssetBuildInternal.h"
#include "../MapWorld/MapSourceReader.h"
#include "../MapWorld/MapMipmaps.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include "octaryn_native_schedule_runtime.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
using namespace virtual_geometry;
constexpr std::uint64_t material_budget=512ull*1024*1024;
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
std::filesystem::path path(const std::string& value) {return std::filesystem::u8path(value);}
MapGeometryCache cache(const std::filesystem::path& catalog,const ScenePart& part) {
  return {catalog.parent_path()/path(part.geometry),part.hash,part.pages,part.clusters,part.root_pages};
}
std::uint64_t check_images(const MapModel& model) {
  std::set<std::pair<std::size_t,MapMipOptions>> variants;
  for(const auto& primitive:model.primitives)for(unsigned role=0;role<5;++role) {
    const auto image=primitive.material.textures[role].image;
    if(image>=0)variants.emplace(std::size_t(image),map_mip_options(primitive.material,role));
  }
  std::uint64_t bytes{};
  for(std::size_t image=0;image<model.images.size();++image) {
    const auto count=std::count_if(variants.begin(),variants.end(),[&](const auto& item){return item.first==image;});
    if(!count)continue;
    MapDecodedImage decoded;std::string error;
    if(!decode_map_image(model.images[image],decoded,error))throw std::runtime_error(error);
    const auto required=std::uint64_t(decoded.width)*decoded.height*8*count;
    require(required<=material_budget-bytes,"scene texture preparation budget exceeded");bytes+=required;
  }
  return bytes;
}
}
SceneAssets::SceneAssets()=default;
SceneAssets::~SceneAssets()=default;
std::uint64_t SceneAssets::texture_bytes() const {return materials_?materials_->texture_bytes:0;}
bool SceneAssets::load(WorldRenderer& renderer,const std::filesystem::path& catalog_path,
    const std::filesystem::path& source,std::string& error) {
  try {
    if(!read_scene_catalog(catalog_path,catalog_,error))return false;
    require(std::filesystem::equivalent(content::file_io_path(source),content::file_io_path(path(catalog_.source))),"scene catalog belongs to another source");
    for(const auto& resource:catalog_.resources) {
      require(std::filesystem::file_size(content::file_io_path(path(resource.path)))==resource.bytes,"scene source resource size changed");
      require(scene_resource_hash(path(resource.path),error)==resource.hash,"scene source resource content changed");
    }
    if(!verify_scene_orders(catalog_path,catalog_,error))return false;
    catalog_path_=catalog_path;source_=source;
    page_scheduler_=std::shared_ptr<void>(octaryn_native_schedule_runtime_create(2,2),octaryn_native_schedule_runtime_destroy);
    require(bool(page_scheduler_),"scene page scheduler creation failed");
    mesh_nodes_.resize(catalog_.mesh_count);
    for(const auto& instance:catalog_.instances) {
      GeometryTransform transform;
      if(!geometry_transform(instance.transform,transform,error))return false;
      mesh_nodes_[instance.mesh].push_back(std::uint32_t(nodes_.size()));
      transforms_.push_back(transform);nodes_.push_back({instance.node,instance.mesh,instance.transform,instance.bounds});
    }
    std::vector<MapMaterial> materials;
    for(const auto& primitive:catalog_.primitives)materials.push_back(primitive.surface);
    if(!load_render_parts(error))return false;
    materials_=std::make_unique<MapRenderer>();auto& resources=*materials_;resources.device=renderer.device;
    MapLoadLimits limits;limits.source_bytes=64ull*1024*1024;limits.encoded_bytes=64ull*1024*1024;
    if(!load_map_material_resources(source,materials,resources.model,error,limits))return false;
    const auto image_bytes=check_images(resources.model);
    textures_allocation_=renderer.scene_memory->reserve(image_bytes,SceneMemoryDomain::Materials);
    require(bool(textures_allocation_),"scene textures exceed aggregate GPU budget");
    resources.texture_allocation=textures_allocation_;resources.texture_cache_directory=catalog_path.parent_path()/"textures";
    require(upload_map_images(resources),"scene image upload failed");
    for(std::size_t index=0;index<resources.textures.size();++index) {
      auto resource=std::make_shared<MapTextureResource>();resource->texture=resources.textures[index];
      resource->view=resources.texture_views[index];resource->ready=true;
      const auto& desc=resource->texture->getDesc();const auto& format=rhi::getFormatInfo(desc.format);
      for(unsigned mip=0;mip<desc.mipCount;++mip)
        resource->bytes+=std::uint64_t((std::max(1u,desc.size.width>>mip)+format.blockWidth-1)/format.blockWidth)*
            ((std::max(1u,desc.size.height>>mip)+format.blockHeight-1)/format.blockHeight)*format.blockSizeInBytes;
      resources.texture_resources.push_back(std::move(resource));
    }
    require(textures_allocation_->resize(resources.texture_bytes),"scene texture allocation exceeded preparation estimate");
    if(!initialize_shared(renderer,error))return false;
    error.clear();return true;
  }catch(const std::exception& failure){error=failure.what();return false;}
}
bool SceneAssets::prepare(std::uint32_t id,PreparedScenePart& prepared,std::string& error) const {
  try {
    const auto& render=render_parts_.at(id);const auto& part=render.geometry;
    const auto& primitive=catalog_.primitives[render.primitive];
    require(!part.geometry.empty() && part.bounds_prepared,"scene part preparation is incomplete");
    GeometryAsset asset;const auto metadata=cache(render.hierarchy?hierarchy_path_:catalog_path_,part);
    if(!read_geometry_cache(metadata.path,metadata.hash,asset,error,false))return false;
    require(asset.space==GeometrySpace::Object && asset.material_count==1 && asset.source_triangles==part.triangle_count &&
        asset.pages.size()==part.pages && asset.clusters.size()==part.clusters,"scene part metadata does not match its cooked geometry");
    std::vector<bool> roots(asset.pages.size());
    for(const auto root:asset.roots) {
      const auto& group=asset.groups[root];
      for(unsigned page=0;page<group.page_count;++page)roots[asset.group_pages[group.first_page+page]]=true;
    }
    require(std::size_t(std::count(roots.begin(),roots.end(),true))==part.root_pages,"scene root page metadata differs from cooked geometry");
    if(primitive.surface.alpha_mode==MapAlphaMode::Blend) {
      if(render.hierarchy && !render.exact) {
        MapPrimitive draw;draw.material=primitive.surface;prepared.forward.primitives.push_back(draw);
        if(!append_geometry_roots(metadata.path,asset,prepared.forward,hierarchy_.maximum_triangles,error))return false;
      }else {
        MapSourceReader reader;
        if(!reader.open(source_,catalog_path_.parent_path()/"scratch",error) ||
            !load_scene_part(reader,catalog_path_,catalog_,render.hierarchy?catalog_.parts.at(render.node.leaf_part):part,prepared.forward,error))return false;
      }
    }
    error.clear();return true;
  }catch(const std::exception& failure){error=failure.what();return false;}
}
void SceneAssets::instances(MapRenderer& map,std::span<const std::uint32_t> nodes) const {
  map.geometry_instances.clear();map.geometry_instances.reserve(nodes.size());
  for(const auto node:nodes)map.geometry_instances.push_back(transforms_.at(node));
  ++map.geometry_instances_revision;
}
std::shared_ptr<MapRenderer> SceneAssets::create(WorldRenderer& renderer,const scene_geometry::Selection& selection,
    PreparedScenePart&& prepared,std::string& error,bool* deferred) const {
  if(deferred)*deferred=false;
  try {
    const auto& render=render_parts_.at(selection.part);const auto& part=render.geometry;
    const auto& primitive=catalog_.primitives[render.primitive];
    auto map=std::shared_ptr<MapRenderer>(new MapRenderer,destroy_map_renderer);map->device=renderer.device;
    map->geometry_cache=cache(render.hierarchy?hierarchy_path_:catalog_path_,part);instances(*map,selection.instances);
    map->scene_ray_scheduler=context_.ray;
    require(!map->geometry_instances.empty(),"scene part has no selected instances");
    MapPrimitive draw;draw.material=primitive.surface;draw.index_count=std::uint32_t(part.triangle_count*3);
    std::copy_n(part.bounds.begin(),3,draw.bounds_min);std::copy_n(part.bounds.begin()+3,3,draw.bounds_max);
    map->model.primitives.push_back(draw);map->ray_supported=renderer.device->hasFeature(rhi::Feature::AccelerationStructure);
    const auto& resources=*materials_;
    map->textures=resources.textures;map->texture_views=resources.texture_views;map->texture_resources=resources.texture_resources;
    map->material_texture_slots.push_back(resources.material_texture_slots[render.primitive]);
    map->sampler_cache=resources.sampler_cache;map->texture_bytes=resources.texture_bytes;
    map->material_samplers=resources.material_samplers;
    map->ray_primitives=resources.ray_primitives;map->material_allocation=resources.material_allocation;
    map->texture_allocation=resources.texture_allocation;
    map->material_buffer_range={std::uint64_t(render.primitive)*16*sizeof(MapRayMaterial),sizeof(MapRayMaterial)};
    if(primitive.surface.alpha_mode==MapAlphaMode::Blend) {
      map->model=std::move(prepared.forward);
      const auto bytes=map->model.vertices.size()*sizeof(MapVertex)+map->model.indices.size()*8;
      map->forward_allocation=context_.ledger->reserve(bytes,SceneMemoryDomain::Pages,SceneMemoryPhase::Pending);
      if(!map->forward_allocation) {if(deferred)*deferred=true;error="scene forward buffers exceed aggregate budget";return {};}
      if(!upload_scene_forward(*map,error))return {};
      release_map_cpu_geometry(map.get());
    }
    map->geometry=std::make_shared<WorldGeometry>();
    if(!map->geometry->initialize(renderer,*map,page_scheduler_,&context_)) {
      if(deferred)*deferred=map->geometry->admission_rejected();error=map->geometry->error();return {};
    }
    error.clear();return map;
  }catch(const std::exception& failure){error=failure.what();return {};}
}
}
