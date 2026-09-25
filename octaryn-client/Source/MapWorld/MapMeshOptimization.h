#pragma once
#include "MapModel.h"
namespace octaryn::client::rendering {
// Byte-exact attributes and oriented triangles; blend triangle order is retained.
bool optimize_map_mesh(MapModel& model,std::string& error);
}
