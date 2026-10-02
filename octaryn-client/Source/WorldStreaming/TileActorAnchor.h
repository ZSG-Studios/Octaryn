#pragma once
#include "octaryn_residency_api.h"
#include <cmath>
namespace octaryn::client::rendering {
inline bool tile_actor_anchor(bool streaming,bool valid,bool authoritative,float x,float y,float z,octaryn_host_region_anchor& out) {
 out={};if(!streaming || !valid || !authoritative || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))return false;
 out={x,y,z};return true;
}
}
