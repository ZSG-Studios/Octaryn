#pragma once
#include "CookedAsset.h"

namespace octaryn::client::animation {
inline constexpr std::uint32_t animation_cache_version=1;
// An atomic manifest references an immutable, content-addressed geometry sidecar.
bool write_animation_cache(const std::filesystem::path&,const CookedAsset&,std::string& error);
bool read_animation_cache(const std::filesystem::path&,CookedAsset&,std::string& error,
    const std::string& expected_source_hash={},bool load_geometry_pages=true);
}
