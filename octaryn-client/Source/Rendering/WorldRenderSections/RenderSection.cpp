#include "RenderSection.h"

world_render_section_visibility world_render_section_visibility_all() {
  world_render_section_visibility visibility{};
  for (uint32_t face = 0u; face < 6u; ++face) {
    visibility.face_masks[face] = 0x3fu;
  }
  return visibility;
}

world_render_section_visibility world_render_section_visibility_none() {
  return {};
}

bool world_render_section_faces_can_see(
    const world_render_section_visibility &visibility, uint32_t from_face,
    uint32_t to_face) {
  if (from_face >= 6u || to_face >= 6u) {
    return false;
  }
  return (visibility.face_masks[from_face] & (1u << to_face)) != 0u;
}

void world_render_section_connect_faces(
    world_render_section_visibility &visibility, uint32_t first_face,
    uint32_t second_face) {
  if (first_face >= 6u || second_face >= 6u) {
    return;
  }
  visibility.face_masks[first_face] |= static_cast<uint8_t>(1u << second_face);
  visibility.face_masks[second_face] |= static_cast<uint8_t>(1u << first_face);
}

uint32_t world_render_section_opposite_face(uint32_t face) {
  constexpr uint32_t kOppositeFaces[6] = {1u, 0u, 3u, 2u, 5u, 4u};
  return face < 6u ? kOppositeFaces[face] : 6u;
}
