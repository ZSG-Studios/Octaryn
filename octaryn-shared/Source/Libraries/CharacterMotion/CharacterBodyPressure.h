#pragma once
#include <box3d/box3d.h>
namespace octaryn::character_motion {
void apply_character_body_pressure(b3WorldId,b3Pos,const b3Capsule&,b3Vec3 desired_velocity,float seconds);
}
