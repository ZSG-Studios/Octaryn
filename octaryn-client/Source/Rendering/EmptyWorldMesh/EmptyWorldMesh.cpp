#include "EmptyWorldMesh.h"

#include "Log.h"
#include "View.h"

#include <cinttypes>
#include <cstdio>

void build_empty_world_mesh_frame_from_stream(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const octaryn_client_app::block_lookup &overrides,
    const chunk_view &previous_chunk_view,
    world_mesh_upload_frame &mesh_frame) {
  const chunk_view stream_view =
      chunk_view_from_server_stream(stream);
  build_empty_world_mesh_frame(stream_view, previous_chunk_view, overrides,
                               mesh_frame);

  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "native_empty_chunk_stream active=1 source=server_background "
                 "epoch=%" PRIu64 " render_distance=%" PRIu32
                 " columns=%zu override_edits=%zu visible_chunks=%zu "
                 "opaque_faces=%zu\n",
                 stream.epoch, stream.radius, stream.columns.size(),
                 overrides.size(), mesh_frame.chunks.size(),
                 mesh_frame.opaque_faces.size());
    std::fflush(octaryn_client_app::g_log);
  }
}
