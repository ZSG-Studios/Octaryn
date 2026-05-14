#pragma once

#include <cstdint>
#include <cstddef>

constexpr int32_t kWorldRenderSectionSize = 32;
constexpr uint32_t kWorldRenderSectionLoaded = 1u << 0u;
constexpr uint32_t kWorldRenderSectionEmpty = 1u << 1u;
constexpr uint32_t kWorldRenderSectionSolid = 1u << 2u;

struct world_render_section_key {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;

  bool operator==(const world_render_section_key &other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct world_render_section_key_hash {
  size_t operator()(const world_render_section_key &key) const {
    uint64_t hash = 1469598103934665603ull;
    hash = (hash ^ static_cast<uint32_t>(key.x)) * 1099511628211ull;
    hash = (hash ^ static_cast<uint32_t>(key.y)) * 1099511628211ull;
    hash = (hash ^ static_cast<uint32_t>(key.z)) * 1099511628211ull;
    return static_cast<size_t>(hash);
  }
};

struct world_render_section_visibility {
  uint8_t face_masks[6]{};
};

struct world_render_section_state {
  world_render_section_key key{};
  world_render_section_visibility visibility{};
  uint32_t flags = 0u;
};

world_render_section_visibility world_render_section_visibility_all();
world_render_section_visibility world_render_section_visibility_none();
bool world_render_section_faces_can_see(
    const world_render_section_visibility &visibility, uint32_t from_face,
    uint32_t to_face);
void world_render_section_connect_faces(
    world_render_section_visibility &visibility, uint32_t first_face,
    uint32_t second_face);
uint32_t world_render_section_opposite_face(uint32_t face);
