#include "Probe.h"
#include "MapMipFixture.h"
#include "../../../octaryn-client/Source/MapWorld/MapRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>
#include <thread>

namespace mesh_probe {
namespace {
void pbr_environment_cases(Fixture& f) {
  auto& r=f.renderer;
  const auto shader=(std::filesystem::path(__FILE__).parent_path()/"PbrEnvironmentProbe.slang").generic_string();
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(create_rhi_compute_pipeline(r.device,shader.c_str(),"main",pipeline),"PBR environment shader");
  rhi::BufferDesc desc{};desc.size=64*16;desc.elementSize=16;
  desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> output;
  checked(r.device->createBuffer(desc,nullptr,output.writeRef()),"PBR environment output");
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"PBR environment commands");
  auto* pass=commands->beginComputePass();auto* root=pass->bindPipeline(pipeline);
  checked(rhi::ShaderCursor(root)["pbrResults"].setBinding(rhi::Binding(output)),"PBR environment binding");
  pass->dispatchCompute(1,1,1);pass->end();auto submission=commands->finish();
  checked(r.queue->submit(submission),"PBR environment submit");
  checked(r.queue->waitOnHost(),"PBR environment wait");
  std::array<std::array<float,4>,64> values{};
  checked(r.device->readBuffer(output,0,sizeof(values),values.data()),"PBR environment readback");
  for(unsigned c=0;c<3;++c)require(std::abs(values[0][c]-.015696f)<.0001f,"rough grazing environment reflectance");
  require(std::abs(values[0][3]-.045638f)<.0001f,"analytic smooth normal environment reflectance");
  for(unsigned i=1;i<values.size();++i) {
    for(float value:values[i])require(std::isfinite(value),"nonfinite PBR visible-normal result");
    require(std::abs(values[i][0]-1)<.0001f,"PBR visible normal not unit length");
    require(values[i][1]>0&&values[i][2]>=0,"PBR sampled invisible normal");
    require(values[i][3]>=0&&values[i][3]<=1.00001f,"PBR unbounded reflection weight");
  }
  std::printf("map_pbr_environment_gpu=passed cases=64 rough_grazing_reflectance=%.6f bounded_vndf=1\n",values[0][0]);
}
void map_uploaded_mips(Fixture& f,const std::filesystem::path& cache={}) {
  auto& r=f.renderer;MapRenderer map;map.device=r.device;
  initialize_map_mip_fixture(map.model);map.texture_cache_directory=cache;
  auto& mask=map.model.primitives[1].material;
  if(!cache.empty())require(std::filesystem::is_directory(cache),"run offline texture cache self-test before cached GPU mip fixture");
  require(upload_map_images(map)&&upload_map_materials(map),"production role mip upload");
  unsigned compressed=0;
  for(const auto& texture:map.textures) {
    const auto format=texture->getDesc().format;
    if(format==rhi::Format::BC7Unorm || format==rhi::Format::BC7UnormSrgb)++compressed;
  }
  if(!cache.empty()) {
    require(compressed>0,"cached GPU fixture silently used only uncompressed textures");
    require(map.textures[map.material_texture_slots[0][0]]->getDesc().format==rhi::Format::BC7UnormSrgb,
        "cached color fixture did not use BC7 sRGB upload");
  } else require(compressed==0,"uncooked fixture unexpectedly compressed at runtime");
  const auto shader=(std::filesystem::path(__FILE__).parent_path()/"MapMaterialsProbe.slang").generic_string();
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(create_rhi_compute_pipeline(r.device,shader.c_str(),"mip_main",pipeline),"map uploaded mip shader");
  rhi::BufferDesc desc{};desc.size=64;desc.elementSize=16;
  desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> output;checked(r.device->createBuffer(desc,nullptr,output.writeRef()),"map mip output");
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"map mip commands");
  auto* pass=commands->beginComputePass();auto* root=pass->bindPipeline(pipeline);
  const rhi::ShaderCursor cursor(root);
  checked(cursor["mapRayPrimitives"].setBinding(rhi::Binding(map.ray_primitives)),"uploaded mip materials");
  checked(cursor["mapProbeResults"].setBinding(rhi::Binding(output)),"uploaded mip output");
  pass->dispatchCompute(1,1,1);pass->end();auto submission=commands->finish();
  checked(r.queue->submit(submission),"map mip submit");checked(r.queue->waitOnHost(),"map mip wait");
  std::array<std::array<float,4>,4> values{};
  checked(r.device->readBuffer(output,0,sizeof(values),values.data()),"map mip readback");
  for(unsigned c=0;c<3;++c)require(std::abs(values[0][c]-.5f)<.006f,"uploaded color mip is not linear-light midpoint");
  const auto& normal=values[1];
  const float magnitude=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
  require(std::abs(magnitude-1)<.012f&&normal[0]>.70f&&normal[2]>.70f,"uploaded normal mip lost normalization");
  require(std::abs(values[2][1]-std::sqrt(.5f))<.006f,"uploaded roughness mip lost RMS");
  unsigned coverage=0;for(float alpha:values[3])if(alpha>=mask.alpha_cutoff)++coverage;
  require(coverage==2,"uploaded MASK mip lost half coverage");
  require(values[3][0]<.01f&&values[3][1]<.9f&&values[3][2]>=.9f,"uploaded nearest MASK samples blended or reordered");
  std::printf("map_uploaded_mips_gpu=passed cached=%u bc7_textures=%u explicit_lod=1 color_linear=%.5f normal_length=%.5f roughness_rms=%.5f mask_coverage=%u/4\n",
      unsigned(!cache.empty()),compressed,values[0][0],magnitude,values[2][1],coverage);
}
}
void map_material_cases(Fixture& f) {
  auto& r=f.renderer;
  MapRenderer map;map.device=r.device;map.ray_supported=true;
  constexpr float texels[6][4]={{.2f,.4f,.6f,1},{.1f,.5f,.75f,1},{.8f,.5f,.9f,1},
      {.25f,0,0,1},{.3f,.4f,.5f,1},{1,1,1,0}};
  for(const auto& texel:texels) {
    rhi::TextureDesc desc{};desc.size={1,1,1};desc.format=rhi::Format::RGBA32Float;
    desc.usage=rhi::TextureUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
    const rhi::SubresourceData data{texel,16,16};
    Slang::ComPtr<rhi::ITexture> texture;Slang::ComPtr<rhi::ITextureView> view;
    checked(r.device->createTexture(desc,&data,texture.writeRef()),"map fixture texture");
    checked(texture->getDefaultView(view.writeRef()),"map fixture view");
    map.textures.push_back(texture);map.texture_views.push_back(view);
  }
  // Indexed geometry must not build triangles from unrelated shared vertices.
  map.model.vertices.resize(4096);
  for(unsigned i=0;i<6;++i) {
    auto& v=map.model.vertices[i];
    v.position[0]=i%3==1?1.f:0.f;v.position[1]=i%3==2?1.f:0.f;v.position[2]=i<3?1.f:0.f;
    v.normal[2]=1;v.tangent[0]=1;v.tangent[3]=1;
    v.uv[0]=v.uv1[0]=v.position[0];v.uv[1]=v.uv1[1]=v.position[1];
  }
  map.model.indices={0,1,2,3,4,5};map.model.primitives.resize(2);
  map.material_texture_slots={{{5,0,0,0,0}},{{0,1,2,3,4}}};
  auto& cutout=map.model.primitives[0];cutout.index_count=3;
  cutout.material.alpha_mode=MapAlphaMode::Mask;cutout.material.alpha_cutoff=.5f;
  cutout.material.textures[0].image=5;
  auto& back=map.model.primitives[1];back.first_index=3;back.index_count=3;
  auto& material=back.material;material.base_color[0]=.5f;material.base_color[1]=.5f;material.base_color[2]=.5f;
  material.metallic=.8f;material.roughness=.6f;material.occlusion_strength=.5f;
  material.emissive[0]=2;material.emissive[1]=3;material.emissive[2]=4;
  for(unsigned i=0;i<5;++i) {material.textures[i].image=int(i);material.textures[i].texcoord=1;}
  const auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::AccelerationStructureBuildInput;
  map.vertices=buffer(r,map.model.vertices.data(),map.model.vertices.size()*sizeof(MapVertex),sizeof(MapVertex),usage);
  map.indices=buffer(r,map.model.indices.data(),map.model.indices.size()*4,4,usage);
  rhi::AccelerationStructureBuildInput size_input{};
  size_input.type=rhi::AccelerationStructureBuildInputType::Triangles;
  auto& triangles=size_input.triangles;
  triangles.vertexBuffers[0]=map.vertices;triangles.vertexBufferCount=1;
  triangles.vertexFormat=rhi::Format::RGB32Float;triangles.vertexStride=sizeof(MapVertex);
  triangles.vertexCount=6;triangles.indexBuffer=map.indices;
  triangles.indexFormat=rhi::IndexFormat::Uint32;triangles.indexCount=3;
  rhi::AccelerationStructureBuildDesc size_build{};
  size_build.inputs=&size_input;size_build.inputCount=1;
  size_build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  rhi::AccelerationStructureSizes baseline{},padded{};
  checked(r.device->getAccelerationStructureSizes(size_build,&baseline),"indexed baseline sizes");
  triangles.vertexCount=static_cast<unsigned>(map.model.vertices.size());
  checked(r.device->getAccelerationStructureSizes(size_build,&padded),"indexed padded sizes");
  require(baseline.accelerationStructureSize==padded.accelerationStructureSize &&
      baseline.scratchSize==padded.scratchSize,"indexed allocation independent of unused vertices");
  std::printf("map_indexed_blas_sizes=passed vertices=4096 indices_per_geometry=3 blas=%llu scratch=%llu\n",
      static_cast<unsigned long long>(padded.accelerationStructureSize),
      static_cast<unsigned long long>(padded.scratchSize));
  require(upload_map_materials(map),"production map material upload");
  const std::array<std::uint32_t,8> empty_record{};
  auto dummy=buffer(r,empty_record.data(),sizeof(empty_record),32,rhi::BufferUsage::ShaderResource);
  rhi::BufferDesc desc{};desc.size=8*16;desc.elementSize=16;
  desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> output;checked(r.device->createBuffer(desc,nullptr,output.writeRef()),"map output");
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto shader=(std::filesystem::path(__FILE__).parent_path()/"MapMaterialsProbe.slang").generic_string();
  require(create_rhi_compute_pipeline(r.device,shader.c_str(),"main",pipeline),"map material production shader");
  require(initialize_map_ray_scene(map,r.queue.get()),"production static map RT initialization");
  require(map_ray_ready(map)&&!map.blas_scratch&&!map.tlas_scratch&&!map.instances,
      "static map RT initialization did not complete or release scratch");
  {
    MapRenderer late;late.device=map.device;late.model=map.model;
    late.vertices=map.vertices;late.indices=map.indices;late.ray_supported=true;
    require(pump_map_ray_scene(late,r.queue.get(),false)&&!late.blas,"RT off must not allocate");
    require(pump_map_ray_scene(late,r.queue.get(),true)&&!map_ray_ready(late)&&late.ray_pending_fence,
        "late RT build must remain pending after submit");
    // Turning the request off still polls and retires the already submitted build.
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(!map_ray_ready(late)&&std::chrono::steady_clock::now()<deadline) {
      require(pump_map_ray_scene(late,r.queue.get(),false),"disabled pending RT poll");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    finish_map_ray_scene(late);
    require(map_ray_ready(late)&&!late.ray_pending_fence&&!late.ray_pending_commands&&
        !late.blas_scratch&&!late.tlas_scratch&&!late.instances,"late RT fence retirement");
    require(pump_map_ray_scene(late,r.queue.get(),true)&&!late.ray_pending_fence,"ready RT reenable must not rebuild");
  }
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"map commands");
  require(prepare_map_ray_scene(map,commands),"production map BLAS/TLAS");
  auto* pass=commands->beginComputePass();auto* root=pass->bindPipeline(pipeline);
  require(bind_map_ray_buffers(map,root)&&bind_world_atlas(r.atlas,root),"map/atlas bindings");
  const rhi::ShaderCursor cursor(root);
  checked(cursor["rayScene"].setBinding(rhi::Binding(map.tlas)),"map ray scene");
  if(cursor["playerShadowScene"].isValid())checked(cursor["playerShadowScene"].setBinding(rhi::Binding(map.tlas)),"player scene");
  if(cursor["rayRecords"].isValid())checked(cursor["rayRecords"].setBinding(rhi::Binding(dummy)),"dummy voxel records");
  const std::array<float,4> settings{1,10,.001f,0};const unsigned disabled=0;
  checked(cursor["raySettings"].setData(settings.data(),sizeof(settings)),"ray settings");
  if(cursor["playerShadowEnabled"].isValid())checked(cursor["playerShadowEnabled"].setData(&disabled,4),"player disabled");
  checked(cursor["mapProbeResults"].setBinding(rhi::Binding(output)),"map output binding");
  pass->dispatchCompute(1,1,1);pass->end();auto submission=commands->finish();
  checked(r.queue->submit(submission),"map submit");checked(r.queue->waitOnHost(),"map wait");
  std::array<std::array<float,4>,8> values{};
  checked(r.device->readBuffer(output,0,sizeof(values),values.data()),"map readback");
  unsigned checks=0;
  const auto near=[&](float a,float b,float tolerance=.0001f) {
    ++checks;
    if(!std::isfinite(a)||std::abs(a-b)>=tolerance)
      std::fprintf(stderr,"map_numeric check=%u actual=%.9g expected=%.9g tolerance=%.9g\n",checks,a,b,tolerance);
    require(std::isfinite(a)&&std::abs(a-b)<tolerance,"map material numeric mismatch");
  };
  near(values[0][0],.1f);near(values[0][1],.2f);near(values[0][2],.3f);near(values[0][3],1);
  near(values[1][0],.6f);near(values[1][1],.3f);near(values[1][2],.625f);
  near(values[1][3],2);
  near(values[2][0],.6f);near(values[2][1],0);near(values[2][2],.8f);
  near(values[3][0],.6f);near(values[3][1],1.2f);near(values[3][2],2);
  for(unsigned axis=0;axis<3;++axis)near(values[4][axis],values[2][axis],.015f);
  near(values[5][0],1);near(values[5][1],2);near(values[5][2],1);near(values[5][3],.3f);
  for(unsigned axis=0;axis<3;++axis) {near(values[6][axis],values[0][axis]);near(values[7][axis],values[3][axis]);}
  near(values[7][3],.6f);
  require(r.debug.errors.load()==0,"map probe graphics validation errors");
  std::printf("map_material_gpu=passed checks=%u texture_slots=5 material_stride=%zu vertex_stride=%zu cutout_skipped=1 ray_hit=1\n",checks,sizeof(MapRayMaterial),sizeof(MapVertex));
  map_raster_cases(f);
  pbr_environment_cases(f);
  map_uploaded_mips(f);
  const auto workspace=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
  map_uploaded_mips(f,workspace/"build/release-windows/tools/map-texture-cache-test/gpu-cache");
}
}
