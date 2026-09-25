#include "Probe.h"
#include "SlangShaderPath.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <bit>
#include <cmath>

namespace mesh_probe {
namespace {
using Vertex=std::array<float,8>;
std::vector<Vertex> vertices(Fixture& f,rhi::IComputePipeline* pipeline,const Mesh& mesh,
                           const WorldCamera& camera) {
  auto& r=f.renderer;world_renderer_prepare_draw(r,camera);
  r.draw_uniforms[28]=float(mesh.gpu.pass_counts[0]+mesh.gpu.pass_counts[1]+mesh.gpu.pass_counts[2]);
  std::vector<Vertex> result(mesh.faces.size()*6);
  rhi::BufferDesc desc{};desc.size=result.size()*sizeof(Vertex);desc.elementSize=16;
  desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> output;
  checked(r.device->createBuffer(desc,nullptr,output.writeRef()),"relative output buffer");
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"relative command encoder");
  auto* compute=commands->beginComputePass();require(compute!=nullptr,"relative compute pass");
  auto* root=compute->bindPipeline(pipeline);require(root!=nullptr,"relative pipeline binding");
  require(bind_world_atlas(r.atlas,root),"relative production atlas bindings");
  const rhi::ShaderCursor cursor(root);
  const auto binding=[&](const char* name,rhi::IBuffer* value) {
    const auto field=cursor[name];require(field.isValid(),name);
    checked(field.setBinding(rhi::Binding(value)),name);
  };
  binding("faces",mesh.gpu.faces);binding("blockMaterials",world_atlas_materials(r.atlas));
  binding("fluidFaces",mesh.gpu.fluids);binding("precisionVertices",output);
  constexpr const char* names[]={"cameraPosition","cameraRight","cameraUp","cameraForward","projection",
      "lightDirectionSky","worldTime","drawSettings","materialSettings"};
  for(unsigned i=0;i<9;++i) {
    const auto field=cursor[names[i]];
    if(field.isValid())checked(field.setData(r.draw_uniforms.data()+i*4,16),"relative vertex uniforms");
  }
  compute->dispatchCompute(static_cast<unsigned>(result.size()),1,1);compute->end();
  auto submission=commands->finish();require(submission!=nullptr,"relative command finish");
  checked(r.queue->submit(submission),"relative submit");checked(r.queue->waitOnHost(),"relative completion");
  checked(r.device->readBuffer(output,0,desc.size,result.data()),"relative vertex readback");
  return result;
}
}
void relative_precision_cases(Fixture& f) {
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=resolve_slang_shader_path("octaryn-client/Shaders/Voxel/RelativePrecision.slang");
  require(create_rhi_compute_pipeline(f.renderer.device,path.c_str(),"relative_precision",pipeline),"production vertex precision pipeline");
  unsigned torch=0,water=0,lava=0;
  for(unsigned i=1;i<f.catalog.size();++i) {
    const auto& b=f.catalog[i];
    if(b.sprite && b.requiresSolidBase)torch=i;
    if(b.fluidKind=="water" && b.fluidLevel==7)water=i;
    if(b.fluidKind=="lava" && b.fluidLevel==7)lava=i;
  }
  require(torch && water && lava,"precision fixture needs actual torch and level-seven fluids");
  unsigned compared=0,previous_lost=0;
  for(const int min_y:{-256,224}) {
    f.renderer.sources.clear();auto c=column(0,0,min_y,32);
    put(c,3,1,5,static_cast<std::uint16_t>(torch));
    put(c,13,4,11,static_cast<std::uint16_t>(water));
    put(c,21,24,23,static_cast<std::uint16_t>(lava));
    auto mesh=f.mesh(c);f.verify("relative_precision_source",c,mesh);
    require(mesh.gpu.pass_counts[1] && mesh.gpu.pass_counts[3] && mesh.gpu.pass_counts[4],"precision fixture pass coverage");
    const WorldCamera camera{16,float(min_y+16),48,.31f,-.17f,1.05f};
    const auto expected=vertices(f,pipeline,mesh,camera);
    const auto local_faces=mesh.faces;
    for(const int shift:{-16777216,16777216}) {
      mesh.faces=local_faces;
      for(auto& face:mesh.faces)for(const unsigned axis:{0u,2u})
        face[axis]=std::bit_cast<unsigned>(std::bit_cast<int>(face[axis])+shift);
      mesh.gpu.faces=buffer(f.renderer,mesh.faces.data(),mesh.faces.size()*sizeof(Face),16,rhi::BufferUsage::ShaderResource);
      auto moved=camera;moved.x+=float(shift);moved.z+=float(shift);
      const auto actual=vertices(f,pipeline,mesh,moved);
      for(std::size_t i=0;i<actual.size();++i)for(unsigned component=0;component<8;++component) {
        if(!std::isfinite(actual[i][component]) || actual[i][component]!=expected[i][component])
          std::fprintf(stderr,"world_mesh_precision_difference shift=%d min_y=%d vertex=%zu component=%u actual=%.9g expected=%.9g id=%u\n",
              shift,min_y,i,component,actual[i][component],expected[i][component],mesh.faces[i/6][3]&65535);
        require(std::isfinite(actual[i][component]) && actual[i][component]==expected[i][component],
            "large signed translation changed production torch/fluid relative or clip vertex");
        ++compared;
      }
      // Negative control derived from actual origin vertices: world-first float
      // arithmetic must lose information here, or this fixture is not sensitive.
      for(const auto& vertex:expected)for(const unsigned axis:{0u,2u}) {
        const float eye=axis==0?camera.x:camera.z;
        volatile float world_first=float(shift)+(vertex[axis]+eye);
        if(world_first-(float(shift)+eye)!=vertex[axis])++previous_lost;
      }
    }
  }
  require(previous_lost>0,"precision fixture failed to expose world-first rounding");
  require(f.renderer.debug.errors.load()==0,"relative precision validation errors");
  std::printf("world_mesh_relative_precision=passed scalar_checks=%u previous_world_first_losses=%u signed_offsets=16777216 y_min=-256 y_max=255\n",compared,previous_lost);
}
}
