#include "WorldRendererInternal.h"
#include "DynamicReceivers.h"
#include "BlockTransportInternal.h"
#include <array>
#include <filesystem>
#include <fstream>

namespace octaryn::client::rendering {
bool capture_block_transport_ready(WorldRenderer& r,bool& ready) {
  ready=true;
  if(r.gi_mode!=GiMode::BlockTransport)return true;
  if(!resolve_block_transport_statistics(r))return false;
  ready=r.block_gi.stats.transport_ready && r.block_gi.stats.measured_frame==r.frames;
  return true;
}
bool capture_dynamic_receiver_values(WorldRenderer& r,std::ostream& out) {
  const std::array<const DynamicReceivers*,2> receivers{player_receiver_stats(r.player),item_receiver_stats(r.items)};
  const char* names[]{"player_gi","item_gi"};std::uint64_t bytes{};
  for(unsigned owner=0;owner<receivers.size();++owner) {
    const auto* s=receivers[owner];std::array<std::uint32_t,4> counters{};
    if(s && s->active && s->counters &&
        !world_rhi_ok(r.device->readBuffer(s->counters,0,sizeof(counters),counters.data())))return false;
    if(s)bytes+=s->gpu_bytes;
    out<<",\n  \""<<names[owner]<<"\":{\"active\":"<<(s && s->active?"true":"false")
        <<",\"receivers\":"<<(s?s->count:0)<<",\"samples_per_receiver\":"<<(s?s->samples:0)
        <<",\"gpu_bytes\":"<<(s?s->gpu_bytes:0)<<",\"query_calls\":[";
    for(unsigned i=0;i<counters.size();++i)out<<(i?",":"")<<counters[i];
    out<<"]}";
  }
  out<<",\n  \"dynamic_receiver_gpu_bytes\":"<<bytes
      <<",\n  \"all_gi_gpu_bytes\":"<<r.block_gi.stats.total_gpu_bytes+bytes;
  return bool(out);
}
// Fenced diagnostics preserve exact identities and linear irradiance/pi.
bool capture_block_transport_values(WorldRenderer& r,const char* path) {
  const auto* enabled=SDL_getenv("OCTARYN_CLIENT_CAPTURE_GI_CACHE");
  if(!enabled || std::string_view(enabled)!="1" || r.gi_mode!=GiMode::BlockTransport)return true;
  auto& s=r.block_gi;if(!s.active)return false;
  const std::array<rhi::IBuffer*,4> sources{s.surfaces,s.direct_values,s.environment,s.output};
  std::array<Slang::ComPtr<ISlangBlob>,4> blobs;
  for(unsigned i=0;i<sources.size();++i) {
    const auto bytes=std::uint64_t(BlockTransportCapacity)*(i?16:sizeof(BlockTransportSurface));
    if(!world_rhi_ok(r.device->readBuffer(sources[i],0,bytes,blobs[i].writeRef())) ||
        !blobs[i] || blobs[i]->getBufferSize()!=bytes)return false;
  }
  auto target=std::filesystem::path(path);target+=".gi.bin";
  std::ofstream out(target,std::ios::binary);
  const std::array<char,16> magic{'B','T','G','I','_','C','A','C','H','E','_','V','1',0,0,0};
  const std::array<std::uint32_t,8> header{1,BlockTransportCapacity,s.epoch,s.radiance_epoch,
      std::uint32_t(r.frames),std::uint32_t(r.frames>>32),sizeof(BlockTransportSurface),16};
  out.write(magic.data(),magic.size());
  out.write(reinterpret_cast<const char*>(header.data()),sizeof(header));
  for(const auto& blob:blobs)
    out.write(static_cast<const char*>(blob->getBufferPointer()),blob->getBufferSize());
  return bool(out);
}
}
