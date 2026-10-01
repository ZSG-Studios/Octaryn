#include "RayGeometryInternal.h"
#include <algorithm>
namespace octaryn::client::rendering::virtual_geometry {
using namespace ray_geometry;
bool RayGeometry::record_compaction(rhi::ICommandEncoder* commands) {
  auto& s=*state_;try {
    check(s.device&&commands,"ray geometry is not initialized");check(!s.pending,"ray replacement already pending");
    s.error.clear();if(!s.current||!s.current->compact_sizes)return false;
    // Scene builds publish directly under whole-candidate admission. Their
    // shared vertices and retired snapshots must never be charged twice.
    if(s.config.scheduler)return false;
    auto source=s.current;std::vector<std::uint64_t> sizes(source->blas.size());
    checked(source->compact_sizes->getResult(0,static_cast<std::uint32_t>(sizes.size()),sizes.data()),"ray compacted sizes readback failed");
    bool useful=false;std::uint64_t new_bytes=0;
    for(std::size_t i=0;i<sizes.size();++i) {
      const auto original=source->blas[i]->getDesc().size;
      check(sizes[i]>0,"empty ray compacted size");
      sizes[i]=std::min(sizes[i],original);useful|=sizes[i]<original;new_bytes+=sizes[i];
    }
    if(!useful)return false;
    auto next=std::make_unique<State::Build>();next->scene=std::make_shared<RaySnapshot>(*source);
    next->source_scene=source;next->compaction=true;next->scene->compact_sizes=nullptr;
    next->scene->tlas=nullptr;if(source->tlas)next->scene->bytes-=source->tlas->getDesc().size;
    next->build_bytes=new_bytes;
    check(new_bytes<=s.config.maximum_build_bytes&&s.live_bytes()+new_bytes<=s.config.maximum_resident_bytes,"ray compaction budget exceeded");
    for(std::size_t i=0;i<sizes.size();++i) {
      next->scene->bytes-=source->blas[i]->getDesc().size;next->scene->bytes+=sizes[i];
      next->scene->blas[i]=acceleration(s.device,rhi::AccelerationStructureKind::BottomLevel,sizes[i]);
    }
    s.pending=std::move(next);auto& pending=*s.pending;
    for(std::size_t i=0;i<sizes.size();++i)
      commands->copyAccelerationStructure(pending.scene->blas[i],source->blas[i],rhi::AccelerationStructureCopyMode::Compact);
    commands->globalBarrier();s.tlas(pending,commands);pending.recorded=true;return true;
  } catch(const std::exception& e) {s.error=e.what();return false;}
}
}
