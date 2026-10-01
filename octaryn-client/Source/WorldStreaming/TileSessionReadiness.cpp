#include "TileSessionInternal.h"
#include "Camera.h"
namespace octaryn::client::rendering {
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
    if(camera_is_box_visible(&view,b[0],b[1],b[2],b[3]-b[0],b[4]-b[1],b[5]-b[2])) {
      ++out.visible;if(!ready)++out.visible_missing;
    }
  }
  return out;
}
}
