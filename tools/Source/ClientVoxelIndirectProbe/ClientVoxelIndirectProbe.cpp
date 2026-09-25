#include "VoxelGreedyReference.h"
#include "VoxelIndirect.h"

#include <cstdio>

namespace {

bool fail(const char *message) {
  std::fprintf(stderr, "client voxel indirect probe failed: %s\n", message);
  return false;
}

bool validate_zero_quads() {
  const auto command =
      octaryn::client::voxel::make_voxel_quad_indirect_command(0u, 0u);
  if (command.index_count != 0u || command.instance_count != 0u) {
    return fail("zero quad batch must not create a draw command");
  }
  return true;
}

bool validate_solid_shell_command() {
  using namespace octaryn::client::voxel;
  const auto quads = build_uniform_solid_shell_quads(7u);
  const auto command = make_voxel_quad_indirect_command(
      11u, static_cast<std::uint32_t>(quads.size()), 3u);
  if (!indirect_command_valid(command)) {
    return fail("solid shell command must be a valid indexed quad draw");
  }
  if (command.index_count != 6u || command.instance_count != 6u ||
      command.first_index != 3u || command.vertex_offset != 0 ||
      command.first_instance != 11u) {
    return fail("solid shell command fields do not match face pulling");
  }
  return true;
}

} // namespace

int main() {
  if (!validate_zero_quads() || !validate_solid_shell_command()) {
    return 1;
  }
  std::puts(
      "client_voxel_indirect_probe=passed index_count=6 solid_instances=6");
  return 0;
}
