#pragma once
#include "BlockTransportSetup.h"
#include "AtlasInternal.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <thread>

namespace mesh_probe::world_admission {
using Words=std::array<unsigned,4>;
using Pixel=std::array<float,4>;
using Key=std::array<std::int32_t,4>;
using Snapshot=std::map<Key,Pixel>;
constexpr unsigned Capacity=8192,Epoch=131,Frame=17,Links=16;
inline Key key(BlockSurfaceKey value) {return {value.x,value.y,value.z,int(value.direction)};}
inline void cap(WorldRenderer& r,std::chrono::steady_clock::time_point start) {
  std::this_thread::sleep_until(start+std::chrono::milliseconds(34));
  std::ofstream heartbeat("frame-timing.csv",std::ios::app);
  heartbeat<<r.frames++<<','<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
  require(bool(heartbeat),"BT world admission heartbeat write");
}
inline void submit(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto command=commands->finish();require(bool(command),"BT world admission command finish");
  require(r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
      "BT world admission bounded completion");
}
inline unsigned material(Fixture& f,const char* name) {
  const std::string id=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id==id)return i;
  require(false,"BT world admission material missing");return 0;
}
inline std::set<Key> expected(Fixture& f,const StreamColumn& source,const Key& low,const Key& high) {
  std::set<Key> result;
  for(const auto& face:f.expected(source)) {
    const unsigned id=face[3]&65535,direction=(face[3]>>16)&15;
    const auto& block=f.catalog[id];
    if(!block.opaque || block.fluidKind!="none" || block.requiresSolidBase)continue;
    const int x=std::bit_cast<int>(face[0]),y=std::bit_cast<int>(face[1]),z=std::bit_cast<int>(face[2]);
    if(x<low[0] || x>=high[0] || y<low[1] || y>=high[1] || z<low[2] || z>=high[2])continue;
    if(!block.sprite)result.insert({x,y,z,int(direction)});
    else if(direction==6 || direction==8) {
      const unsigned layer=world_atlas_preview_layer(f.renderer.atlas,id);
      for(unsigned side=0;side<2;++side)result.insert({x,y,z,int((layer<<4)|(direction+side))});
    }
  }
  return result;
}
struct Cache {
  WorldRenderer& r;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::IBuffer> surfaces,counters;
  explicit Cache(WorldRenderer& renderer):r(renderer) {
    const auto rw=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
    std::vector<BlockTransportSurface> zero(Capacity);std::array<unsigned,12> stats{};
    surfaces=buffer(r,zero.data(),zero.size()*sizeof(zero[0]),sizeof(zero[0]),rw);
    counters=buffer(r,stats.data(),sizeof(stats),sizeof(unsigned),rw);
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Admit.slang",
        "main",pipeline),"BT world admission production pipeline");
  }
  std::vector<BlockTransportSurface> read()const {
    std::vector<BlockTransportSurface> rows(Capacity);
    checked(r.device->readBuffer(surfaces,0,rows.size()*sizeof(rows[0]),rows.data()),"BT world admission cache readback");
    return rows;
  }
  Snapshot snapshot()const {
    Snapshot result;
    for(const auto& row:read())if(row.state[0]==Epoch) {
      require(row.extra[3]==1,"BT admitted resident surface lacks its world-region pin");
      require(result.emplace(key(row.key),row.albedo).second,"BT world admission duplicated a full surface identity");
    }
    return result;
  }
  void admit(rhi::IBuffer* faces,unsigned face_count,const Key& low,const Key& high) {
    for(unsigned offset=0;offset<face_count;offset+=256) {
      const auto start=std::chrono::steady_clock::now();const unsigned count=std::min(256u,face_count-offset);
      r.active_frame=r.frame_queue.slot(r.frames);
      require(r.frame_queue.wait(r.active_frame,2000),"BT world admission frame reuse");
      auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT world admission encoder");
      auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT world admission pass");
      auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT world admission pipeline binding");
      require(bind_world_atlas(r.atlas,root),"BT world admission atlas and mask binding");
      const rhi::ShaderCursor cursor(root);
      for(const char* forbidden:{"btSurfaceKeys","positions","voxels","colors","btDimensions","eye","projection"})
        require(!cursor[forbidden].isValid(),"BT world admission retained a screen or projection resource");
      checked(cursor["btWorldFaces"].setBinding(faces),"BT resident world face buffer");
      checked(cursor["btSurfaces"].setBinding(surfaces),"BT world admission surface cache");
      checked(cursor["btCounters"].setBinding(counters),"BT world admission counters");
      const Words info{Epoch,Frame,Capacity,Links},range{offset,count,face_count,0};
      checked(cursor["btFrameInfo"].setData(info.data(),sizeof(info)),"BT world admission epochs");
      checked(cursor["btAdmissionFaces"].setData(range.data(),sizeof(range)),"BT world admission packed face range");
      checked(cursor["btAdmissionMin"].setData(low.data(),sizeof(low)),"BT world admission integer minimum");
      checked(cursor["btAdmissionMax"].setData(high.data(),sizeof(high)),"BT world admission integer maximum");
      pass->dispatchCompute(count,1,1);pass->end();submit(r,commands);cap(r,start);
    }
  }
  Snapshot fill(rhi::IBuffer* faces,unsigned count,const Key& low,const Key& high,const std::set<Key>& wanted) {
    Snapshot actual;unsigned passes=0;
    for(;passes<16;) {
      admit(faces,count,low,high);++passes;actual=snapshot();if(actual.size()==wanted.size())break;
    }
    require(actual.size()==wanted.size(),"BT world admission did not cover resident surfaces within bounded retries");
    for(const auto& [identity,rho]:actual) {
      require(wanted.contains(identity),"BT world admission escaped the independent voxel-surface oracle");
      for(unsigned c=0;c<3;++c)require(std::isfinite(rho[c]) && rho[c]>=0 && rho[c]<=1,"BT world face reflectance bounds");
    }
    std::array<unsigned,12> before{},stats{};
    checked(r.device->readBuffer(counters,0,sizeof(before),before.data()),"BT world admission historical failures");
    require(count>0 && before[8]==wanted.size() && before[8]<=Capacity && before[6]==0 && before[7]==0,
        "BT world admission lost live occupancy, exceeded capacity or issued rays");
    // Confirm a whole production pass after every expected key is already present.
    admit(faces,count,low,high);++passes;
    require(snapshot()==actual,"BT clean admission pass changed exact keys, pins or reflectance");
    checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT world admission clean-pass readback");
    require(stats[8]==wanted.size() && stats[8]<=Capacity && stats[6]==0 && stats[7]==0,
        "BT clean admission pass lost live occupancy, exceeded capacity or issued rays");
    require(stats[11]==before[11],"BT complete resident admission pass still has new mandatory failures");
    require(stats[0]<=17ull*count*1024,"BT world admission exceeded fixed per-face expansion bound");
    std::printf("block_transport_world_clean_sweep=passed rows=%zu historical_failures=%u current_failures=%u new_failures=%u passes=%u exact_keys=1 occupancy=1 no_rays=1\n",
        wanted.size(),before[11],stats[11],stats[11]-before[11],passes);
    return actual;
  }
};
void expansion(Fixture&,const Mesh&);
void budget(Fixture&,Cache&);
void room(Fixture&);
}
