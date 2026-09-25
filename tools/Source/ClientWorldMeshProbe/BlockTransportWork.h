#pragma once
#include "BlockTransportSetup.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>

namespace mesh_probe {
struct BlockTransportWork {
  using Words=std::array<unsigned,4>;
  WorldRenderer& renderer;
  unsigned capacity,maximum;
  Slang::ComPtr<rhi::IBuffer> rows,blocks,schedule;
  std::array<Slang::ComPtr<rhi::IComputePipeline>,3> pipelines;
  BlockTransportWork(WorldRenderer& r,unsigned count,unsigned budget):renderer(r),capacity(count),maximum(budget) {
    require(count>0 && budget>0 && budget<=count,"BT work fixture bounds");
    const auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination;
    std::vector<Words> row_data(budget),block_data((count+255)/256);
    std::array<unsigned,16> header{};
    rows=buffer(r,row_data.data(),row_data.size()*sizeof(Words),sizeof(Words),usage);
    blocks=buffer(r,block_data.data(),block_data.size()*sizeof(Words),sizeof(Words),usage);
    schedule=buffer(r,header.data(),sizeof(header),sizeof(unsigned),usage);
    const char* entries[]={"count_main","prefix_main","select_main"};
    for(unsigned i=0;i<3;++i)require(block_transport_pipeline(r.device,
        "octaryn-client/Shaders/BlockTransportGI/Select.slang",entries[i],pipelines[i]),
        "BT retained production occupied-row selection pipeline");
  }
  void encode(rhi::ICommandEncoder* commands,rhi::IBuffer* surfaces,rhi::IBuffer* direct,
      rhi::IBuffer* environment,rhi::IBuffer* indirect,const Words& frame,unsigned radiance) {
    const Words work{0,maximum,radiance,0};
    commands->setBufferState(rows,rhi::ResourceState::UnorderedAccess);
    commands->setBufferState(blocks,rhi::ResourceState::UnorderedAccess);
    commands->setBufferState(schedule,rhi::ResourceState::UnorderedAccess);
    for(unsigned stage=0;stage<3;++stage) {
      auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT occupied-row selection pass");
      auto* root=pass->bindPipeline(pipelines[stage]);require(root!=nullptr,"BT occupied-row selection binding");
      const rhi::ShaderCursor cursor(root);
      const auto bind=[&](const char* name,rhi::IBuffer* resource) {
        auto field=cursor[name];if(field.isValid())checked(field.setBinding(resource),name);
      };
      const auto data=[&](const char* name,const void* bytes,std::size_t size) {
        auto field=cursor[name];if(field.isValid())checked(field.setData(bytes,size),name);
      };
      bind("btSurfaces",surfaces);bind("btDirect",direct);bind("btEnvironment",environment);bind("btIndirect",indirect);
      bind("btWorkRows",rows);bind("btWorkBlocks",blocks);bind("btSchedule",schedule);
      data("btFrameInfo",frame.data(),sizeof(frame));data("btWork",work.data(),sizeof(work));
      for(const char* forbidden:{"btSurfaceKeys","positions","voxels","colors","btDimensions","eye","projection"})
        require(!cursor[forbidden].isValid(),"BT occupied-row selection depends on screen input");
      pass->dispatchCompute(stage==1?1:(capacity+255)/256,1,1);pass->end();commands->globalBarrier();
    }
    commands->setBufferState(rows,rhi::ResourceState::ShaderResource);
    commands->setBufferState(schedule,rhi::ResourceState::ShaderResource);
  }
  void bind(rhi::IShaderObject* root)const {
    const rhi::ShaderCursor cursor(root);
    for(const auto pair:{std::pair{"btWorkRows",rows.get()},std::pair{"btSchedule",schedule.get()}}) {
      auto field=cursor[pair.first];if(field.isValid())checked(field.setBinding(pair.second),pair.first);
    }
  }
  std::array<unsigned,16> header()const {
    std::array<unsigned,16> value{};
    checked(renderer.device->readBuffer(schedule,0,sizeof(value),value.data()),"BT occupied-row header readback");
    return value;
  }
};
}
