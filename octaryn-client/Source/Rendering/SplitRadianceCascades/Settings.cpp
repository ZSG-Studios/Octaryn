#include "Config.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
namespace octaryn::client::rendering {
bool src_environment_config(SrcConfig& c,std::string& error) {
  struct FloatSetting {const char* name;float* value;};
  const FloatSetting floats[]={
    {"OCTARYN_SRC_SPACING",&c.spacing},{"OCTARYN_SRC_CONTACT_LENGTH",&c.contact_length},
    {"OCTARYN_SRC_BASE_INTERVAL",&c.base_interval},{"OCTARYN_SRC_INTERVAL_GROWTH",&c.interval_growth},
    {"OCTARYN_SRC_DECAY",&c.decay},{"OCTARYN_SRC_MAX_TRACE_DISTANCE",&c.max_trace_distance},
    {"OCTARYN_SRC_FEEDBACK",&c.feedback},{"OCTARYN_SRC_LOD_RADIUS",&c.lod_radius},{"OCTARYN_SRC_LOD_BLEND",&c.lod_blend}};
  for(const auto& setting:floats)if(const char* text=std::getenv(setting.name)) {
    char* end=nullptr;errno=0;float value=std::strtof(text,&end);
    if(errno||end==text||*end||!std::isfinite(value)) {error=std::string("Invalid SRC setting: ")+setting.name;return false;}
    *setting.value=value;
  }
  struct UintSetting {const char* name;unsigned* value;};
  const UintSetting integers[]={
    {"OCTARYN_SRC_CASCADES",&c.cascades},{"OCTARYN_SRC_ANGULAR_RESOLUTION",&c.angular_resolution},
    {"OCTARYN_SRC_BASE_CAPACITY",&c.base_capacity},{"OCTARYN_SRC_VISIBLE_LIFETIME",&c.visible_lifetime},
    {"OCTARYN_SRC_SECONDARY_LIFETIME",&c.secondary_lifetime},{"OCTARYN_SRC_MAX_SURFACE_RAYS",&c.max_surface_rays},
    {"OCTARYN_SRC_HASH_SEARCH_LIMIT",&c.hash_search_limit}};
  for(const auto& setting:integers)if(const char* text=std::getenv(setting.name)) {
    char* end=nullptr;errno=0;auto value=std::strtoull(text,&end,10);
    if(errno||end==text||*end||*text=='-'||value>std::numeric_limits<unsigned>::max()) {
      error=std::string("Invalid SRC setting: ")+setting.name;return false;
    }
    *setting.value=unsigned(value);
  }
  if(const char* text=std::getenv("OCTARYN_SRC_MEMORY_MIB")) {
    char* end=nullptr;errno=0;auto value=std::strtoull(text,&end,10);
    if(errno||end==text||*end||*text=='-'||value==0||value>1024) {error="SRC memory budget must be 1..1024 MiB";return false;}
    c.memory_limit=value*1024*1024;
  }
  SrcLayout layout;return src_validate(c,layout,error);
}
}
