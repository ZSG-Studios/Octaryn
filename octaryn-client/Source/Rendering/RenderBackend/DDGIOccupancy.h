#pragma once
#include "DDGISystem.h"
namespace octaryn::client::rendering {
struct WorldRenderer;
void ddgi_follow_opening(WorldRenderer&,DDGISystem&);
void ddgi_classify_occupancy(WorldRenderer&,DDGISystem&);
bool ddgi_ignore_covers(const DDGISystem&,std::int32_t column_x,std::int32_t column_z);
void ddgi_clear_ignore(DDGISystem&);
}
