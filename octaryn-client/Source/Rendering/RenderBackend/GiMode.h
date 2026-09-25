#pragma once
#include <SDL3/SDL_stdinc.h>
#include <cstring>
namespace octaryn::client::rendering {
enum class GiMode { BlockTransport=1, Direct=2, Invalid=3 };
inline GiMode requested_gi_mode() {
  const char* value=SDL_getenv("OCTARYN_CLIENT_GI");
  if(!value || !*value || std::strcmp(value,"block-transport")==0)return GiMode::BlockTransport;
  if(std::strcmp(value,"direct")==0)return GiMode::Direct;
  return GiMode::Invalid;
}
inline const char* gi_mode_name(GiMode mode) {
  switch(mode) {
    case GiMode::BlockTransport:return "block-transport";
    case GiMode::Direct:return "direct";
    default:return "invalid";
  }
}
}
