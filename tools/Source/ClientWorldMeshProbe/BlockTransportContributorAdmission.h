#pragma once
#include "BlockTransportWork.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportPolicy.h"

namespace mesh_probe {
inline void block_transport_contributor_domain_cases(WorldRenderer& r) {
  using Words=std::array<unsigned,4>;
  constexpr unsigned Capacity=256,Epoch=71,Radiance=73,Links=16;
  BlockTransportWork work(r,Capacity,1);
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/AdmitContributors.slang",
      "main",pipeline),"BT production contributor admission pipeline");
  std::array<std::int32_t,4> minimum{},maximum{};
  block_contributor_bounds({-65,-33,-97,0},{-1,31,-33,0},minimum,maximum);
  require(minimum==std::array<std::int32_t,4>{-163,-131,-195,0} &&
      maximum==std::array<std::int32_t,4>{97,129,65,0},"BT contributor98m signed domain");
  std::array<BlockTransportSurface,Capacity> initial{};
  const BlockSurfaceKey receiver{-33,-1,-65,3};const auto receiver_slot=block_surface_hash(receiver)&(Capacity-1);
  initial[receiver_slot]={receiver,{.5f,.5f,.5f,1},{Epoch,0,UINT32_MAX,0},{1,1,1,1}};
  std::array<BlockTransportCandidate,Links> candidates{};
  for(auto& candidate:candidates)candidate.key.direction=UINT32_MAX;
  for(unsigned axis=0;axis<3;++axis)for(unsigned side=0;side<2;++side) {
    const unsigned index=axis*2+side;
    std::array<int,3> inside{-33,-1,-65},outside=inside;
    inside[axis]=side?maximum[axis]-1:minimum[axis];outside[axis]=side?maximum[axis]:minimum[axis]-1;
    candidates[index]={{inside[0],inside[1],inside[2],3},{.2f,.4f,.8f,1}};
    candidates[index+6]={{outside[0],outside[1],outside[2],3},{.8f,.4f,.2f,1}};
  }
  std::array<unsigned,12> stats{};stats[8]=1;
  std::array<std::array<float,4>,Capacity> zero{};
  const auto writable=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination;
  const auto surfaces=buffer(r,initial.data(),sizeof(initial),sizeof(initial[0]),writable);
  const auto counters=buffer(r,stats.data(),sizeof(stats),sizeof(unsigned),writable);
  const auto inputs=buffer(r,candidates.data(),sizeof(candidates),sizeof(candidates[0]),rhi::BufferUsage::ShaderResource);
  const auto radiance=buffer(r,zero.data(),sizeof(zero),sizeof(zero[0]),rhi::BufferUsage::ShaderResource);
  unsigned frame=800;
  const auto run=[&](unsigned limit,unsigned repeats) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT contributor domain frame fence");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT contributor domain commands");
    const Words info{Epoch,frame,Capacity,Links},budget{0,1,Radiance,0};
    work.encode(commands,surfaces,radiance,radiance,radiance,info,Radiance);
    for(unsigned repeat=0;repeat<repeats;++repeat) {
      auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT contributor domain pass");
      auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT contributor domain bind");
      work.bind(root);const rhi::ShaderCursor c(root);
      checked(c["btSurfaces"].setBinding(surfaces),"BT contributor domain surfaces");
      checked(c["btCounters"].setBinding(counters),"BT contributor domain counters");
      checked(c["btCandidates"].setBinding(inputs),"BT contributor domain candidates");
      checked(c["btFrameInfo"].setData(info.data(),sizeof(info)),"BT contributor domain frame");
      checked(c["btWork"].setData(budget.data(),sizeof(budget)),"BT contributor domain work");
      checked(c["btContributorMin"].setData(minimum.data(),sizeof(minimum)),"BT contributor domain minimum");
      checked(c["btContributorMax"].setData(maximum.data(),sizeof(maximum)),"BT contributor domain maximum");
      checked(c["btContributorLimit"].setData(&limit,sizeof(limit)),"BT contributor domain limit");
      pass->dispatchCompute(1,1,1);pass->end();commands->globalBarrier();
    }
    auto command=commands->finish();require(bool(command),"BT contributor domain finish");
    require(r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BT contributor domain bounded completion");
    checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT contributor domain counters readback");
    checked(r.device->readBuffer(surfaces,0,sizeof(initial),initial.data()),"BT contributor domain surfaces readback");
    block_transport_complete(r,start);
  };
  run(0,1);require(stats[8]==1 && stats[11]==0,"BT zero-budget production admission allocated a new row");
  ++frame;run(Capacity,8);require(stats[8]==7 && stats[11]==0,"BT production signed contributor admission missed allowed keys");
  ++frame;run(0,1);require(stats[8]==7 && stats[11]==0,"BT zero-budget production admission lost existing rows");
  for(unsigned candidate=0;candidate<12;++candidate) {
    unsigned matches=0;
    for(const auto& surface:initial)if(surface.state[0]==Epoch && surface.key==candidates[candidate].key) {
      ++matches;require(surface.extra[1]==frame && surface.extra[3]==0,"BT existing contributor was not touched or became pinned");
    }
    require(matches==(candidate<6?1u:0u),"BT contributor min-inclusive max-exclusive signed boundary mismatch");
  }
  require(initial[receiver_slot].extra[3]==1,"BT contributor dispatch changed mandatory pin");
  std::puts("block_transport_contributor_domain=passed production_admit=1 signed_axes=3 boundary_cases=12 margin=98 zero_budget=1 existing_touch=1 validation_errors=0");
}
}
