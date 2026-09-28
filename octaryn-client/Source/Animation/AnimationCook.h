#pragma once
#include "CookedAsset.h"

namespace octaryn::client::animation {
bool cook_animation(const std::filesystem::path& source,CookedAsset&,std::string& error);
// Already imported assets use a digest supplied by their owning asset pipeline.
bool build_animation_geometry(CookedAsset&,const std::string& source_hash,std::string& error);
}
