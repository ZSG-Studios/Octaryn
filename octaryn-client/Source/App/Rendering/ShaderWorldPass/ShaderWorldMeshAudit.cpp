#include "ShaderWorldMeshAudit.h"

#include "EmptyWorldMesh.h"
#include "Log.h"
#include "Packing.h"

#include <cmath>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <vector>

namespace {

constexpr int32_t kAuditRadius = 16;
constexpr int32_t kAuditReadyRadius = 20;
constexpr uint64_t kSurfaceAuditCadenceFrames = 15u;
constexpr int32_t kTerrainWaterHeight = 30;

bool mesh_audit_enabled() {
  const char *value = std::getenv("OCTARYN_CLIENT_MESH_AUDIT");
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

int32_t floor_div_chunk(float value) {
  const int32_t block = static_cast<int32_t>(std::floor(value));
  return block >= 0 ? block / 32 : -((31 - block) / 32);
}

std::vector<int32_t> expected_surface_sections(int32_t chunk_x,
                                               int32_t chunk_z) {
  const int32_t origin_x = chunk_x * kWorldRenderSectionSize;
  const int32_t origin_z = chunk_z * kWorldRenderSectionSize;
  std::unordered_set<int32_t> sections;
  bool has_water = false;
  for (int32_t local_z = 0; local_z < kWorldRenderSectionSize; ++local_z) {
    for (int32_t local_x = 0; local_x < kWorldRenderSectionSize; ++local_x) {
      const empty_world_terrain_column column =
          empty_world_seed_column(origin_x + local_x, origin_z + local_z);
      sections.insert(floor_div_int32(column.height, kWorldRenderSectionSize));
      if (column.height < kTerrainWaterHeight) {
        has_water = true;
      }
    }
  }
  if (has_water) {
    sections.insert(
        floor_div_int32(kTerrainWaterHeight - 1, kWorldRenderSectionSize));
  }
  return {sections.begin(), sections.end()};
}

uint64_t mesh_column_key(int32_t chunk_x, int32_t chunk_z) {
  return static_cast<uint32_t>(chunk_x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(chunk_z)) << 32u);
}

uint64_t mesh_section_key(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
  uint64_t value = mesh_column_key(chunk_x, chunk_z);
  value ^= static_cast<uint64_t>(static_cast<uint32_t>(chunk_y)) +
           0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
  return value;
}

bool chunk_has_drawable_faces(
    const world_mesh_gpu_buffers::chunk_buffers &chunk) {
  return (chunk.record.opaque_face_count != 0u &&
          chunk.opaque_faces != nullptr) ||
         (chunk.record.transparent_face_count != 0u &&
          chunk.transparent_faces != nullptr) ||
         (chunk.record.sprite_index_count != 0u &&
          chunk.sprite_vertices != nullptr);
}

void add_drawn_columns(const world_mesh_gpu_buffers &mesh_buffers,
                       const std::vector<size_t> &indices,
                       std::unordered_set<uint64_t> &drawn_columns) {
  for (const size_t index : indices) {
    if (index >= mesh_buffers.chunks.size()) {
      continue;
    }
    const auto &chunk = mesh_buffers.chunks[index];
    drawn_columns.insert(
        mesh_column_key(chunk.record.chunk_x, chunk.record.chunk_z));
  }
}

void add_drawn_sections(const world_mesh_gpu_buffers &mesh_buffers,
                        const std::vector<size_t> &indices,
                        std::unordered_set<uint64_t> &drawn_sections) {
  for (const size_t index : indices) {
    if (index >= mesh_buffers.chunks.size()) {
      continue;
    }
    const auto &chunk = mesh_buffers.chunks[index];
    drawn_sections.insert(mesh_section_key(
        chunk.record.chunk_x, chunk.record.chunk_y, chunk.record.chunk_z));
  }
}

} // namespace

