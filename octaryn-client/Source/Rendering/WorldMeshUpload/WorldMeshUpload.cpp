#include "WorldMeshUpload.h"

#include "FrameProfile.h"
#include "FunctionProfile.h"
#include "Log.h"
#include "WorldMeshTransfer.h"

#include <cinttypes>
#include <cstdio>
#include <limits>

namespace {

bool same_chunk_mesh(const octaryn_client_chunk_mesh_upload_record &left,
                     const octaryn_client_chunk_mesh_upload_record &right) {
  return left.chunk_x == right.chunk_x && left.chunk_y == right.chunk_y &&
         left.chunk_z == right.chunk_z;
}

bool chunk_mesh_has_geometry(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return chunk.opaque_face_count != 0u || chunk.transparent_face_count != 0u ||
         chunk.sprite_vertex_count != 0u;
}

void add_gpu_totals(world_mesh_gpu_buffers &buffers,
                    const octaryn_client_chunk_mesh_upload_record &chunk) {
  buffers.opaque_faces += chunk.opaque_face_count;
  buffers.transparent_faces += chunk.transparent_face_count;
  buffers.sprite_vertices += chunk.sprite_vertex_count;
  buffers.opaque_bytes += chunk.opaque_byte_count;
  buffers.transparent_bytes += chunk.transparent_byte_count;
  buffers.sprite_bytes += chunk.sprite_byte_count;
}

void subtract_gpu_totals(world_mesh_gpu_buffers &buffers,
                         const octaryn_client_chunk_mesh_upload_record &chunk) {
  buffers.opaque_faces =
      buffers.opaque_faces >= chunk.opaque_face_count
          ? buffers.opaque_faces - chunk.opaque_face_count
          : 0u;
  buffers.transparent_faces =
      buffers.transparent_faces >= chunk.transparent_face_count
          ? buffers.transparent_faces - chunk.transparent_face_count
          : 0u;
  buffers.sprite_vertices =
      buffers.sprite_vertices >= chunk.sprite_vertex_count
          ? buffers.sprite_vertices - chunk.sprite_vertex_count
          : 0u;
  buffers.opaque_bytes =
      buffers.opaque_bytes >= chunk.opaque_byte_count
          ? buffers.opaque_bytes - chunk.opaque_byte_count
          : 0u;
  buffers.transparent_bytes =
      buffers.transparent_bytes >= chunk.transparent_byte_count
          ? buffers.transparent_bytes - chunk.transparent_byte_count
          : 0u;
  buffers.sprite_bytes =
      buffers.sprite_bytes >= chunk.sprite_byte_count
          ? buffers.sprite_bytes - chunk.sprite_byte_count
          : 0u;
}

world_render_section_key key_from_chunk(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return {chunk.chunk_x, chunk.chunk_y, chunk.chunk_z};
}

bool update_replaces_chunk_with_geometry(
    const world_mesh_upload_frame &update,
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  for (const octaryn_client_chunk_mesh_upload_record &update_chunk :
       update.chunks) {
    if (chunk_mesh_has_geometry(update_chunk) &&
        same_chunk_mesh(update_chunk, chunk)) {
      return true;
    }
  }
  return false;
}

void release_buffer(SDL_GPUDevice *device, SDL_GPUBuffer *&buffer) {
  if (buffer == nullptr) {
    return;
  }
  SDL_ReleaseGPUBuffer(device, buffer);
  buffer = nullptr;
}

void release_chunk_buffers(SDL_GPUDevice *device,
                           world_mesh_gpu_buffers::chunk_buffers &chunk) {
  release_buffer(device, chunk.opaque_faces);
  release_buffer(device, chunk.opaque_indirect);
  release_buffer(device, chunk.transparent_faces);
  release_buffer(device, chunk.sprite_vertices);
  release_buffer(device, chunk.sprite_indirect);
  chunk.opaque_capacity = 0u;
  chunk.opaque_indirect_capacity = 0u;
  chunk.transparent_capacity = 0u;
  chunk.sprite_capacity = 0u;
  chunk.sprite_indirect_capacity = 0u;
}

size_t find_chunk_buffer_index(
    const world_mesh_gpu_buffers &buffers,
    const octaryn_client_chunk_mesh_upload_record &update) {
  const auto found = buffers.chunk_indices.find(key_from_chunk(update));
  if (found == buffers.chunk_indices.end()) {
    return buffers.chunks.size();
  }
  return found->second < buffers.chunks.size() ? found->second
                                               : buffers.chunks.size();
}

void remove_chunk_buffer(SDL_GPUDevice *device, world_mesh_gpu_buffers &buffers,
                         size_t index) {
  if (index >= buffers.chunks.size()) {
    return;
  }
  const world_render_section_key removed_key =
      key_from_chunk(buffers.chunks[index].record);
  subtract_gpu_totals(buffers, buffers.chunks[index].record);
  release_chunk_buffers(device, buffers.chunks[index]);
  buffers.chunk_indices.erase(removed_key);
  const size_t last_index = buffers.chunks.size() - 1u;
  if (index != last_index) {
    buffers.chunks[index] = buffers.chunks[last_index];
    buffers.chunk_indices[key_from_chunk(buffers.chunks[index].record)] = index;
  }
  buffers.chunks.pop_back();
}

bool ensure_gpu_buffer(SDL_GPUDevice *device, SDL_GPUBuffer *&buffer,
                       uint64_t &capacity, uint64_t byte_count,
                       SDL_GPUBufferUsageFlags usage) {
  if (byte_count == 0u) {
    return true;
  }
  if (byte_count > std::numeric_limits<Uint32>::max()) {
    octaryn_client_app::log_line("gpu_chunk_mesh_upload=too_large");
    return false;
  }
  if (buffer != nullptr && capacity >= byte_count) {
    return true;
  }

  SDL_GPUBufferCreateInfo buffer_info{};
  buffer_info.usage = usage;
  buffer_info.size = static_cast<Uint32>(byte_count);
  SDL_GPUBuffer *next = SDL_CreateGPUBuffer(device, &buffer_info);
  if (next == nullptr) {
    octaryn_client_app::log_line("gpu_chunk_mesh_buffer=create_failed");
    return false;
  }
  release_buffer(device, buffer);
  buffer = next;
  capacity = byte_count;
  return true;
}

} // namespace

