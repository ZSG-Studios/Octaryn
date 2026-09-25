#pragma once
#include "BlockTransportSetup.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>

namespace mesh_probe {
inline void block_transport_item_lighting_cases(Fixture& fixture) {
  using Pixel=std::array<float,4>;
  struct Input {Pixel color,settings,normal,view,light,incident_front,incident_back,direct_front,direct_back;};
  static_assert(sizeof(Input)==144);
  constexpr double Pi=3.14159265358979323846;
  std::vector<Input> inputs;std::vector<std::array<double,3>> expected;
  const auto append=[&](unsigned metal,float rough,bool front,unsigned mode) {
    const float side=front?1.f:-1.f;
    Input input{{.2f,.5f,.8f,float(metal)},{rough,mode==3?0.f:2.f,front?1.f:0.f,0},
        {0,1,0,0},{0,side,0,0},{0,side,0,0},{100,100,100,0},{100,100,100,0},
        {100,100,100,100},{100,100,100,100}};
    const Pixel incident=mode>=2?Pixel{}:Pixel{.7f,.3f,.5f,0};
    const Pixel direct=mode==3?Pixel{}:Pixel{.4f,.1f,.2f,mode==1?.0f:.25f};
    if(front){input.incident_front=incident;input.direct_front=direct;}
    else {input.incident_back=incident;input.direct_back=direct;}
    if(mode==2)input.settings[1]=0;
    std::array<double,3> wanted{};
    for(unsigned c=0;c<3;++c) {
      const double albedo=input.color[c],f0=metal?albedo:.04;
      // At N=V=L, Smith G=1 and GGX D=1/(pi*roughness^4).
      const double sunlight=((1-f0)*albedo*(1-metal)/Pi+f0/(4*Pi*std::pow(double(rough),4)))*
          input.settings[1]*direct[3];
      wanted[c]=.96*(1-metal)*albedo*(double(incident[c])+direct[c])+sunlight;
    }
    inputs.push_back(input);expected.push_back(wanted);
  };
  for(unsigned metal=0;metal<2;++metal)for(float rough:{.5f,1.f})for(bool front:{false,true})append(metal,rough,front,0);
  append(0,1,true,1);append(1,1,false,1);append(0,1,false,2);append(0,1,true,3);
  auto& r=fixture.renderer;Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportItemLighting.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BT item lighting retained production helper");
  auto source=buffer(r,inputs.data(),inputs.size()*sizeof(Input),sizeof(Input),rhi::BufferUsage::ShaderResource);
  std::vector<Pixel> actual(inputs.size());
  auto output=buffer(r,actual.data(),actual.size()*sizeof(Pixel),sizeof(Pixel),rhi::BufferUsage::UnorderedAccess);
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT item lighting frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT item lighting encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT item lighting pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT item lighting binding");
  const rhi::ShaderCursor cursor(root);const unsigned count=unsigned(inputs.size());
  checked(cursor["itemLightingInputs"].setBinding(source),"BT item lighting input");
  checked(cursor["itemLightingOutputs"].setBinding(output),"BT item lighting output");
  checked(cursor["itemLightingCount"].setData(&count,sizeof(count)),"BT item lighting count");
  pass->dispatchCompute(1,1,1);pass->end();auto command=commands->finish();
  require(bool(command) && r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
      "BT item lighting bounded completion");
  checked(r.device->readBuffer(output,0,actual.size()*sizeof(Pixel),actual.data()),"BT actual item lighting readback");
  for(unsigned i=0;i<count;++i)for(unsigned c=0;c<3;++c)
    require(std::isfinite(actual[i][c]) && std::abs(actual[i][c]-expected[i][c])<2e-6,
        "BT item shade helper differs from analytic Lambertian/GGX or counted wrong side/source");
  require(count==12 && r.debug.errors.load()==0,"BT item lighting scope/validation");
  block_transport_complete(r,start);
  std::puts("block_transport_item_lighting=passed hardware=1 production_shade=1 analytic_normal_incidence=1 lambert_pi=1 metallic_diffuse_zero=1 specular_preserved=1 local_once=1 side_isolation=1 source_removal=1 cases=12 scalar_checks=36 validation_errors=0");
}
}
