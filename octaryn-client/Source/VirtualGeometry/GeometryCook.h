#pragma once
#include "GeometryFormat.h"

namespace octaryn::client::rendering::virtual_geometry {
bool cook_geometry(const MapModel&,const std::string& source_hash,GeometryAsset&,std::string& error);
}
