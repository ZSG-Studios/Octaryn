#pragma once
#include "MapModel.h"
#include <fastgltf/types.hpp>
namespace octaryn::client::rendering {
MapMaterial load_map_material(const fastgltf::Asset&,const fastgltf::Primitive&);
}
