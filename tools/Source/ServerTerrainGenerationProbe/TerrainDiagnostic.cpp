#include "TerrainDensity.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace terrain = octaryn::basegame::terrain;
namespace {
struct Materials {
  int water_height{30};
  std::uint16_t water_block{14}, sand_block{3}, grass_block{1}, dirt_block{2},
      stone_block{5}, snow_block{4};
};
constexpr Materials rules{};
using Color = std::array<std::uint8_t, 3>;
constexpr std::array biome_colors{Color{35, 102, 166}, Color{218, 199, 124},
    Color{114, 161, 77}, Color{44, 112, 62}, Color{226, 173, 85}, Color{228, 237, 238}};
constexpr std::array biome_names{"ocean", "beach", "plains", "forest", "desert", "alpine"};

Color block_color(std::uint16_t block) {
  switch (block) {
  case 0: return {17, 22, 30};
  case 1: return {106, 160, 69};
  case 2: return {118, 81, 56};
  case 3: return {218, 195, 130};
  case 4: return {231, 238, 243};
  case 5: return {105, 110, 121};
  case 14: return {40, 112, 184};
  default: return {255, 0, 255};
  }
}

struct Image {
  int width, height;
  std::vector<Color> pixels;
  Image(int w, int h) : width(w), height(h), pixels(static_cast<std::size_t>(w * h)) {}
  Color& at(int x, int y) { return pixels[static_cast<std::size_t>(x + y * width)]; }
  void save(const std::filesystem::path& path) const {
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (const auto& pixel : pixels)
      out.write(reinterpret_cast<const char*>(pixel.data()), 3);
    if (!out) throw std::runtime_error("cannot write diagnostic image");
  }
};

void generate(const std::filesystem::path& output) {
  constexpr int spacing = 4, extent = 2048, width = 2 * extent / spacing + 1;
  Image height_map(width, width), material_map(width, width), biome_map(width, width);
  std::array<std::uint64_t, 6> biome_count{};
  int minimum = std::numeric_limits<int>::max(), maximum = std::numeric_limits<int>::min();
  for (int iz = 0; iz < width; ++iz) for (int ix = 0; ix < width; ++ix) {
    const auto sample = terrain::sample_column(ix * spacing - extent, iz * spacing - extent);
    const auto biome = static_cast<std::size_t>(terrain::classify_biome(sample, rules));
    ++biome_count[biome];
    minimum = std::min(minimum, sample.terrain_height);
    maximum = std::max(maximum, sample.terrain_height);
    const auto gray = static_cast<std::uint8_t>(std::clamp(sample.terrain_height, 0, 255));
    height_map.at(ix, iz) = {gray, gray, gray};
    biome_map.at(ix, iz) = biome_colors[biome];
    const auto material = terrain::classify_materials(sample, rules);
    auto color = block_color(sample.terrain_height < rules.water_height - 1 ? rules.water_block : material.surface_block);
    // Elevation shading is diagnostic and does not represent game lighting.
    const double brightness = 0.68 + 0.32 * std::clamp(sample.terrain_height / 180.0, 0.0, 1.0);
    for (auto& channel : color) channel = static_cast<std::uint8_t>(channel * brightness);
    material_map.at(ix, iz) = color;
  }
  height_map.save(output / "height.ppm");
  material_map.save(output / "surface-materials.ppm");
  biome_map.save(output / "biomes.ppm");

  constexpr int section_width = 1024, section_height = 512;
  Image section(section_width, section_height);
  std::uint64_t cave_voxels = 0, solid_voxels = 0;
  for (int ix = 0; ix < section_width; ++ix) {
    const auto sample = terrain::sample_column(ix - section_width / 2, 0);
    const auto material = terrain::classify_materials(sample, rules);
    for (int iy = 0; iy < section_height; ++iy) {
      const int y = terrain::WorldMaxYExclusive - 1 - iy;
      const auto block = terrain::sample_block(sample, y, rules, material);
      section.at(ix, iy) = y > sample.terrain_height && block == 0 ? Color{154, 186, 210} : block_color(block);
      if (y < sample.terrain_height - 8) {
        cave_voxels += block == 0 ? 1u : 0u;
        solid_voxels += block != 0 ? 1u : 0u;
      }
    }
  }
  section.save(output / "cave-section.ppm");
  std::ofstream report(output / "coverage.json");
  report << "{\n  \"seed\": " << terrain::Seed << ",\n  \"generator_revision\": " << terrain::GeneratorRevision
      << ",\n  \"map_extent\": [-2048, 2048],\n  \"map_sample_spacing\": 4,\n  \"sample_count\": " << width * width
      << ",\n  \"minimum_height\": " << minimum << ",\n  \"maximum_height\": " << maximum
      << ",\n  \"section_x\": [-512, 511],\n  \"section_z\": 0,\n  \"section_y\": [-256, 255],\n"
      << "  \"section_cave_voxels\": " << cave_voxels << ",\n  \"section_deep_solid_voxels\": " << solid_voxels
      << ",\n  \"biome_samples\": {\n";
  bool all_biomes = true;
  for (std::size_t i = 0; i < biome_names.size(); ++i) {
    report << "    \"" << biome_names[i] << "\": " << biome_count[i]
        << (i + 1 == biome_names.size() ? "\n" : ",\n");
    std::cout << biome_names[i] << '=' << biome_count[i] << ' ';
    all_biomes &= biome_count[i] > 0;
  }
  report << "  }\n}\n";
  if (!report) throw std::runtime_error("cannot write coverage report");
  std::cout << "\nterrain_diagnostic samples=" << width * width << " height_min=" << minimum
      << " height_max=" << maximum << " cave_voxels=" << cave_voxels << '\n';
  if (!all_biomes || cave_voxels == 0 || minimum >= rules.water_height || maximum <= 105)
    throw std::runtime_error("terrain diagnostic coverage is incomplete");
}
}

int main(int argc, char** argv) {
  try {
    if (argc != 2) throw std::runtime_error("usage: octaryn_terrain_diagnostic OUTPUT_DIRECTORY");
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    generate(output);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "terrain_diagnostic=failed " << error.what() << '\n';
    return 1;
  }
}
