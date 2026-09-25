#pragma once
#include "BlockTransportPlantProbe.h"
#include <algorithm>
#include <limits>
namespace mesh_probe::boundary {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
struct Query {BlockSurfaceKey key;Words info;Pixel origin,direction;};
struct Result {Pixel origin,direction,position;Words key;Pixel hit;};
static_assert(sizeof(Query)==64 && sizeof(Result)==80);
inline std::vector<Result> dispatch(Fixture& fixture,const std::vector<Query>& queries) {
  auto& r=fixture.renderer;Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportBoundary.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BT boundary retained production-helper pipeline");
  auto input=buffer(r,queries.data(),queries.size()*sizeof(Query),sizeof(Query),rhi::BufferUsage::ShaderResource);
  std::vector<Result> values(queries.size());
  auto output=buffer(r,values.data(),values.size()*sizeof(Result),sizeof(Result),rhi::BufferUsage::UnorderedAccess);
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT boundary fixture frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT boundary command encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT boundary pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT boundary binding");
  require(bind_world_atlas(r.atlas,root) && world_ray_bind(r,root),"BT boundary real atlas and scene");
  const rhi::ShaderCursor cursor(root);const unsigned count=unsigned(queries.size());
  const Pixel settings{1,64,.001f,0};const unsigned disabled=0;
  checked(cursor["boundaryQueries"].setBinding(input),"BT boundary query buffer");
  checked(cursor["boundaryResults"].setBinding(output),"BT boundary output buffer");
  checked(cursor["boundaryCount"].setData(&count,sizeof(count)),"BT boundary query count");
  checked(cursor["raySettings"].setData(settings.data(),sizeof(settings)),"BT boundary production TMin settings");
  if(cursor["playerShadowEnabled"].isValid())checked(cursor["playerShadowEnabled"].setData(&disabled,sizeof(disabled)),"BT boundary no actor");
  pass->dispatchCompute((count+63)/64,1,1);pass->end();plant_probe::submit(r,commands);
  checked(r.device->readBuffer(output,0,values.size()*sizeof(Result),values.data()),"BT boundary production readback");
  block_transport_complete(r,start);return values;
}
inline bool expected_position(BlockSurfaceKey key) {
  const int cells[3]={key.x,key.y,key.z};
  for(unsigned axis=0;axis<3;++axis) {
    const std::int64_t cell=cells[axis];
    if(cell < -16777216 || cell > 16777215)return false;
    const float low=float(cell),high=float(cell+1);
    if(double(low)!=double(cell) || double(high)!=double(cell+1))return false;
    if(axis!=key.direction/2 && std::nextafter(low,INFINITY)>std::nextafter(high,-INFINITY))return false;
  }
  return true;
}
inline void check_position(const Query& query,const Result& value) {
  const int cells[3]={query.key.x,query.key.y,query.key.z};const unsigned axis=query.key.direction/2;
  const double sign=query.key.direction%2?1:-1,plane=double(cells[axis])+(sign>0?1:0);
  const bool representable=expected_position(query.key);
  require((value.position[3]!=0)==representable,"BT boundary position validity differs from independent IEEE interval");
  if(!representable){require(value.origin[3]==0,"BT ambiguous world cell emitted transport ray");return;}
  const double step=std::abs(double(std::nextafter(float(plane),sign>0?INFINITY:-INFINITY))-plane);
  const bool shifted=step<1;
  require((value.origin[3]!=0)==shifted,"BT lost unit-plane precision was accepted or valid large position rejected");
  require(double(value.position[axis])==plane,"BT boundary direct sample left exact owner plane");
  if(shifted) {
    const double delta=(double(value.origin[axis])-plane)*sign;
    require(delta>0 && delta<=std::max(step,1./1048576)+step,"BT boundary offset is not outward and ULP bounded");
  }
  for(unsigned c=0;c<3;++c)if(c!=axis) {
    require(double(value.position[c])>cells[c] && double(value.position[c])<double(cells[c])+1,
        "BT rounded sample escaped its exact integer owner interval");
    if(shifted)require(value.origin[c]==value.position[c],"BT normal offset changed cube tangent domain");
  }
}
}
