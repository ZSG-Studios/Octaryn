#pragma once
#include "octaryn_scene_physics.h"
#include <box3d/box3d.h>
#include <vector>
namespace octaryn::character_motion {
// Hull storage must remain alive until the caller destroys the attached body.
bool attach_scene_body_shape(b3BodyId,std::vector<b3HullData*>&,
    const octaryn_scene_body_shape&,const b3ShapeDef&);
}
