#include "WorldMeshUpload.h"

#include "Log.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
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

bool same_chunk_mesh(
    const world_mesh_gpu_buffers::chunk_buffers &left,
    const octaryn_client_chunk_mesh_upload_record &right) {
  return left.record.chunk_x == right.chunk_x &&
         left.record.chunk_y == right.chunk_y &&
         left.record.chunk_z == right.chunk_z;
}

bool chunk_mesh_update_contains(
    const world_mesh_upload_frame &update,
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  for (const octaryn_client_chunk_mesh_upload_record &update_chunk :
       update.chunks) {
    if (same_chunk_mesh(update_chunk, chunk)) {
      return true;
    }
  }
  return false;
}

void append_chunk_record(world_mesh_upload_frame &destination,
                         const octaryn_client_chunk_mesh_upload_record &chunk) {
  octaryn_client_chunk_mesh_upload_record copied = chunk;
  copied.opaque_face_offset = 0u;
  copied.transparent_face_offset = 0u;
  copied.sprite_vertex_offset = 0u;
  copied.opaque_byte_count =
      static_cast<uint64_t>(copied.opaque_face_count) * sizeof(uint64_t);
  copied.transparent_byte_count =
      static_cast<uint64_t>(copied.transparent_face_count) * sizeof(uint64_t);
  copied.sprite_byte_count =
      static_cast<uint64_t>(copied.sprite_vertex_count) * sizeof(uint32_t);
  destination.opaque_bytes += copied.opaque_byte_count;
  destination.transparent_bytes += copied.transparent_byte_count;
  destination.sprite_bytes += copied.sprite_byte_count;
  destination.fluid_blocks += copied.fluid_block_count;
  destination.chunks.push_back(copied);
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

bool stage_gpu_buffer_upload(SDL_GPUDevice *device, SDL_GPUCopyPass *copy_pass,
                             std::vector<SDL_GPUTransferBuffer *> &transfers,
                             SDL_GPUBuffer *target, const void *data,
                             uint64_t byte_count, bool cycle) {
  if (byte_count == 0u) {
    return true;
  }
  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = static_cast<Uint32>(byte_count);
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr) {
    octaryn_client_app::log_line("gpu_chunk_mesh_transfer=create_failed");
    return false;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    octaryn_client_app::log_line("gpu_chunk_mesh_transfer=map_failed");
    return false;
  }
  std::memcpy(mapped, data, static_cast<size_t>(byte_count));
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUTransferBufferLocation source{};
  source.transfer_buffer = transfer;
  SDL_GPUBufferRegion destination{};
  destination.buffer = target;
  destination.size = static_cast<Uint32>(byte_count);
  SDL_UploadToGPUBuffer(copy_pass, &source, &destination, cycle);
  transfers.push_back(transfer);
  return true;
}

} // namespace

bool drain_chunk_mesh_uploads(uint64_t frame_index,
                              world_mesh_upload_scratch &scratch,
                              world_mesh_upload_frame &upload_frame) {
  uint32_t upload_written = 0u;
  uint32_t opaque_faces_written = 0u;
  uint32_t transparent_faces_written = 0u;
  uint32_t sprite_vertices_written = 0u;
  const int result = octaryn_client_drain_chunk_mesh_uploads(
      scratch.chunks.data(), static_cast<uint32_t>(scratch.chunks.size()),
      &upload_written, scratch.opaque_faces.data(),
      static_cast<uint32_t>(scratch.opaque_faces.size()),
      &opaque_faces_written, scratch.transparent_faces.data(),
      static_cast<uint32_t>(scratch.transparent_faces.size()),
      &transparent_faces_written, scratch.sprite_vertices.data(),
      static_cast<uint32_t>(scratch.sprite_vertices.size()),
      &sprite_vertices_written);
  if (result != 0) {
    octaryn_client_app::log_result("drain_chunk_mesh_uploads", result);
    return false;
  }

  upload_frame.chunks.assign(scratch.chunks.begin(),
                             scratch.chunks.begin() + upload_written);
  upload_frame.opaque_faces.assign(scratch.opaque_faces.begin(),
                                   scratch.opaque_faces.begin() +
                                       opaque_faces_written);
  upload_frame.transparent_faces.assign(
      scratch.transparent_faces.begin(),
      scratch.transparent_faces.begin() + transparent_faces_written);
  upload_frame.sprite_vertices.assign(scratch.sprite_vertices.begin(),
                                      scratch.sprite_vertices.begin() +
                                          sprite_vertices_written);
  upload_frame.fluid_blocks = 0u;
  upload_frame.opaque_bytes = 0u;
  upload_frame.transparent_bytes = 0u;
  upload_frame.sprite_bytes = 0u;
  uint32_t sprite_indices = 0u;

  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       upload_frame.chunks) {
    upload_frame.fluid_blocks += chunk.fluid_block_count;
    upload_frame.opaque_bytes += chunk.opaque_byte_count;
    upload_frame.transparent_bytes += chunk.transparent_byte_count;
    upload_frame.sprite_bytes += chunk.sprite_byte_count;
    sprite_indices += chunk.sprite_index_count;
  }

  if (upload_written != 0u && octaryn_client_app::g_log != nullptr) {
    std::fprintf(
        octaryn_client_app::g_log,
        "live_chunk_mesh_drain frame=%" PRIu64
        " active=1 chunks=%" PRIu32 " opaque_faces=%" PRIu32
        " transparent_faces=%" PRIu32 " sprite_vertices=%" PRIu32
        " sprite_indices=%" PRIu32 " fluid_blocks=%" PRIu32 "\n",
        frame_index, upload_written, opaque_faces_written,
        transparent_faces_written, sprite_vertices_written, sprite_indices,
        upload_frame.fluid_blocks);
    std::fflush(octaryn_client_app::g_log);
  }
  return true;
}