void release_world_mesh_gpu_buffers(SDL_GPUDevice *device,
                                    world_mesh_gpu_buffers &buffers) {
  for (world_mesh_gpu_buffers::chunk_buffers &chunk : buffers.chunks) {
    release_chunk_buffers(device, chunk);
  }
  buffers = {};
}

bool world_mesh_gpu_has_geometry(const world_mesh_gpu_buffers &buffers) {
  return buffers.opaque_faces != 0u || buffers.transparent_faces != 0u ||
         buffers.sprite_vertices != 0u;
}

bool apply_world_mesh_upload_update(SDL_GPUDevice *gpu_device,
                                    world_mesh_upload_frame &visible_frame,
                                    const world_mesh_upload_frame &update_frame,
                                    world_mesh_gpu_buffers &mesh_buffers,
                                    uint64_t frame_index, const char *source,
                                    int &result) {
  if (update_frame.chunks.empty()) {
    if (!update_frame.sections.empty()) {
      merge_world_mesh_upload_frame(visible_frame, update_frame, frame_index,
                                    source);
      apply_world_mesh_draw_index_update(mesh_buffers, update_frame);
    }
    return true;
  }

  function_profile_scope upload_profile_scope("world_mesh_upload", frame_index,
                                              source);
  merge_world_mesh_upload_frame(visible_frame, update_frame, frame_index,
                                source);
  if (!upload_world_mesh_frame(gpu_device, update_frame, mesh_buffers,
                               frame_index)) {
    result = -6;
    return false;
  }
  apply_world_mesh_draw_index_update(mesh_buffers, update_frame);
  return true;
}

