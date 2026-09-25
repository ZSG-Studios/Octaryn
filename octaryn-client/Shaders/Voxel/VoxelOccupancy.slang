#include "VoxelTypes.slang"

uint voxel_occupancy_lane_bit(uint localY)
{
    return 1u << (localY + 1u);
}
