#include "TileSessionInternal.h"
#include "Camera.h"
#include "TileRegionStatus.h"
#include "TileDesiredSet.h"
namespace octaryn::client::rendering {
bool TileSession::set_desired_regions(std::span<const std::uint32_t> wanted,std::span<const std::uint32_t> retained) {
 auto& s=*state_;return s.tiles.external_residency() && s.error.empty() &&
     tile_desired_set(s.tiles.tile_count(),wanted,retained,s.external_wanted,s.external_retained);
}
bool TileSession::region_count(std::uint32_t& count,std::uint64_t& generation) const {
 const auto& s=*state_;count=s.tiles.tile_count();generation=s.statistics.generation;return count>0;
}
bool TileSession::region_status(std::uint32_t index,octaryn_host_region_status& out) const {
 const auto& s=*state_;const auto* tile=s.tiles.tile(index);if(!tile || index>=s.entries.size())return false;
 const auto& entry=s.entries[index];
 out=tile_region_status(*tile,index,std::uint32_t(entry.phase),entry.wanted,entry.keep,
     entry.phase==State::Phase::Ready,s.collision && s.collision->contains(index),!s.error.empty(),s.statistics.generation);
 return true;
}
TileStartupReadiness TileSession::startup_readiness(const ::camera& view) const {
  const auto& s=*state_;
  TileStartupReadiness out;
  out.generation=s.statistics.generation;
  out.requested_set_hash=14695981039346656037ull;
  out.total=s.tiles.tile_count();out.resident=s.statistics.resident;
  out.requested=s.statistics.wanted;out.requested_ready=capture_ready();
  out.all_manifest_ready=out.total>0 && s.error.empty();
  for(unsigned i=0;i<out.total;++i) {
    const auto& entry=s.entries[i];const auto& tile=*s.tiles.tile(i);
    const bool ready=entry.phase==State::Phase::Ready;
    out.all_manifest_ready=out.all_manifest_ready && ready;
    if(entry.wanted)for(unsigned byte=0;byte<4;++byte) {
      out.requested_set_hash^=(i>>(8*byte))&255u;
      out.requested_set_hash*=1099511628211ull;
    }
    const auto* b=tile.bounds;
    // External catalogs can contain mutually exclusive resolutions of the same
    // surface. Unselected alternatives are not missing visible geometry.
    if((!s.tiles.external_residency() || entry.wanted) &&
        camera_is_box_visible(&view,b[0],b[1],b[2],b[3]-b[0],b[4]-b[1],b[5]-b[2])) {
      ++out.visible;if(!ready)++out.visible_missing;
    }
  }
  return out;
}
}
