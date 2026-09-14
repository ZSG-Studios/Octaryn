#include "PackedVoxelQuad.h"
#include "VoxelFaceMask.h"
#include "VoxelGreedyReference.h"
#include "VoxelLayout.h"

#include <cstdio>
#include <vector>

namespace {

bool expect_true(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "client voxel mesh probe failed: %s\n", message);
    return false;
  }
  return true;
}

bool validate_empty_chunk() {
  const octaryn::client::voxel::VoxelMaskChunk chunk{};
  return expect_true(octaryn::client::voxel::count_visible_faces(chunk) == 0u,
                     "empty chunk must emit zero visible faces");
}

bool validate_solid_chunk() {
  using namespace octaryn::client::voxel;
  const VoxelMaskChunk chunk = make_uniform_solid_chunk();
  constexpr std::uint32_t expected_faces =
      6u * ChunkWidthBlocks * ChunkWidthBlocks;
  if (!expect_true(count_visible_faces(chunk) == expected_faces,
                   "solid chunk must expose only exterior faces")) {
    return false;
  }
  const std::vector<PackedVoxelQuad16> shell =
      build_uniform_solid_shell_quads(42u);
  return expect_true(solid_shell_quads_valid(shell, 42u),
                     "solid chunk greedy shell must pack six quads");
}

bool validate_checkerboard_chunk() {
  using namespace octaryn::client::voxel;
  const VoxelMaskChunk chunk = make_checkerboard_chunk();
  constexpr std::uint32_t solid_voxels = ChunkVoxelCount / 2u;
  constexpr std::uint32_t expected_faces = solid_voxels * 6u;
  return expect_true(count_visible_faces(chunk) == expected_faces,
                     "checkerboard chunk must expose every solid face");
}

} // namespace

int main() {
  if (!validate_empty_chunk() || !validate_solid_chunk() ||
      !validate_checkerboard_chunk()) {
    return 1;
  }
  std::puts("client_voxel_mesh_probe=passed empty_faces=0 solid_shell_quads=6 "
            "checkerboard_faces=98304");
  return 0;
}