void merge_world_mesh_upload_frame(world_mesh_upload_frame &visible_frame,
                                   const world_mesh_upload_frame &update_frame,
                                   uint64_t frame_index, const char *source) {
  if (update_frame.chunks.empty()) {
    return;
  }

  uint32_t sprite_indices = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       update_frame.chunks) {
    sprite_indices += chunk.sprite_index_count;
  }
  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_drain frame=%" PRIu64
                 " active=1 source=%s chunks=%zu opaque_faces=%zu"
                 " transparent_faces=%zu sprite_vertices=%zu"
                 " sprite_indices=%" PRIu32 " fluid_blocks=%" PRIu32 "\n",
                 frame_index, source, update_frame.chunks.size(),
                 update_frame.opaque_faces.size(),
                 update_frame.transparent_faces.size(),
                 update_frame.sprite_vertices.size(), sprite_indices,
                 update_frame.fluid_blocks);
    std::fflush(octaryn_client_app::g_log);
  }

  world_mesh_upload_frame merged{};
  merged.chunks.reserve(visible_frame.chunks.size() + update_frame.chunks.size());
  merged.opaque_faces.reserve(visible_frame.opaque_faces.size() +
                              update_frame.opaque_faces.size());
  merged.transparent_faces.reserve(visible_frame.transparent_faces.size() +
                                   update_frame.transparent_faces.size());
  merged.sprite_vertices.reserve(visible_frame.sprite_vertices.size() +
                                 update_frame.sprite_vertices.size());

  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       visible_frame.chunks) {
    if (!chunk_mesh_update_contains(update_frame, chunk)) {
      append_chunk_record(merged, chunk);
    }
  }

  uint32_t removed_chunks = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       update_frame.chunks) {
    if (chunk_mesh_has_geometry(chunk)) {
      append_chunk_record(merged, chunk);
    } else {
      ++removed_chunks;
    }
  }

  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_retained frame=%" PRIu64
                 " active=1 updates=%zu retained=%zu removed=%" PRIu32
                 " visible_chunks=%zu opaque_faces=%zu transparent_faces=%zu"
                 " sprite_vertices=%zu\n",
                 frame_index, update_frame.chunks.size(),
                 visible_frame.chunks.size(), removed_chunks,
                 merged.chunks.size(), merged.opaque_faces.size(),
                 merged.transparent_faces.size(), merged.sprite_vertices.size());
    std::fflush(octaryn_client_app::g_log);
  }

  visible_frame = std::move(merged);
}

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
  uint32_t updated_chunks = 0u;
  uint32_t removed_chunks = 0u;
  bool ok = true;
  for (const octaryn_client_chunk_mesh_upload_record &update :
       upload_frame.chunks) {
    auto found = std::find_if(buffers.chunks.begin(), buffers.chunks.end(),
                              [&](const auto &chunk) {
                                return same_chunk_mesh(chunk, update);
                              });
    if (!chunk_mesh_has_geometry(update)) {
      if (found != buffers.chunks.end()) {
        release_chunk_buffers(device, *found);
        buffers.chunks.erase(found);
        ++removed_chunks;
      }
      continue;
    }

    if (found == buffers.chunks.end()) {
      buffers.chunks.push_back({});
      found = buffers.chunks.end() - 1;
    }
    world_mesh_gpu_buffers::chunk_buffers &chunk = *found;
    chunk.record = update;
    chunk.record.opaque_face_offset = 0u;
    chunk.record.transparent_face_offset = 0u;
    chunk.record.sprite_vertex_offset = 0u;

    const void *opaque_source =
        update.opaque_byte_count == 0u
            ? nullptr
            : upload_frame.opaque_faces.data() +
                  static_cast<std::ptrdiff_t>(update.opaque_face_offset);
    const void *sprite_source =
        update.sprite_byte_count == 0u
            ? nullptr
            : upload_frame.sprite_vertices.data() +
                  static_cast<std::ptrdiff_t>(update.sprite_vertex_offset);
    const SDL_GPUIndirectDrawCommand opaque_draw{
        update.opaque_face_count * 6u, 1u, 0u, 0u};
    const SDL_GPUIndirectDrawCommand sprite_draw{
        update.sprite_index_count, 1u, 0u, 0u};

    ok = ensure_gpu_buffer(device, chunk.opaque_faces, chunk.opaque_capacity,
                           update.opaque_byte_count,
                           SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ) &&
         ensure_gpu_buffer(device, chunk.opaque_indirect,
                           chunk.opaque_indirect_capacity,
                           sizeof(opaque_draw),
                           SDL_GPU_BUFFERUSAGE_INDIRECT) &&
         stage_gpu_buffer_upload(device, copy_pass, transfers,
                                 chunk.opaque_faces, opaque_source,
                                 update.opaque_byte_count, true) &&
         stage_gpu_buffer_upload(device, copy_pass, transfers,
                                 chunk.opaque_indirect, &opaque_draw,
                                 sizeof(opaque_draw), true);
    if (!ok) {
      break;
    }

    if (update.sprite_byte_count != 0u) {
      ok = ensure_gpu_buffer(device, chunk.sprite_vertices,
                             chunk.sprite_capacity, update.sprite_byte_count,
                             SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ) &&
           ensure_gpu_buffer(device, chunk.sprite_indirect,
                             chunk.sprite_indirect_capacity,
                             sizeof(sprite_draw),
                             SDL_GPU_BUFFERUSAGE_INDIRECT) &&
           stage_gpu_buffer_upload(device, copy_pass, transfers,
                                   chunk.sprite_vertices, sprite_source,
                                   update.sprite_byte_count, true) &&
           stage_gpu_buffer_upload(device, copy_pass, transfers,
                                   chunk.sprite_indirect, &sprite_draw,
                                   sizeof(sprite_draw), true);
      if (!ok) {
        break;
      }
    } else {
      release_buffer(device, chunk.sprite_vertices);
      release_buffer(device, chunk.sprite_indirect);
      chunk.sprite_capacity = 0u;
      chunk.sprite_indirect_capacity = 0u;
    }
    ++updated_chunks;
  }

  SDL_EndGPUCopyPass(copy_pass);
  if (!ok || !SDL_SubmitGPUCommandBuffer(command_buffer)) {
    for (SDL_GPUTransferBuffer *transfer : transfers) {
      SDL_ReleaseGPUTransferBuffer(device, transfer);
    }
    octaryn_client_app::log_line("gpu_chunk_mesh_upload=failed");
    return false;
  }
  for (SDL_GPUTransferBuffer *transfer : transfers) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
  }

  buffers.opaque_faces = 0u;
  buffers.transparent_faces = 0u;
  buffers.sprite_vertices = 0u;
  buffers.opaque_bytes = 0u;
  buffers.transparent_bytes = 0u;
  buffers.sprite_bytes = 0u;
  for (const world_mesh_gpu_buffers::chunk_buffers &chunk : buffers.chunks) {
    buffers.opaque_faces += chunk.record.opaque_face_count;
    buffers.transparent_faces += chunk.record.transparent_face_count;
    buffers.sprite_vertices += chunk.record.sprite_vertex_count;
    buffers.opaque_bytes += chunk.record.opaque_byte_count;
    buffers.transparent_bytes += chunk.record.transparent_byte_count;
    buffers.sprite_bytes += chunk.record.sprite_byte_count;
  }

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
  return true;
}
