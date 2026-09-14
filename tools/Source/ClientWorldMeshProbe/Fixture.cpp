#include "Probe.h"
#include "AssetPath.h"
#include "SlangShaderPath.h"
#include "WorldRasterPipeline.h"
#include <glaze/glaze.hpp>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

namespace mesh_probe {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void checked(SlangResult value,const char* message) {require(SLANG_SUCCEEDED(value),message);}
Slang::ComPtr<rhi::IBuffer> buffer(WorldRenderer& r,const void* data,std::size_t bytes,unsigned stride,rhi::BufferUsage usage) {
  rhi::BufferDesc desc{};desc.size=std::max(std::size_t{16},bytes);desc.elementSize=stride;
  desc.usage=usage|rhi::BufferUsage::CopySource;desc.defaultState=usage==rhi::BufferUsage::IndirectArgument?
      rhi::ResourceState::IndirectArgument:rhi::ResourceState::ShaderResource;
  Slang::ComPtr<rhi::IBuffer> result;
  checked(r.device->createBuffer(desc,bytes?data:nullptr,result.writeRef()),"fixture buffer creation");return result;
}
Fixture::Fixture(bool batch_capacity,bool ray_tracing) {
  auto& r=renderer;
  const rhi::Feature required[]{rhi::Feature::Rasterization,rhi::Feature::AccelerationStructure,
      rhi::Feature::RayQuery,rhi::Feature::Bindless};
  rhi::DeviceDesc desc{};desc.deviceType=rhi::DeviceType::Vulkan;
  desc.requiredFeatures=required;desc.requiredFeatureCount=ray_tracing?4:1;desc.enableValidation=true;desc.debugCallback=&r.debug;
  desc.slang.targetFlags=SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;desc.slang.targetProfile="spirv_1_3";
  if(const char* backend=SDL_getenv("OCTARYN_CLIENT_MESH_PROBE_BACKEND")) {
    if(!std::strcmp(backend,"d3d12")) {
      desc.deviceType=rhi::DeviceType::D3D12;
      desc.slang.targetFlags=0;desc.slang.targetProfile="sm_6_8";
    } else require(!std::strcmp(backend,"vulkan"),"mesh probe backend must be vulkan or d3d12");
  }
  rhi::DebugLayerOptions layers{};layers.coreValidation=true;layers.required=true;
  checked(rhi::getRHI()->setDebugLayerOptions(layers),"required graphics validation setup");
  if(batch_capacity)desc.bindless.bufferCount=WorldBatchDescriptorCapacity;
  checked(rhi::getRHI()->createDevice(desc,r.device.writeRef()),"headless production-mesh device");
  r.capabilities=renderer_capabilities(r.device,desc.bindless);
  require(r.device->getDeviceType()==desc.deviceType,"mesh probe silently changed requested backend");
  const auto& info=r.device->getInfo();
  std::printf("world_mesh_device api=%s adapter=%s validation=required bindless=%u multi_draw=%u first_instance=%u shader_draw_parameters=%u max_draws=%u\n",
      info.apiName,info.adapterName,r.device->hasFeature(rhi::Feature::Bindless)?1u:0u,
      r.device->hasFeature(rhi::Feature::MultiDrawIndirect)?1u:0u,
      r.device->hasFeature(rhi::Feature::DrawIndirectFirstInstance)?1u:0u,
      r.device->hasFeature(rhi::Feature::ShaderDrawParameters)?1u:0u,info.limits.maxDrawIndirectCount);
  checked(r.device->getQueue(rhi::QueueType::Graphics,r.queue.writeRef()),"headless graphics queue");
  const auto path=resolve_slang_shader_path("octaryn-client/Shaders/Voxel/WorldFaces.slang");
  require(create_rhi_compute_pipeline(r.device,path.c_str(),"main",r.mesh_pipeline),"production meshing pipeline");
  require(create_world_raster_pipelines(r.device,r.raster_pipeline,r.sprite_pipeline,r.lava_pipeline,r.transparent_pipeline),
      "production raster pipelines");
  r.atlas=create_world_atlas(r.device);require(r.atlas!=nullptr,"production atlas");
  char catalog_path[4096]{};
  require(bundle_path_build(catalog_path,sizeof(catalog_path),"Data/Blocks/octaryn.basegame.blocks.json"),"catalog path");
  std::ifstream file(std::filesystem::path(reinterpret_cast<const char8_t*>(catalog_path)));
  require(bool(file),"catalog open");const std::string text((std::istreambuf_iterator<char>(file)),{});
  Catalog parsed;
  constexpr glz::opts options{.error_on_unknown_keys=false};
  require(!glz::read<options>(parsed,text) && parsed.schema=="octaryn.basegame.blocks.v1" && !parsed.blocks.empty(),"catalog schema");
  catalog=std::move(parsed.blocks);
  r.width=Size;r.height=Size;r.culling_enabled=false;r.fog_distance=0;
  r.lighting.visual_sky_visibility=1;r.lighting.skylight_floor=.1f;r.lighting.ambient_strength=1;
  rhi::TextureDesc texture{};texture.size={Size,Size,1};
  texture.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource;
  texture.defaultState=rhi::ResourceState::RenderTarget;
  for(unsigned i=0;i<4;++i) {
    texture.format=world_gbuffer_formats[i];
    checked(r.device->createTexture(texture,nullptr,targets[i].writeRef()),"MRT creation");
    checked(targets[i]->getDefaultView(views[i].writeRef()),"MRT view");
  }
  texture.format=rhi::Format::D32Float;texture.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::CopySource;
  texture.defaultState=rhi::ResourceState::DepthWrite;
  checked(r.device->createTexture(texture,nullptr,r.target().depth.writeRef()),"depth creation");
  checked(r.target().depth->getDefaultView(r.target().depth_view.writeRef()),"depth view");
  std::printf("world_mesh_device api=%s adapter=%s surfaces=0\n",r.device->getInfo().apiName,r.device->getInfo().adapterName);
}
Mesh Fixture::mesh(const StreamColumn& source) {
  renderer.sources.insert_or_assign({source.x,source.z},source);
  WorldColumnGpu gpu;require(world_renderer_mesh(renderer,source,gpu),renderer.status.c_str());
  gpu.min_y=source.min_y;gpu.height=source.height;
  return read_mesh(gpu);
}
Mesh Fixture::read_mesh(const WorldColumnGpu& gpu) const {
  Mesh result;result.gpu=gpu;
  result.faces.resize(result.gpu.face_count);
  if(!result.faces.empty()) checked(renderer.device->readBuffer(result.gpu.faces,0,result.faces.size()*sizeof(Face),result.faces.data()),"GPU faces readback");
  std::array<std::uint32_t,40> arguments{};
  checked(renderer.device->readBuffer(result.gpu.arguments,0,sizeof(arguments),arguments.data()),"GPU indirect readback");
  unsigned base=0;
  for(unsigned p=0;p<5;++p) {
    require(arguments[p*4]==6 && arguments[p*4+1]==result.gpu.pass_counts[p] && arguments[p*4+2]==0 && arguments[p*4+3]==base,
        "count/emit indirect ranges disagree");base+=result.gpu.pass_counts[p];
  }
  require(base==result.faces.size(),"indirect ranges do not cover face buffer");
  unsigned patch_total=0;
  for(unsigned p=0;p<5;++p) {
    require(result.gpu.patch_counts[p]<=std::uint64_t(result.gpu.pass_counts[p])*64,
        "GPU patch count exceeds packed ordinal capacity");
    patch_total+=result.gpu.patch_counts[p];
  }
  result.patches.resize(patch_total);
  if(patch_total)checked(renderer.device->readBuffer(result.gpu.patches,0,
      result.patches.size()*sizeof(std::uint32_t),result.patches.data()),"GPU patches readback");
  unsigned face_base=0,patch_base=0;
  for(unsigned p=0;p<5;++p) {
    require(arguments[20+p*4]==6 && arguments[21+p*4]==result.gpu.patch_counts[p] &&
        arguments[22+p*4]==0 && arguments[23+p*4]==patch_base,"patch draw indirect ranges disagree");
    std::vector<std::uint32_t> expected;
    for(unsigned i=face_base;i<face_base+result.gpu.pass_counts[p];++i) {
      require(i<(1u<<26),"face index exceeds patch encoding");
      const auto& face=result.faces[i];
      require((face[3]&65535)<catalog.size() && pass(catalog[face[3]&65535])==p,
          "patch face belongs to wrong material pass");
      const unsigned w=((face[3]>>20)&31)+1,h=((face[3]>>25)&31)+1;
      const unsigned count=std::min(w*h,w+h);
      for(unsigned ordinal=0;ordinal<count;++ordinal)expected.push_back((i<<6)|ordinal);
    }
    require(expected.size()==result.gpu.patch_counts[p],"patch count differs from independent rectangle coverage");
    std::vector<std::uint32_t> actual(result.patches.begin()+patch_base,
        result.patches.begin()+patch_base+result.gpu.patch_counts[p]);
    std::sort(actual.begin(),actual.end());
    require(actual==expected,"patch mapping has missing, duplicate, out-of-range or wrong-pass records");
    face_base+=result.gpu.pass_counts[p];patch_base+=result.gpu.patch_counts[p];
  }
  result.fluids.resize(result.gpu.pass_counts[3]+result.gpu.pass_counts[4]);
  if(!result.fluids.empty()) checked(renderer.device->readBuffer(result.gpu.fluids,0,result.fluids.size()*32,result.fluids.data()),"GPU fluid readback");
  return result;
}
Mesh Fixture::unit_mesh(const std::vector<Face>& faces,int min_y,int height) {
  // Diagnostic reference geometry only. The game always uses production GPU meshing.
  Mesh result;result.faces=faces;result.gpu.face_count=static_cast<unsigned>(faces.size());
  result.gpu.min_y=min_y;result.gpu.height=height;
  std::stable_sort(result.faces.begin(),result.faces.end(),[&](const Face& a,const Face& b){return pass(catalog[a[3]&65535])<pass(catalog[b[3]&65535]);});
  for(const auto& face:faces) ++result.gpu.pass_counts[pass(catalog[face[3]&65535])];
  std::array<std::uint32_t,40> arguments{};unsigned base=0;
  for(unsigned p=0;p<5;++p) {
    arguments[p*4]=6;arguments[p*4+1]=result.gpu.pass_counts[p];arguments[p*4+3]=base;
    arguments[20+p*4]=6;arguments[23+p*4]=static_cast<unsigned>(result.patches.size());
    for(unsigned i=base;i<base+result.gpu.pass_counts[p];++i) {
      require(i<(1u<<26),"reference face index exceeds patch encoding");
      const auto packed=result.faces[i][3];
      const unsigned w=((packed>>20)&31)+1,h=((packed>>25)&31)+1;
      for(unsigned ordinal=0;ordinal<std::min(w*h,w+h);++ordinal)result.patches.push_back((i<<6)|ordinal);
    }
    result.gpu.patch_counts[p]=static_cast<unsigned>(result.patches.size())-arguments[23+p*4];
    arguments[21+p*4]=result.gpu.patch_counts[p];base+=result.gpu.pass_counts[p];
  }
  const std::array<float,8> no_fluid{};
  result.gpu.faces=buffer(renderer,result.faces.data(),result.faces.size()*16,16,rhi::BufferUsage::ShaderResource);
  result.gpu.patches=buffer(renderer,result.patches.data(),result.patches.size()*sizeof(std::uint32_t),4,rhi::BufferUsage::ShaderResource);
  result.gpu.arguments=buffer(renderer,arguments.data(),sizeof(arguments),0,rhi::BufferUsage::IndirectArgument);
  result.gpu.fluids=buffer(renderer,no_fluid.data(),sizeof(no_fluid),32,rhi::BufferUsage::ShaderResource);
  return result;
}
}
