#include "SrcProfile.h"
#include "System.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace octaryn::client::rendering {
namespace {
const char* counter_name(unsigned index) {
  switch (index) {
    case 40: return "insert_failures";
    case 42: return "unresolved_traces";
    case 43: return "trace_rays";
    case 44: return "dda_steps";
    case 45: return "occluded_links";
    case 48: return "ray_offset_base";
    case 78: return "donor_finds";
    case 79: return "donor_misses";
    case 80: return "oct_invalid";
    case 81: return "contact_candidates";
    case 82: return "resolve_pixels";
    case 83: return "zero_donor_pixels";
    case 84: return "gated_pixels";
    case 85: return "contact_traces";
    case 86: return "unknown_contacts";
    default: return nullptr;
  }
}
}
bool src_profile_enabled() {
  static const bool enabled = std::getenv("OCTARYN_SRC_PROFILE") != nullptr;
  return enabled;
}
bool src_profile_poll(SplitRadianceCascades& s,rhi::IDevice* device,std::uint64_t frame) {
  if(!s.initialized || !s.counters || !device)return true;
  std::uint32_t values[96]{};
  if(SLANG_FAILED(device->readBuffer(s.counters,0,sizeof(values),values)))return false;
  const char* path = std::getenv("OCTARYN_SRC_PROFILE_PATH");
  std::FILE* out = stderr;
  if(path && *path) {
    out = std::fopen(path,"a");
    if(!out)return true;
  }
  std::fprintf(out,"{\"frame\":%llu",static_cast<unsigned long long>(frame));
  for(unsigned i=0;i<96;++i) {
    const char* name = counter_name(i);
    if(!name || values[i]==0)continue;
    std::fprintf(out,",\"%s\":%u",name,values[i]);
  }
  for(unsigned c=0;c<8;++c) {
    if(values[c*4]||values[c*4+1]||values[c*4+2])
      std::fprintf(out,",\"cascade%u\":{\"capacity\":%u,\"taken\":%u,\"failed\":%u}",
                   c,values[c*4],values[c*4+1],values[c*4+2]);
  }
  std::fprintf(out,"}\n");
  if(out!=stderr)std::fclose(out);
  return true;
}
}