void audit_near_camera_mesh_columns(
    const world_mesh_gpu_buffers &mesh_buffers,
    const visible_section_draw_list &visible_sections, const camera &camera,
    uint64_t frame_index) {
  if (octaryn_client_app::g_log == nullptr) {
    return;
  }
  if (!mesh_audit_enabled()) {
    return;
  }

  static uint64_t audit_sample = 0u;
  ++audit_sample;
  const bool run_surface_audit =
      audit_sample % kSurfaceAuditCadenceFrames == 0u;
  std::unordered_set<uint64_t> retained_columns;
  retained_columns.reserve(mesh_buffers.chunks.size());
  std::unordered_set<uint64_t> retained_sections;
  retained_sections.reserve(mesh_buffers.chunks.size());
  for (const auto &chunk : mesh_buffers.chunks) {
    if (chunk_has_drawable_faces(chunk)) {
      retained_columns.insert(
          mesh_column_key(chunk.record.chunk_x, chunk.record.chunk_z));
      retained_sections.insert(mesh_section_key(
          chunk.record.chunk_x, chunk.record.chunk_y, chunk.record.chunk_z));
    }
  }

  std::unordered_set<uint64_t> drawn_columns;
  drawn_columns.reserve(visible_sections.opaque_indices.size() +
                        visible_sections.transparent_indices.size() +
                        visible_sections.sprite_indices.size());
  add_drawn_columns(mesh_buffers, visible_sections.opaque_indices,
                    drawn_columns);
  add_drawn_columns(mesh_buffers, visible_sections.transparent_indices,
                    drawn_columns);
  add_drawn_columns(mesh_buffers, visible_sections.sprite_indices,
                    drawn_columns);
  std::unordered_set<uint64_t> drawn_sections;
  drawn_sections.reserve(drawn_columns.size());
  add_drawn_sections(mesh_buffers, visible_sections.opaque_indices,
                     drawn_sections);
  add_drawn_sections(mesh_buffers, visible_sections.transparent_indices,
                     drawn_sections);
  add_drawn_sections(mesh_buffers, visible_sections.sprite_indices,
                     drawn_sections);

  const int32_t camera_chunk_x = floor_div_chunk(camera.position[0]);
  const int32_t camera_chunk_z = floor_div_chunk(camera.position[2]);
  const uint32_t checked_columns =
      static_cast<uint32_t>((kAuditRadius * 2 + 1) *
                            (kAuditRadius * 2 + 1));
  const uint32_t ready_columns =
      static_cast<uint32_t>((kAuditReadyRadius * 2 + 1) *
                            (kAuditReadyRadius * 2 + 1));
  const bool audit_ready = retained_columns.size() >= ready_columns;
  uint32_t missing = 0u;
  uint32_t retained_not_drawn = 0u;
  uint32_t loaded_without_drawable = 0u;
  uint32_t section_not_drawn = 0u;
  uint32_t surface_section_missing = 0u;
  uint32_t surface_section_not_drawn = 0u;
  int32_t first_missing_x = 0;
  int32_t first_missing_z = 0;
  int32_t first_not_drawn_x = 0;
  int32_t first_not_drawn_z = 0;
  world_render_section_key first_loaded_without_drawable{};
  world_render_section_key first_section_not_drawn{};
  world_render_section_key first_surface_missing{};
  world_render_section_key first_surface_not_drawn{};
  for (int32_t z = camera_chunk_z - kAuditRadius;
       z <= camera_chunk_z + kAuditRadius; ++z) {
    for (int32_t x = camera_chunk_x - kAuditRadius;
         x <= camera_chunk_x + kAuditRadius; ++x) {
      const uint64_t key = mesh_column_key(x, z);
      const bool retained = retained_columns.contains(key);
      if (audit_ready && !retained) {
        if (missing == 0u) {
          first_missing_x = x;
          first_missing_z = z;
        }
        ++missing;
      } else if (retained && !drawn_columns.contains(key)) {
        if (retained_not_drawn == 0u) {
          first_not_drawn_x = x;
          first_not_drawn_z = z;
        }
        ++retained_not_drawn;
      }
      if (run_surface_audit && audit_ready) {
        for (const int32_t section_y : expected_surface_sections(x, z)) {
          const uint64_t section_key = mesh_section_key(x, section_y, z);
          if (!retained_sections.contains(section_key)) {
            if (surface_section_missing == 0u) {
              first_surface_missing = {x, section_y, z};
            }
            ++surface_section_missing;
          } else if (!drawn_sections.contains(section_key)) {
            if (surface_section_not_drawn == 0u) {
              first_surface_not_drawn = {x, section_y, z};
            }
            ++surface_section_not_drawn;
          }
        }
      }
    }
  }

  if (run_surface_audit) {
    for (const world_render_section_state &section : mesh_buffers.sections) {
      if ((section.flags & kWorldRenderSectionLoaded) == 0u ||
          (section.flags & kWorldRenderSectionEmpty) != 0u ||
          (section.flags & kWorldRenderSectionSolid) != 0u ||
          std::abs(section.key.x - camera_chunk_x) > kAuditRadius ||
          std::abs(section.key.z - camera_chunk_z) > kAuditRadius) {
        continue;
      }
      const uint64_t key =
          mesh_section_key(section.key.x, section.key.y, section.key.z);
      if (!retained_sections.contains(key)) {
        if (loaded_without_drawable == 0u) {
          first_loaded_without_drawable = section.key;
        }
        ++loaded_without_drawable;
      } else if (!drawn_sections.contains(key)) {
        if (section_not_drawn == 0u) {
          first_section_not_drawn = section.key;
        }
        ++section_not_drawn;
      }
    }
  }

  if (missing == 0u && retained_not_drawn == 0u &&
      loaded_without_drawable == 0u && section_not_drawn == 0u &&
      surface_section_missing == 0u && surface_section_not_drawn == 0u &&
      audit_sample % 60u != 0u) {
    return;
  }

  std::fprintf(octaryn_client_app::g_log,
               "live_world_mesh_column_audit active=1 frame=%" PRIu64
               " sample=%" PRIu64
               " camera_chunk=(%d,%d) radius=%d retained_columns=%zu"
               " drawn_columns=%zu retained_chunks=%zu checked=%u"
               " missing=%u first_missing=(%d,%d)"
               " retained_not_drawn=%u first_not_drawn=(%d,%d)"
               " loaded_without_drawable=%u"
               " first_loaded_without_drawable=(%d,%d,%d)"
               " section_not_drawn=%u first_section_not_drawn=(%d,%d,%d)"
               " surface_section_missing=%u"
               " first_surface_missing=(%d,%d,%d)"
               " surface_section_not_drawn=%u"
               " first_surface_not_drawn=(%d,%d,%d)\n",
               frame_index, audit_sample, camera_chunk_x, camera_chunk_z,
               kAuditRadius, retained_columns.size(), drawn_columns.size(),
               mesh_buffers.chunks.size(), checked_columns,
               missing, first_missing_x, first_missing_z, retained_not_drawn,
               first_not_drawn_x, first_not_drawn_z, loaded_without_drawable,
               first_loaded_without_drawable.x, first_loaded_without_drawable.y,
               first_loaded_without_drawable.z, section_not_drawn,
               first_section_not_drawn.x, first_section_not_drawn.y,
               first_section_not_drawn.z, surface_section_missing,
               first_surface_missing.x, first_surface_missing.y,
               first_surface_missing.z, surface_section_not_drawn,
               first_surface_not_drawn.x, first_surface_not_drawn.y,
               first_surface_not_drawn.z);
  std::fflush(octaryn_client_app::g_log);
}
