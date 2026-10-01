#include "ItemRenderer.h"
#include "WorldRendererInternal.h"
#include "../../MapWorld/MapRendererInternal.h"
#include "AssetPath.h"
#include "WorldRayCapacity.h"
#include <glaze/glaze.hpp>
#include <fstream>

namespace octaryn::client::rendering {
struct ItemVisualEntry {std::uint32_t item_id{};std::string mesh;};
struct ItemVisualCatalog {unsigned version{};std::vector<ItemVisualEntry> items;};
bool initialize_item_renderer(WorldRenderer& r) {
  if(r.items.initialized)return true;
  ItemRenderer next;auto& items=next;
  const auto capacity=world_ray::item_prewarm_capacity(SDL_getenv("OCTARYN_CLIENT_ITEM_PREWARM_CAPACITY"));
  if(!capacity) {std::fprintf(stderr,"item_prewarm_capacity_invalid expected=1000_or_10000\n");return false;}
  char path[4096];
  if(!bundle_path_build(path,sizeof(path),"Data/Items/render.json"))return false;
  if(!std::filesystem::is_regular_file(path)) {items.initialized=true;r.items=std::move(next);return true;}
  if(std::filesystem::file_size(path)>65536)return false;
  std::ifstream file(path);std::string data((std::istreambuf_iterator<char>(file)),{});
  ItemVisualCatalog catalog;
  if(glz::read_json(catalog,data) || catalog.version!=1 || catalog.items.empty() || catalog.items.size()>256)return false;
  for(const auto& entry:catalog.items) {
    if(!entry.item_id || items.asset_lookup.contains(entry.item_id))return false;
    const auto asset="Assets/"+entry.mesh;
    if(!bundle_path_build(path,sizeof(path),asset.c_str()))return false;
    std::shared_ptr<MapRenderer> mesh(create_map_renderer(r.device,rhi::Format::RGBA16Float,
        rhi::Format::D32Float,path,"octaryn-client/Shaders/Map/WorldMap.slang",false),destroy_map_renderer);
    if(!mesh || mesh->index_count>49152)return false;
    for(const auto& primitive:mesh->model.primitives)
      if(primitive.material.alpha_mode==MapAlphaMode::Blend)return false;
    if(!initialize_map_ray_scene(*mesh,r.queue))return false;
    const auto memory=map_memory_stats(*mesh);
    items.gpu_bytes+=memory.geometry+memory.acceleration+memory.scratch+memory.textures;
    release_map_cpu_geometry(mesh.get());
    items.asset_lookup.emplace(entry.item_id,unsigned(items.assets.size()));
    items.assets.push_back({entry.item_id,std::move(mesh)});
  }
  for(auto& buffer:items.buffers) {
    rhi::BufferDesc desc{};desc.size=ItemRenderCapacity*sizeof(ItemRenderInstance);desc.elementSize=sizeof(ItemRenderInstance);
    desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
    desc.defaultState=rhi::ResourceState::ShaderResource;desc.label="item_instances";
    if(SLANG_FAILED(r.device->createBuffer(desc,nullptr,buffer.writeRef())))return false;
    items.gpu_bytes+=desc.size;
  }
  rhi::ColorTargetDesc targets[world_gbuffer_formats.size()]{};
  for(unsigned i=0;i<world_gbuffer_formats.size();++i)targets[i].format=world_gbuffer_formats[i];
  rhi::RenderPipelineDesc desc{};desc.targets=targets;desc.targetCount=world_gbuffer_attachment_count(r.device);
  desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  desc.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;desc.rasterizer.cullMode=rhi::CullMode::None;
  desc.depthStencil.format=rhi::Format::D32Float;desc.depthStencil.depthTestEnable=true;
  desc.depthStencil.depthWriteEnable=true;desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  Slang::ComPtr<rhi::IShaderProgram> program;
  const char* entries[]={"item_vertex","item_fragment"};
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/Items/WorldItem.slang",entries,2,program))return false;
  desc.program=program;
  if(SLANG_FAILED(r.device->createRenderPipeline(desc,items.gbuffer.writeRef())))return false;
  entries[1]="item_motion";
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/Items/WorldItem.slang",entries,2,program))return false;
  desc.program=program;desc.targetCount=1;desc.depthStencil.depthWriteEnable=false;
  if(SLANG_FAILED(r.device->createRenderPipeline(desc,items.motion.writeRef())))return false;
  items.poses.reserve(ItemRenderCapacity);items.instances.reserve(ItemRenderCapacity);
  items.batches.reserve(catalog.items.size());
  if(!prewarm_item_history(items.history->poses,*capacity,ItemRenderCapacity) ||
      !world_ray_prewarm_items(r,items,*capacity))return false;
  // No fallible work remains after ray capacity publication.
  items.initialized=true;
  r.items=std::move(next);
  std::printf("item_history_prewarm capacity=%u retained_generations=2 bytes=%zu\n",*capacity,r.items.history->memory.bytes());
  std::printf("item_renderer_ready assets=%zu capacity=%u bytes=%llu geometry=module_glb\n",r.items.assets.size(),
      ItemRenderCapacity,static_cast<unsigned long long>(r.items.gpu_bytes));
  return true;
}
bool open_world_renderer_prepare_items(WorldRenderer* r) {return r && initialize_item_renderer(*r);}
}
