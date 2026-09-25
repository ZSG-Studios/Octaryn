#pragma once
#include "BlockTransportSetup.h"
#include "AtlasInternal.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <thread>

namespace mesh_probe::plant_probe {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
using Vector=std::array<float,3>;
struct Query {
  Words face{};Pixel toward{};BlockSurfaceKey key{};Words options{};
  Pixel origin{},direction{};
};
struct Result {
  Words packed{};BlockSurfaceKey key{};Pixel origin{},direction{},normal{};Words flags{};
};
static_assert(sizeof(Query)==96 && sizeof(Result)==96);
inline Vector normal(unsigned plane,unsigned side) {
  const float sign=side?-.7071067811865475f:.7071067811865475f;
  return {(plane?-1.f:1.f)*-sign,0,sign};
}
inline Vector point(Vector anchor,unsigned plane,float u,float v) {
  return {anchor[0]+1-u,anchor[1]+1-v,anchor[2]+(plane?u:1-u)};
}
inline bool opaque(SDL_Surface* atlas,unsigned layer,unsigned x,unsigned y) {
  require(x<32 && y<32 && layer<29,"BT plant PNG oracle bounds");
  const auto* row=static_cast<const Uint8*>(atlas->pixels)+y*unsigned(atlas->pitch);
  return row[(layer*32+x)*4+3]>=.35f*255;
}
inline unsigned tag(unsigned layer,unsigned plane,unsigned side) {return (layer<<4)|(6+2*plane+side);}
inline unsigned encoded_voxel(unsigned layer,unsigned plane,unsigned side) {
  return (layer<<3)|256u|512u|(plane<<10)|(side<<11);
}
inline void cap(WorldRenderer& r,std::chrono::steady_clock::time_point start) {
  std::this_thread::sleep_until(start+std::chrono::milliseconds(34));
  const auto now=std::chrono::steady_clock::now();
  std::ofstream file("frame-timing.csv",std::ios::app);
  file<<r.frames++<<','<<std::chrono::duration<double,std::milli>(now-start).count()<<'\n';
}
inline void submit(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto command=commands->finish();require(bool(command),"BT plant command finish");
  require(r.frame_queue.submit(r.queue,command,r.active_frame) &&
      r.frame_queue.wait(r.active_frame,2000),"BT plant bounded GPU completion");
}
inline std::vector<Result> dispatch(Fixture& f,const char* entry,const std::vector<Query>& queries,bool ray=false) {
  auto& r=f.renderer;const auto start=std::chrono::steady_clock::now();
  r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT plant frame reuse");
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportPlants.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),entry,pipeline),"BT plant production-helper pipeline");
  const auto rw=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
  const auto input=buffer(r,queries.data(),queries.size()*sizeof(Query),sizeof(Query),rhi::BufferUsage::ShaderResource);
  std::vector<Result> actual(queries.size());
  const auto output=buffer(r,actual.data(),actual.size()*sizeof(Result),sizeof(Result),rw);
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT plant command encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT plant pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT plant pipeline bind");
  require(bind_world_atlas(r.atlas,root),"BT plant atlas and mask bindings");
  if(ray)require(world_ray_bind(r,root),"BT plant exact scene binding");
  const rhi::ShaderCursor cursor(root);const Words info{unsigned(queries.size()),0,0,0};
  checked(cursor["plantQueries"].setBinding(input),"BT plant query input");
  checked(cursor["plantResults"].setBinding(output),"BT plant result output");
  checked(cursor["plantInfo"].setData(info.data(),sizeof(info)),"BT plant query count");
  if(ray) {const Pixel settings{1,16,.002f,0};
    checked(cursor["raySettings"].setData(settings.data(),sizeof(settings)),"BT plant alpha mip");}
  pass->dispatchCompute((unsigned(queries.size())+63)/64,1,1);pass->end();submit(r,commands);
  checked(r.device->readBuffer(output,0,actual.size()*sizeof(Result),actual.data()),"BT plant result readback");
  cap(r,start);return actual;
}
inline void settle(WorldRenderer& r) {
  for(unsigned attempt=0;attempt<64;++attempt) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT plant RT frame reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT plant RT encoder");
    require(world_ray_prepare(r,commands,r.active_frame),"BT plant exact RT scene preparation");
    submit(r,commands);const bool ready=world_ray_coverage_complete(r);cap(r,start);if(ready)return;
  }
  require(false,"BT plant RT scene failed bounded convergence");
}
void raster(Fixture&,unsigned material,unsigned layer);
void shadows(Fixture&,SDL_Surface*,unsigned layer);
}
