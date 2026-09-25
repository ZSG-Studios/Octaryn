#include "BlockTransportInternal.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <limits>

namespace octaryn::client::rendering {
bool prepare_block_transport_admission(WorldRenderer& r,bool reset) {
  auto& s=r.block_gi;auto& a=s.admission;a.pending_count=0;
  s.stats.admission_faces=0;s.stats.admission_columns=0;
  if(!block_admission_bounds(s.pending_cache_origin,a.minimum,a.maximum))return false;
  block_contributor_bounds(a.minimum,a.maximum,a.contributor_minimum,a.contributor_maximum);
  const auto min_x=block_admission_column(a.minimum[0]),max_x=block_admission_column(a.maximum[0]-1);
  const auto min_z=block_admission_column(a.minimum[2]),max_z=block_admission_column(a.maximum[2]-1);
  // A 64-cell window intersects at most nine 32-cell columns. No mesh readback.
  for(auto x=min_x;x<=max_x;++x)for(auto z=min_z;z<=max_z;++z) {
    const auto found=r.columns.find({x,z});if(found==r.columns.end())continue;
    const auto& source=found->second;
    if(source.height<=0 || std::int64_t(source.min_y)+source.height<=a.minimum[1] ||
        source.min_y>=a.maximum[1])continue;
    const auto faces=std::uint64_t(source.pass_counts[0])+source.pass_counts[1];
    if(faces>source.face_count || source.face_count>UINT32_MAX/16)return false;
    if(!faces)continue;
    if(!source.faces || source.faces->getDesc().size<std::uint64_t(source.face_count)*16 ||
        !world_ray_mesh_published(r,x,z,source) || a.pending_count==BlockAdmissionColumns)return false;
    BlockAdmissionColumn next{x,z,static_cast<std::uint32_t>(faces),0,0};
    if(!reset)for(unsigned i=0;i<a.count;++i) {
      const auto& previous=a.columns[i];
      if(previous.x==x && previous.z==z && previous.faces==next.faces) {next=previous;break;}
    }
    a.pending_columns[a.pending_count++]=next;
  }
  for(unsigned i=0;i<a.pending_count;++i) {
    const auto budget=BlockAdmissionFaces/a.pending_count+(i<BlockAdmissionFaces%a.pending_count?1u:0u);
    a.ranges[i]=block_admission_advance(a.pending_columns[i],budget);
    s.stats.admission_faces+=a.ranges[i].count;
  }
  s.stats.admission_columns=a.pending_count;
  return true;
}
bool admit_block_transport_world(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  auto& s=r.block_gi;const auto& a=s.admission;
  const std::array<unsigned,4> frame{s.pending_epoch,unsigned(r.frames),BlockTransportCapacity,BlockTransportLinks};
  for(unsigned i=0;i<a.pending_count;++i) {
    const auto& column=a.pending_columns[i];const auto range=a.ranges[i];
    if(!range.count)continue;
    const auto found=r.columns.find({column.x,column.z});if(found==r.columns.end())return false;
    const auto& source=found->second;
    const std::array<unsigned,4> faces{range.offset,range.count,source.face_count,0};
    auto* pass=commands->beginComputePass();if(!pass)return false;
    auto* root=pass->bindPipeline(s.admit);bool ok=root!=nullptr;
    if(ok) {
      rhi::ShaderCursor c(root);
      ok=world_rhi_ok(c["btFrameInfo"].setData(frame.data(),sizeof(frame))) &&
          world_rhi_ok(c["btAdmissionFaces"].setData(faces.data(),sizeof(faces))) &&
          world_rhi_ok(c["btAdmissionMin"].setData(a.minimum.data(),sizeof(a.minimum))) &&
          world_rhi_ok(c["btAdmissionMax"].setData(a.maximum.data(),sizeof(a.maximum))) &&
          world_rhi_ok(c["btWorldFaces"].setBinding(rhi::Binding(source.faces))) &&
          world_rhi_ok(c["btSurfaces"].setBinding(rhi::Binding(s.surfaces))) &&
          world_rhi_ok(c["btCounters"].setBinding(rhi::Binding(s.counters))) && bind_world_atlas(r.atlas,root);
    }
    if(ok)pass->dispatchCompute(range.count,1,1);
    pass->end();if(!ok)return false;
    commands->globalBarrier();
  }
  return true;
}
void commit_block_transport_admission(WorldRenderer& r,bool reset) {
  auto& s=r.block_gi;auto& a=s.admission;
  a.columns=a.pending_columns;a.count=a.pending_count;a.generation=a.pending_generation;
  std::uint64_t sweeps=a.count?std::numeric_limits<std::uint64_t>::max():1;
  for(unsigned i=0;i<a.count;++i)sweeps=std::min(sweeps,a.columns[i].sweeps);
  if(reset) {
    s.stats.admission_sweeps=0;s.stats.admission_complete_frame=0;s.stats.transport_ready=false;
    s.stats.checked_sweeps=0;s.stats.checked_admission_failures=0;s.stats.mandatory_ready=false;
    s.stats.contributor_start_frame=UINT64_MAX;
  }
  if(!s.stats.admission_sweeps && sweeps)s.stats.admission_complete_frame=r.frames;
  s.stats.admission_sweeps=sweeps;
}
}
