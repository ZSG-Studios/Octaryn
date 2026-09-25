#pragma once
#include "BlockTransportSetup.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/ReceiverGeometry.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>

namespace mesh_probe {
inline void block_transport_geometry_cases(Fixture& fixture) {
  using Pixel=std::array<float,4>;
  struct Input {std::array<Pixel,4> columns;Pixel position,normal,feet,settings;};
  struct Output {Pixel position,normal,geometric_normal;};
  static_assert(sizeof(Input)==128 && sizeof(Output)==48);
  std::vector<Input> inputs;std::vector<ReceiverGeometry> expected;
  const auto pixel=[](ReceiverVector value){return Pixel{value[0],value[1],value[2],0};};
  for(unsigned pose=0;pose<8;++pose)for(unsigned axis=0;axis<3;++axis) {
    const float t=float(pose)*.37f,mirror=(pose&1)?-1.f:1.f;
    const std::array<ReceiverVector,4> columns{{{mirror*(1+t),.12f*t,.03f*t},{.2f*t,1+.1f*t,.15f*t},
        {.05f*t,-.1f*t,1.3f},{.5f*t,-.2f*t,.3f*t}}};
    const ReceiverVector position{.31f,-.27f,.17f},feet{pose<4?-127.25f:8191.75f,19.5f,-31.25f};
    ReceiverVector normal{};normal[axis]=1;
    ReceiverGeometry geometry;const float cosine=std::cos(t),sine=std::sin(t);
    require(player_receiver_geometry(columns,position,normal,feet,cosine,sine,geometry),
        "BT animated player receiver transform rejected a nonsingular pose");
    for(unsigned tangent=0;tangent<3;++tangent)if(tangent!=axis)
      require(std::abs(receiver_dot(geometry.normal,receiver_rotate(columns[tangent],cosine,sine)))<2e-5f,
          "BT player receiver normal is not orthogonal to skinned tangent");
    Input input{};for(unsigned column=0;column<4;++column)input.columns[column]=pixel(columns[column]);
    input.position=pixel(position);input.normal=pixel(normal);input.feet=pixel(feet);input.settings={cosine,sine,-1,0};
    inputs.push_back(input);expected.push_back(geometry);
  }
  const unsigned player_cases=unsigned(inputs.size());
  for(unsigned pose=0;pose<4;++pose)for(unsigned kind=0;kind<7;++kind) {
    const bool sprite=kind==6;const unsigned face=sprite?0:kind;
    const float time=pose*.57f,phase=pose*103.f;
    const ReceiverVector position{pose<2?-15.75f:4095.75f,3.25f,-63.5f};
    Input input{};input.feet=pixel(position);input.settings={time,phase,float(face),sprite?1.f:0.f};
    inputs.push_back(input);expected.push_back(item_receiver_geometry(position,phase,time,face,sprite));
  }
  auto& r=fixture.renderer;Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportGeometry.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BT receiver geometry retained pipeline");
  const auto source=buffer(r,inputs.data(),inputs.size()*sizeof(Input),sizeof(Input),rhi::BufferUsage::ShaderResource);
  std::vector<Output> actual(inputs.size());
  const auto target=buffer(r,actual.data(),actual.size()*sizeof(Output),sizeof(Output),rhi::BufferUsage::UnorderedAccess);
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT receiver geometry frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT receiver geometry encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT receiver geometry pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT receiver geometry binding");
  const rhi::ShaderCursor cursor(root);const unsigned count=unsigned(inputs.size());
  checked(cursor["geometryInputs"].setBinding(source),"BT receiver geometry inputs");
  checked(cursor["geometryOutputs"].setBinding(target),"BT receiver geometry outputs");
  checked(cursor["geometryCount"].setData(&count,sizeof(count)),"BT receiver geometry count");
  pass->dispatchCompute((count+63)/64,1,1);pass->end();auto command=commands->finish();
  require(bool(command) && r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
      "BT receiver geometry bounded completion");
  checked(r.device->readBuffer(target,0,actual.size()*sizeof(Output),actual.data()),"BT receiver geometry readback");
  unsigned checks=0;
  for(unsigned i=0;i<count;++i) {
    for(unsigned axis=0;axis<3;++axis) {
      require(std::abs(actual[i].position[axis]-expected[i].position[axis])<.001f,
          "BT lighting receiver position differs from actual vertex geometry");
      require(std::abs(actual[i].normal[axis]-expected[i].normal[axis])<2e-5f,
          "BT lighting receiver normal differs from actual vertex geometry");checks+=2;
    }
    if(i>=player_cases) {
      float alignment=0;for(unsigned axis=0;axis<3;++axis)
        alignment+=actual[i].geometric_normal[axis]*expected[i].normal[axis];
      require(alignment>1-2e-5f,"BT cube or sprite oriented normal disagrees with rendered plane winding");++checks;
    }
  }
  require(count==52 && player_cases==24 && checks==340,"BT receiver geometry fixture scope changed");
  require(r.debug.errors.load()==0,"BT receiver geometry graphics validation errors");
  block_transport_complete(r,start);
  std::printf("block_transport_geometry=passed hardware=1 production_cpu=1 production_vertices=1 animated_skin=1 mirrored_skin=1 tangent_normals=1 cube_faces=1 sprite_plane=1 signed_positions=1 player_cases=24 item_cases=28 scalar_checks=340 validation_errors=0\n");
}
}
