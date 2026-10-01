#include "SceneAssets.h"
#include "SceneMemoryLedger.h"
#include "SceneGeometryPool.h"
#include "SceneRootPages.h"
#include "SelectionResources.h"
#include "SceneRayScheduler.h"
#include "SceneRasterTables.h"
#include "GeometryCoarse.h"
#include "../MapWorld/MapAssetBuildInternal.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include "../Rendering/RenderBackend/SlangShaderPath.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::rendering {
using namespace virtual_geometry;
bool SceneAssets::initialize_shared(WorldRenderer& renderer,std::string& error) {
  try {
    context_.ledger=renderer.scene_memory;
    if(!context_.ledger)throw std::runtime_error("scene GPU ledger is missing");
    std::vector<std::uint32_t> payloads;std::uint64_t root_pages{};
    for(const auto root:roots_) {
      const auto& part=render_parts_[root];
      if(part.hierarchy) {
        for(const auto page:part.node.coarse.root_page_ids)payloads.push_back(part.node.coarse.page_used_bytes.at(page));
        root_pages+=part.node.coarse.root_page_ids.size();
      }else {
        const auto file=catalog_path_.parent_path()/std::filesystem::u8path(part.geometry.geometry);
        GeometryAsset asset;if(!read_geometry_cache(file,part.geometry.hash,asset,error,false))return false;
        const auto sizes=geometry_page_payload_bytes(asset);std::vector<bool> pinned(asset.pages.size());
        for(const auto root:asset.roots) {
          const auto& group=asset.groups.at(root);
          for(unsigned i=0;i<group.page_count;++i)pinned.at(asset.group_pages.at(group.first_page+i))=true;
        }
        for(unsigned page=0;page<pinned.size();++page)if(pinned[page]) {payloads.push_back(sizes[page]);++root_pages;}
      }
    }
    const auto packed=SceneRootPages::required_slots(payloads);
    if(packed==invalid_id)throw std::runtime_error("scene root packing is invalid");
    SceneGeometryPoolConfig pool;pool.root_slots=std::max(1u,packed)+128;pool.slots=pool.root_slots+128;
    pool.maximum_assets=std::max(1024u,std::uint32_t(roots_.size())+512);pool.feedback_capacity=128;
    pool.scheduler=page_scheduler_;
    context_.pages=std::make_shared<SceneGeometryPool>();
    if(!context_.pages->initialize(renderer.device,context_.ledger,pool)) {error=context_.pages->error();return false;}
    SelectionResourcesConfig selection;selection.groups=4096;selection.clusters=16384;selection.pages=2048;
    selection.page_references=32768;selection.parents=65536;selection.feedback_capacity=128;
    selection.tickets_per_frame=pool.maximum_assets;selection.instances=1;
    std::vector<unsigned> mesh_nodes(catalog_.mesh_count);
    for(const auto& node:catalog_.instances)selection.instances=std::max(selection.instances,++mesh_nodes[node.mesh]);
    const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/VirtualGeometry/Selection.slang");
    context_.selection=std::make_shared<SelectionResources>();
    if(!context_.selection->initialize(renderer.device,context_.ledger,shader.c_str(),selection)) {
      error=context_.selection->error();return false;
    }
    context_.ray=std::make_shared<SceneRayScheduler>(context_.ledger);
    const auto expand=resolve_slang_shader_path("octaryn-client/Shaders/VirtualGeometry/RayExpand.slang");
    if(!context_.ray->initialize(renderer.device,expand.c_str())) {error=context_.ray->error();return false;}
    SceneRasterCapacity raster;
    for(const auto root:roots_) {
      const auto count=this->selection(root).instances.size();if(!count)continue;
      const auto& part=render_parts_[root].geometry;raster.clusters+=part.clusters;raster.pages+=part.pages;
      raster.instances+=count;raster.draws+=std::uint64_t(part.clusters)*count;
    }
    context_.raster=std::make_shared<SceneRasterTables>();
    const auto directory=std::filesystem::path(shader).parent_path().generic_string();
    if(!context_.raster->initialize(renderer.device,context_.ledger,directory.c_str()) || !context_.raster->reserve(raster)) {
      error=context_.raster->error();return false;
    }
    // A 16-record stride satisfies both structured-buffer and 256-byte subrange alignment.
    std::vector<MapRayMaterial> records;
    if(!prepare_map_materials(*materials_,records,SIZE_MAX))throw std::runtime_error("scene materials could not be resolved");
    std::vector<MapRayMaterial> padded(records.size()*16);
    for(std::size_t i=0;i<records.size();++i)padded[i*16]=records[i];
    const auto bytes=padded.size()*sizeof(MapRayMaterial);
    materials_->material_allocation=context_.ledger->reserve(bytes,SceneMemoryDomain::Materials);
    if(!materials_->material_allocation)throw std::runtime_error("scene material table exceeds aggregate GPU budget");
    rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=sizeof(MapRayMaterial);
    desc.usage=rhi::BufferUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
    if(SLANG_FAILED(renderer.device->createBuffer(desc,padded.data(),materials_->ray_primitives.writeRef())))
      throw std::runtime_error("scene shared material buffer upload failed");
    const auto stats=context_.ledger->stats();
    std::printf("scene_shared_resources roots=%zu root_pages=%llu packed_slots=%u root_slots=%u fine_slots=128 material_bytes=%llu "
        "selection_bytes=%llu ledger_used=%llu ledger_limit=%llu\n",roots_.size(),static_cast<unsigned long long>(root_pages),packed,
        pool.root_slots,static_cast<unsigned long long>(bytes),static_cast<unsigned long long>(context_.selection->gpu_bytes()),
        static_cast<unsigned long long>(stats.used),static_cast<unsigned long long>(stats.limit));
    return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
