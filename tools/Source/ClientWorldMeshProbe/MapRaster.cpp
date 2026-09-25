#include "Probe.h"
#include "../../../octaryn-client/Source/MapWorld/MapRendererInternal.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mesh_probe {
namespace {
float half(std::uint16_t bits) {
  const int exponent=(bits>>10)&31,sign=(bits&32768)?-1:1;
  const unsigned mantissa=bits&1023;
  return float(sign)*std::ldexp(float(exponent?mantissa+1024:mantissa),exponent?exponent-25:-24);
}
bool separated_colors(const std::array<float,3>& left,const std::array<float,3>& right) {
  return left[0]>.45f && left[1]<.01f && left[2]<.01f &&
      right[1]>.45f && right[0]<.01f && right[2]<.01f;
}
void raster_pipeline(MapRenderer& map,bool forward,bool index_read=false) {
  const char* entries[]={forward?"forward_vertex_main":"vertex_main",forward?"forward_main":"fragment_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  const auto source=index_read?(std::filesystem::path(__FILE__).parent_path()/"MapIndexStateProbe.slang").generic_string():
      std::string("octaryn-client/Shaders/Map/WorldMap.slang");
  require(create_rhi_program(map.device,source.c_str(),entries,2,program),
      "production map raster program");
  std::array<rhi::ColorTargetDesc,world_gbuffer_count> targets{};
  for(unsigned i=0;i<targets.size();++i)targets[i].format=world_gbuffer_formats[i];
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=targets.data();
  desc.targetCount=forward?1u:unsigned(targets.size());desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  desc.rasterizer.cullMode=rhi::CullMode::None;
  desc.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;
  desc.depthStencil.format=rhi::Format::D32Float;
  desc.depthStencil.depthTestEnable=true;desc.depthStencil.depthWriteEnable=!forward;
  desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  auto& pipeline=forward?map.forward_pipeline:map.gbuffer_pipeline;
  checked(map.device->createRenderPipeline(desc,pipeline.writeRef()),"map raster pipeline");
}
bool render_case(Fixture& f,MapRenderer& map,bool forward,bool check_normals=false) {
  auto& r=f.renderer;
  const WorldCamera camera{0,0,0,0,0,1.57079632679f};
  world_renderer_prepare_draw(r,camera);
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"map raster commands");
  std::array<rhi::RenderPassColorAttachment,world_gbuffer_count> attachments{};
  for(unsigned i=0;i<attachments.size();++i) {
    attachments[i].view=f.views[i];attachments[i].loadOp=rhi::LoadOp::Clear;
    attachments[i].storeOp=rhi::StoreOp::Store;
  }
  require(r.targets[0].depth && r.targets[0].depth_view,"map fixture-owned depth attachment is missing");
  rhi::RenderPassDepthStencilAttachment depth{};depth.view=r.targets[0].depth_view;
  depth.depthClearValue=1;depth.depthLoadOp=rhi::LoadOp::Clear;depth.depthStoreOp=rhi::StoreOp::Store;
  rhi::RenderPassDesc pass{};pass.colorAttachments=attachments.data();
  pass.colorAttachmentCount=forward?1u:unsigned(attachments.size());pass.depthStencilAttachment=&depth;
  auto* encoder=commands->beginRenderPass(pass);require(encoder!=nullptr,"map raster pass");
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(Fixture::Size),float(Fixture::Size));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(Fixture::Size,Fixture::Size);encoder->setRenderState(state);
  require(render_map(&map,encoder,camera,r,forward),"production render_map draw");encoder->end();
  auto submission=commands->finish();checked(r.queue->submit(submission),"map raster submit");
  checked(r.queue->waitOnHost(),"map raster wait");
  Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
  checked(r.device->readTexture(f.targets[0],0,0,pixels.writeRef(),&layout),"map raster readback");
  require(pixels && layout.colPitch==8,"map raster RGBA16 readback");
  const auto color=[&](unsigned x) {
    std::array<float,3> rgb{};
    const auto* source=static_cast<const std::uint8_t*>(pixels->getBufferPointer())+69*layout.rowPitch+x*8;
    for(unsigned channel=0;channel<3;++channel) {
      std::uint16_t bits{};std::memcpy(&bits,source+channel*2,2);rgb[channel]=half(bits);
    }
    return rgb;
  };
  const auto left=color(47),right=color(81);
  std::printf("map_raster_pixels forward=%u first_index=%u left=%.4f,%.4f,%.4f right=%.4f,%.4f,%.4f\n",
      unsigned(forward),map.model.primitives[1].first_index,left[0],left[1],left[2],right[0],right[1],right[2]);
  if(check_normals) {
    Slang::ComPtr<ISlangBlob> normals;rhi::SubresourceLayout normal_layout{};
    checked(r.device->readTexture(f.targets[2],0,0,normals.writeRef(),&normal_layout),"map normal readback");
    require(normals&&normal_layout.colPitch==4,"map normal RGBA8 readback");
    for(unsigned x:{47u,81u}) {
      const auto* p=static_cast<const std::uint8_t*>(normals->getBufferPointer())+69*normal_layout.rowPitch+x*4;
      const float ox=float(p[2])/255*2-1,oy=float(p[3])/255*2-1;
      const float z=1-std::abs(ox)-std::abs(oy);
      std::printf("map_raster_normal x=%u double_sided=%u packed=%u,%u,%u,%u oct_z=%.4f\n",
          x,unsigned(map.model.primitives[0].material.double_sided),p[0],p[1],p[2],p[3],z);
      require((p[0]&7)==6&&z>.98f,"CCW camera-facing map normal was flipped or fragment discarded");
    }
  }
  return separated_colors(left,right);
}
}
void map_raster_cases(Fixture& f) {
  auto& r=f.renderer;MapRenderer map;map.device=r.device;
  const bool previous_ray=r.ray_enabled;r.ray_enabled=false;
  map.model.vertices.resize(6);map.model.indices={0,2,4,1,3,5};map.model.primitives.resize(2);
  constexpr float positions[6][2]={{-1.5f,-.7f},{.1f,-.7f},{-.1f,-.7f},
      {1.5f,-.7f},{-.8f,.7f},{.8f,.7f}};
  for(unsigned i=0;i<6;++i) {
    auto& vertex=map.model.vertices[i];vertex.position[0]=positions[i][0];
    vertex.position[1]=positions[i][1];vertex.position[2]=-3;vertex.normal[2]=1;
  }
  for(unsigned i=0;i<2;++i) {
    auto& primitive=map.model.primitives[i];primitive.first_index=i*3;primitive.index_count=3;
    primitive.bounds_min[0]=i==0?-1.5f:.1f;primitive.bounds_max[0]=i==0?-.1f:1.5f;
    primitive.bounds_min[1]=-.7f;primitive.bounds_max[1]=.7f;
    primitive.bounds_min[2]=-3.01f;primitive.bounds_max[2]=-2.99f;
    primitive.material.double_sided=true;primitive.material.metallic=0;
    std::fill_n(primitive.material.base_color,3,0.f);primitive.material.base_color[i]=1;
    primitive.material.emissive[i]=.5f;
  }
  map.vertices=buffer(r,map.model.vertices.data(),map.model.vertices.size()*sizeof(MapVertex),
      sizeof(MapVertex),rhi::BufferUsage::ShaderResource);
  map.indices=buffer(r,map.model.indices.data(),map.model.indices.size()*4,4,rhi::BufferUsage::ShaderResource);
  rhi::BufferDesc index_desc{};index_desc.size=map.model.indices.size()*4;index_desc.elementSize=4;
  index_desc.usage=rhi::BufferUsage::IndexBuffer;index_desc.defaultState=rhi::ResourceState::IndexBuffer;
  checked(r.device->createBuffer(index_desc,map.model.indices.data(),map.raster_indices.writeRef()),"map raster index buffer");
  raster_pipeline(map,false);raster_pipeline(map,true);
  for(bool forward:{false,true}) {
    for(auto& primitive:map.model.primitives)
      primitive.material.alpha_mode=forward?MapAlphaMode::Blend:MapAlphaMode::Opaque;
    require(upload_map_materials(map),"map raster material upload");
    require(render_case(f,map,forward,!forward),"map raster lost nonzero primitive offset or material identity");
    for(auto& primitive:map.model.primitives)primitive.material.double_sided=false;
    require(upload_map_materials(map),"single-sided map material upload");
    require(render_case(f,map,forward,!forward),"CCW camera-facing single-sided map was discarded");
    for(auto& primitive:map.model.primitives)primitive.material.double_sided=true;
    require(upload_map_materials(map),"double-sided map material restore");
    map.model.primitives[1].first_index=0;
    require(!render_case(f,map,forward),"map raster zero-offset negative control was accepted");
    map.model.primitives[1].first_index=3;
  }
  require(map.indices.get()!=map.raster_indices.get(),"raster index buffer aliases ray SRV indices");
  for(bool forward:{false,true}) {
    raster_pipeline(map,forward,true);
    for(auto& primitive:map.model.primitives)
      primitive.material.alpha_mode=forward?MapAlphaMode::Blend:MapAlphaMode::Opaque;
    require(upload_map_materials(map),"simultaneous index and SRV materials");
    require(render_case(f,map,forward,!forward),"simultaneous index and SRV raster failed");
  }
  r.ray_enabled=previous_ray;
  require(r.debug.errors.load()==0,"map raster graphics validation errors");
  std::puts("map_raster_gpu=passed primitives=2 passes=opaque,forward production_render_map=1 nonzero_first_index=3 normal_positive_z=1 single_sided_visible=1 simultaneous_index_srv=1 negative_control=rejected");
}
}