bool upload_world_mesh_frame(SDL_GPUDevice *device,
                             const world_mesh_upload_frame &upload_frame,
                             world_mesh_gpu_buffers &buffers,
                             uint64_t frame_index) {
  if (upload_frame.chunks.empty()) {
    return true;
  }

  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (command_buffer == nullptr) {
    octaryn_client_app::log_line("gpu_chunk_mesh_command=create_failed");
    return false;
  }
  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
  if (copy_pass == nullptr) {
    SDL_CancelGPUCommandBuffer(command_buffer);
    octaryn_client_app::log_line("gpu_chunk_mesh_copy_pass=create_failed");
    return false;
  }

  std::vector<SDL_GPUTransferBuffer *> transfers;
  std::vector<SDL_GPUIndirectDrawCommand> opaque_draws;
  std::vector<SDL_GPUIndirectDrawCommand> sprite_draws;
  opaque_draws.reserve(upload_frame.chunks.size());
  sprite_draws.reserve(upload_frame.chunks.size());
  for (const octaryn_client_chunk_mesh_upload_record &update :
       upload_frame.chunks) {
    if (!chunk_mesh_has_geometry(update)) {
      continue;
    }
    opaque_draws.push_back(
        SDL_GPUIndirectDrawCommand{update.opaque_face_count * 6u, 1u, 0u, 0u});
    if (update.sprite_byte_count != 0u) {
      sprite_draws.push_back(
          SDL_GPUIndirectDrawCommand{update.sprite_index_count, 1u, 0u, 0u});
    }
  }

  SDL_GPUTransferBuffer *opaque_transfer = nullptr;
  SDL_GPUTransferBuffer *transparent_transfer = nullptr;
  SDL_GPUTransferBuffer *sprite_transfer = nullptr;
  SDL_GPUTransferBuffer *opaque_indirect_transfer = nullptr;
  SDL_GPUTransferBuffer *sprite_indirect_transfer = nullptr;
  const uint64_t transfer_start = SDL_GetTicksNS();
  const uint64_t opaque_transfer_bytes =
      static_cast<uint64_t>(upload_frame.opaque_faces.size()) *
      sizeof(uint64_t);
  const uint64_t transparent_transfer_bytes =
      static_cast<uint64_t>(upload_frame.transparent_faces.size()) *
      sizeof(uint64_t);
  const uint64_t sprite_transfer_bytes =
      static_cast<uint64_t>(upload_frame.sprite_vertices.size()) *
      sizeof(uint32_t);
  const uint64_t opaque_indirect_bytes =
      static_cast<uint64_t>(opaque_draws.size()) *
      sizeof(SDL_GPUIndirectDrawCommand);
  const uint64_t sprite_indirect_bytes =
      static_cast<uint64_t>(sprite_draws.size()) *
      sizeof(SDL_GPUIndirectDrawCommand);
  if (!octaryn_client_app::create_world_mesh_transfer(
          device, transfers, upload_frame.opaque_faces.data(),
          opaque_transfer_bytes, opaque_transfer) ||
      !octaryn_client_app::create_world_mesh_transfer(
          device, transfers, upload_frame.sprite_vertices.data(),
          sprite_transfer_bytes, sprite_transfer) ||
      !octaryn_client_app::create_world_mesh_transfer(
          device, transfers, upload_frame.transparent_faces.data(),
          transparent_transfer_bytes, transparent_transfer) ||
      !octaryn_client_app::create_world_mesh_transfer(
          device, transfers, opaque_draws.data(), opaque_indirect_bytes,
          opaque_indirect_transfer) ||
      !octaryn_client_app::create_world_mesh_transfer(
          device, transfers, sprite_draws.data(), sprite_indirect_bytes,
          sprite_indirect_transfer)) {
    SDL_EndGPUCopyPass(copy_pass);
    SDL_CancelGPUCommandBuffer(command_buffer);
    for (SDL_GPUTransferBuffer *transfer : transfers) {
      SDL_ReleaseGPUTransferBuffer(device, transfer);
    }
    return false;
  }
  const float transfer_ms =
      frame_profile_elapsed_ms_since(transfer_start);

  const uint64_t queue_start = SDL_GetTicksNS();
  uint32_t updated_chunks = 0u;
  uint32_t removed_chunks = 0u;
  uint32_t opaque_indirect_index = 0u;
  uint32_t sprite_indirect_index = 0u;
  bool ok = true;
  for (const octaryn_client_chunk_mesh_upload_record &update :
       upload_frame.chunks) {
    size_t chunk_index = find_chunk_buffer_index(buffers, update);
    if (!chunk_mesh_has_geometry(update)) {
      if (update_replaces_chunk_with_geometry(upload_frame, update)) {
        continue;
      }
      if (chunk_index < buffers.chunks.size()) {
        remove_chunk_buffer(device, buffers, chunk_index);
        ++removed_chunks;
      }
      continue;
    }

    if (chunk_index >= buffers.chunks.size()) {
      const size_t next_index = buffers.chunks.size();
      buffers.chunks.push_back({});
      buffers.chunk_indices[key_from_chunk(update)] = next_index;
      chunk_index = next_index;
    }
    world_mesh_gpu_buffers::chunk_buffers &chunk = buffers.chunks[chunk_index];
    subtract_gpu_totals(buffers, chunk.record);
    chunk.record = update;
    chunk.record.opaque_face_offset = 0u;
    chunk.record.transparent_face_offset = 0u;
    chunk.record.sprite_vertex_offset = 0u;
    add_gpu_totals(buffers, chunk.record);

    ok = ensure_gpu_buffer(device, chunk.opaque_faces, chunk.opaque_capacity,
                           update.opaque_byte_count,
                           SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ) &&
         ensure_gpu_buffer(device, chunk.transparent_faces,
                           chunk.transparent_capacity,
                           update.transparent_byte_count,
                           SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ) &&
         ensure_gpu_buffer(device, chunk.opaque_indirect,
                           chunk.opaque_indirect_capacity,
                           sizeof(SDL_GPUIndirectDrawCommand),
                           SDL_GPU_BUFFERUSAGE_INDIRECT);
    if (!ok) {
      break;
    }
    octaryn_client_app::queue_world_mesh_transfer_upload(
        copy_pass, opaque_transfer, update.opaque_face_offset * sizeof(uint64_t),
        chunk.opaque_faces, update.opaque_byte_count, true);
    octaryn_client_app::queue_world_mesh_transfer_upload(
        copy_pass, opaque_indirect_transfer,
        static_cast<uint64_t>(opaque_indirect_index++) *
            sizeof(SDL_GPUIndirectDrawCommand),
        chunk.opaque_indirect, sizeof(SDL_GPUIndirectDrawCommand), true);
    if (update.transparent_byte_count != 0u) {
      octaryn_client_app::queue_world_mesh_transfer_upload(
          copy_pass, transparent_transfer,
          update.transparent_face_offset * sizeof(uint64_t),
          chunk.transparent_faces, update.transparent_byte_count, true);
    } else {
      release_buffer(device, chunk.transparent_faces);
      chunk.transparent_capacity = 0u;
    }

    if (update.sprite_byte_count != 0u) {
      ok = ensure_gpu_buffer(device, chunk.sprite_vertices,
                             chunk.sprite_capacity, update.sprite_byte_count,
                             SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ) &&
           ensure_gpu_buffer(device, chunk.sprite_indirect,
                             chunk.sprite_indirect_capacity,
                             sizeof(SDL_GPUIndirectDrawCommand),
                             SDL_GPU_BUFFERUSAGE_INDIRECT);
      if (!ok) {
        break;
      }
      octaryn_client_app::queue_world_mesh_transfer_upload(
          copy_pass, sprite_transfer,
          update.sprite_vertex_offset * sizeof(uint32_t),
          chunk.sprite_vertices, update.sprite_byte_count, true);
      octaryn_client_app::queue_world_mesh_transfer_upload(
          copy_pass, sprite_indirect_transfer,
          static_cast<uint64_t>(sprite_indirect_index++) *
              sizeof(SDL_GPUIndirectDrawCommand),
          chunk.sprite_indirect, sizeof(SDL_GPUIndirectDrawCommand), true);
    } else {
      release_buffer(device, chunk.sprite_vertices);
      release_buffer(device, chunk.sprite_indirect);
      chunk.sprite_capacity = 0u;
      chunk.sprite_indirect_capacity = 0u;
    }
    ++updated_chunks;
  }
  const float queue_ms =
      frame_profile_elapsed_ms_since(queue_start);

  SDL_EndGPUCopyPass(copy_pass);
  const uint64_t submit_start = SDL_GetTicksNS();
  if (!ok || !SDL_SubmitGPUCommandBuffer(command_buffer)) {
    for (SDL_GPUTransferBuffer *transfer : transfers) {
      SDL_ReleaseGPUTransferBuffer(device, transfer);
    }
    octaryn_client_app::log_line("gpu_chunk_mesh_upload=failed");
    return false;
  }
  const float submit_ms =
      frame_profile_elapsed_ms_since(submit_start);
  const uint64_t release_start = SDL_GetTicksNS();
  for (SDL_GPUTransferBuffer *transfer : transfers) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
  }
  const float release_ms =
      frame_profile_elapsed_ms_since(release_start);

  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_upload frame=%" PRIu64
                 " active=1 target=sdl_gpu_direct_indirect updates=%" PRIu32
                 " removed=%" PRIu32 " chunks=%zu opaque_bytes=%" PRIu64
                 " transparent_bytes=%" PRIu64 " sprite_bytes=%" PRIu64
                 " fluid_blocks=%" PRIu32 "\n",
                 frame_index, updated_chunks, removed_chunks,
                 buffers.chunks.size(), buffers.opaque_bytes,
                 buffers.transparent_bytes, buffers.sprite_bytes,
                 upload_frame.fluid_blocks);
    std::fflush(octaryn_client_app::g_log);
  }
  const float total_ms = transfer_ms + queue_ms + submit_ms + release_ms;
  if (octaryn_client_app::g_log != nullptr && total_ms >= 2.0f) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_upload_profile frame=%" PRIu64
                 " total_ms=%.3f transfer_ms=%.3f queue_ms=%.3f"
                 " submit_ms=%.3f release_ms=%.3f"
                 " update_chunks=%" PRIu32 " retained_chunks=%zu"
                 " transfer_bytes=%" PRIu64 " indirect_bytes=%" PRIu64 "\n",
                 frame_index, total_ms, transfer_ms, queue_ms, submit_ms,
                 release_ms, updated_chunks, buffers.chunks.size(),
                 opaque_transfer_bytes + sprite_transfer_bytes,
                 opaque_indirect_bytes + sprite_indirect_bytes);
    std::fflush(octaryn_client_app::g_log);
  }
  return true;
}
