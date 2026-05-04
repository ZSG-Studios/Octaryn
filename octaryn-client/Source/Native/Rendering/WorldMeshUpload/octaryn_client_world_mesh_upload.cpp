#include "octaryn_client_world_mesh_upload.h"

#include "octaryn_client_app_log.h"

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

void append_chunk_mesh(world_mesh_upload_frame &destination,
                       const world_mesh_upload_frame &source,
                       const octaryn_client_chunk_mesh_upload_record &chunk) {
  octaryn_client_chunk_mesh_upload_record copied = chunk;
  copied.opaque_face_offset = destination.opaque_faces.size();
  copied.transparent_face_offset = destination.transparent_faces.size();
  copied.sprite_vertex_offset = destination.sprite_vertices.size();

  destination.opaque_faces.insert(
      destination.opaque_faces.end(),
      source.opaque_faces.begin() +
          static_cast<std::ptrdiff_t>(chunk.opaque_face_offset),
      source.opaque_faces.begin() +
          static_cast<std::ptrdiff_t>(chunk.opaque_face_offset +
                                      chunk.opaque_face_count));
  destination.transparent_faces.insert(
      destination.transparent_faces.end(),
      source.transparent_faces.begin() +
          static_cast<std::ptrdiff_t>(chunk.transparent_face_offset),
      source.transparent_faces.begin() +
          static_cast<std::ptrdiff_t>(chunk.transparent_face_offset +
                                      chunk.transparent_face_count));
  destination.sprite_vertices.insert(
      destination.sprite_vertices.end(),
      source.sprite_vertices.begin() +
          static_cast<std::ptrdiff_t>(chunk.sprite_vertex_offset),
      source.sprite_vertices.begin() +
          static_cast<std::ptrdiff_t>(chunk.sprite_vertex_offset +
                                      chunk.sprite_vertex_count));

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

bool upload_gpu_buffer(SDL_GPUDevice *device, const void *data,
                       uint64_t byte_count, SDL_GPUBufferUsageFlags usage,
                       const char *log_prefix, SDL_GPUBuffer *&target) {
  if (target != nullptr) {
    SDL_ReleaseGPUBuffer(device, target);
    target = nullptr;
  }
  if (byte_count == 0u) {
    return true;
  }
  if (byte_count > std::numeric_limits<Uint32>::max()) {
    octaryn_client_app::log_line("gpu_chunk_mesh_upload=too_large");
    return false;
  }

  SDL_GPUBufferCreateInfo buffer_info{};
  buffer_info.usage = usage;
  buffer_info.size = static_cast<Uint32>(byte_count);
  SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buffer_info);
  if (buffer == nullptr) {
    if (octaryn_client_app::g_log != nullptr) {
      std::fprintf(octaryn_client_app::g_log, "%s_buffer=create_failed\n",
                   log_prefix);
      std::fflush(octaryn_client_app::g_log);
    }
    return false;
  }

  SDL_GPUTransferBufferCreateInfo transfer_info{};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = static_cast<Uint32>(byte_count);
  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (transfer == nullptr) {
    SDL_ReleaseGPUBuffer(device, buffer);
    if (octaryn_client_app::g_log != nullptr) {
      std::fprintf(octaryn_client_app::g_log, "%s_transfer=create_failed\n",
                   log_prefix);
      std::fflush(octaryn_client_app::g_log);
    }
    return false;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    if (octaryn_client_app::g_log != nullptr) {
      std::fprintf(octaryn_client_app::g_log, "%s_transfer=map_failed\n",
                   log_prefix);
      std::fflush(octaryn_client_app::g_log);
    }
    return false;
  }
  std::memcpy(mapped, data, static_cast<size_t>(byte_count));
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
  if (command_buffer == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    if (octaryn_client_app::g_log != nullptr) {
      std::fprintf(octaryn_client_app::g_log, "%s_command=create_failed\n",
                   log_prefix);
      std::fflush(octaryn_client_app::g_log);
    }
    return false;
  }

  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
  SDL_GPUTransferBufferLocation source{};
  source.transfer_buffer = transfer;
  SDL_GPUBufferRegion destination{};
  destination.buffer = buffer;
  destination.size = static_cast<Uint32>(byte_count);
  SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);
  SDL_EndGPUCopyPass(copy_pass);

  const bool submitted = SDL_SubmitGPUCommandBuffer(command_buffer);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  if (!submitted || !SDL_WaitForGPUIdle(device)) {
    SDL_ReleaseGPUBuffer(device, buffer);
    if (octaryn_client_app::g_log != nullptr) {
      std::fprintf(octaryn_client_app::g_log, "%s_upload=failed\n", log_prefix);
      std::fflush(octaryn_client_app::g_log);
    }
    return false;
  }

  target = buffer;
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
                                   uint64_t frame_index) {
  if (update_frame.chunks.empty()) {
    return;
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
      append_chunk_mesh(merged, visible_frame, chunk);
    }
  }

  uint32_t removed_chunks = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       update_frame.chunks) {
    if (chunk_mesh_has_geometry(chunk)) {
      append_chunk_mesh(merged, update_frame, chunk);
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
  if (buffers.opaque_faces != nullptr) {
    SDL_ReleaseGPUBuffer(device, buffers.opaque_faces);
    buffers.opaque_faces = nullptr;
  }
  if (buffers.transparent_faces != nullptr) {
    SDL_ReleaseGPUBuffer(device, buffers.transparent_faces);
    buffers.transparent_faces = nullptr;
  }
  if (buffers.sprite_vertices != nullptr) {
    SDL_ReleaseGPUBuffer(device, buffers.sprite_vertices);
    buffers.sprite_vertices = nullptr;
  }
}

bool upload_world_mesh_frame(SDL_GPUDevice *device,
                             const world_mesh_upload_frame &upload_frame,
                             world_mesh_gpu_buffers &buffers,
                             uint64_t frame_index) {
  if (upload_frame.chunks.empty()) {
    return true;
  }

  if (!upload_gpu_buffer(device, upload_frame.opaque_faces.data(),
                         upload_frame.opaque_bytes,
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                         "gpu_chunk_mesh_opaque", buffers.opaque_faces) ||
      !upload_gpu_buffer(device, upload_frame.transparent_faces.data(),
                         upload_frame.transparent_bytes,
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                         "gpu_chunk_mesh_transparent",
                         buffers.transparent_faces) ||
      !upload_gpu_buffer(device, upload_frame.sprite_vertices.data(),
                         upload_frame.sprite_bytes,
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                         "gpu_chunk_mesh_sprite", buffers.sprite_vertices)) {
    return false;
  }

  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_upload frame=%" PRIu64
                 " active=1 target=sdl_gpu chunks=%zu opaque_bytes=%" PRIu64
                 " transparent_bytes=%" PRIu64 " sprite_bytes=%" PRIu64
                 " fluid_blocks=%" PRIu32 "\n",
                 frame_index, upload_frame.chunks.size(),
                 upload_frame.opaque_bytes, upload_frame.transparent_bytes,
                 upload_frame.sprite_bytes, upload_frame.fluid_blocks);
    std::fflush(octaryn_client_app::g_log);
  }
  return true;
}
