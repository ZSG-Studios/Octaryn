#include "VoxelGreedyReference.h"

#include "VoxelLayout.h"

namespace octaryn::client::voxel {
namespace {

PackedVoxelQuad16 make_shell_quad(std::uint32_t x, std::uint32_t y,
                                  std::uint32_t z, FaceDirection direction,
                                  std::uint32_t material_id) noexcept {
  DecodedVoxelQuad quad{};
  quad.local_x = x;
  quad.local_y = y;
  quad.local_z = z;
  quad.width = ChunkWidthBlocks;
  quad.height = ChunkWidthBlocks;
  quad.face_dir = static_cast<std::uint32_t>(direction);
  quad.material_id = material_id;
  return pack_voxel_quad16(quad);
}

} // namespace

std::vector<PackedVoxelQuad16>
build_uniform_solid_shell_quads(std::uint32_t material_id) noexcept {
  return {
      make_shell_quad(31u, 0u, 0u, FaceDirection::PosX, material_id),
      make_shell_quad(0u, 0u, 0u, FaceDirection::NegX, material_id),
      make_shell_quad(0u, 31u, 0u, FaceDirection::PosY, material_id),
      make_shell_quad(0u, 0u, 0u, FaceDirection::NegY, material_id),
      make_shell_quad(0u, 0u, 31u, FaceDirection::PosZ, material_id),
      make_shell_quad(0u, 0u, 0u, FaceDirection::NegZ, material_id),
  };
}

bool solid_shell_quads_valid(const std::vector<PackedVoxelQuad16> &quads,
                             std::uint32_t material_id) noexcept {
  if (quads.size() != 6u) {
    return false;
  }
  for (const PackedVoxelQuad16 &packed : quads) {
    const DecodedVoxelQuad quad = unpack_voxel_quad16(packed);
    if (!voxel_quad16_fields_valid(quad) || quad.material_id != material_id ||
        quad.width != ChunkWidthBlocks || quad.height != ChunkWidthBlocks ||
        quad.face_dir >= 6u) {
      return false;
    }
  }
  return true;
}

} // namespace octaryn::client::voxel
