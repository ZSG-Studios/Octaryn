#pragma once
#include <box3d/box3d.h>
#include <span>
#include <vector>
namespace octaryn::character_motion {
bool attach_decomposed_scene_hull(b3BodyId,std::vector<b3HullData*>&,std::span<const b3Vec3>,const b3ShapeDef&);
}
