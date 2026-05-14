#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "core/check.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"
#include "world/window/internal.h"

void world_window_move_chunks_internal(const camera_t* camera)
{
    const int offset_x = world_window_origin_for_position(camera->position[0]) - world_origin_x_internal();
    const int offset_z = world_window_origin_for_position(camera->position[2]) - world_origin_z_internal();
    if (offset_x || offset_z)
    {
        world_window_queue_move_internal(world_origin_x_internal() + offset_x,
                                         world_origin_z_internal() + offset_z);
    }
}
