#pragma once
#include <string>
#include <utility>
namespace octaryn::client::rendering {
struct RayDiagnosticMode {
  bool compiled{},collecting{};
  std::string requested{"auto"},path;
};
inline bool resolve_ray_diagnostic_mode(const char* option,const char* path,RayDiagnosticMode& output,bool quiet_compiled=true) {
  RayDiagnosticMode result;
  result.path=path?path:"";result.collecting=!result.path.empty();
  if(option && *option) {
    result.requested=option;
    if(result.requested!="0" && result.requested!="1")return false;
    result.compiled=result.requested=="1";
  } else result.compiled=result.collecting || quiet_compiled;
  if(result.collecting && !result.compiled)return false;
  output=std::move(result);return true;
}
inline const char* ray_counter_cache_variant(bool compiled) {
  return compiled?"raycounters1":"raycounters0";
}
}
