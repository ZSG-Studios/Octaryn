#include "TerrainVegetation.h"
#include <cstdio>
#include <limits>

// CPU-only location scout. It samples the production basegame kernel, never authored trees.
int main() {
  using namespace octaryn::basegame::terrain;
  struct Rules {
    int water_height{30};
    uint16_t water_block{14}, sand_block{3}, grass_block{1}, dirt_block{2}, stone_block{5}, snow_block{4};
  } rules;
  int best_x{}, best_z{}, best_y{}, best_trees{}, best_flowers{};
  double best = -std::numeric_limits<double>::infinity();
  for (int z = -512; z <= 512; z += 16) for (int x = -512; x <= 512; x += 16) {
    const auto camera = sample_column(x, z);
    if (classify_biome(camera, rules) != Biome::Forest || tree_anchor(x, z)) continue;
    int trees = 0, flowers = 0;
    for (int az = z - 18; az <= z - 3; ++az) for (int ax = x - 10; ax <= x + 10; ++ax)
      emit_vegetation(ax, az, rules, sample_column, [&](int fx, int fy, int fz, uint16_t block) {
        if (fx != ax || fz != az) return;
        const auto ground = sample_column(ax, az).terrain_height;
        if (std::abs(ground - camera.terrain_height) > 3 || fy != ground + 1) return;
        trees += block == LogBlock;
        flowers += block >= 10 && block <= 13;
      });
    const double score = trees * 10 + std::min(flowers, 8) * 2 - std::hypot(x, z) * 0.02;
    if (trees >= 4 && flowers >= 2 && score > best) {
      best = score; best_x = x; best_z = z; best_y = camera.terrain_height;
      best_trees = trees; best_flowers = flowers;
    }
  }
  if (!std::isfinite(best)) return 1;
  std::printf("{\"revision\":3,\"seed\":1337,\"biome\":\"Forest\",\"x\":%.1f,\"y\":%.2f,"
      "\"z\":%.1f,\"pitch\":0.0,\"yaw\":0.0,\"visible_region_trees\":%d,\"visible_region_flowers\":%d}\n",
      best_x + 0.5, best_y + 2.62, best_z + 0.5, best_trees, best_flowers);
}
