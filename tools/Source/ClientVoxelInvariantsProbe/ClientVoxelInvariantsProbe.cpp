#include "ChunkPalette.h"
#include "ChunkView.h"
#include "ColumnStreaming.h"
#include "GpuChunkPayload.h"
#include "PackedVoxelQuad.h"
#include "RenderDistance.h"
#include "RenderDistanceRing.h"
#include "VoxelLayout.h"

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace voxel = octaryn::client::voxel;

namespace {

bool expect_equal(std::string_view label, int actual, int expected)
{
    if (actual == expected) {
        return true;
    }

    std::fprintf(stderr, "%.*s: expected %d, got %d\n",
                 static_cast<int>(label.size()), label.data(), expected,
                 actual);
    return false;
}

bool expect_true(std::string_view label, bool value)
{
    if (value) {
        return true;
    }

    std::fprintf(stderr, "%.*s: expected true\n",
                 static_cast<int>(label.size()), label.data());
    return false;
}

bool validate_world_layout()
{
    bool ok = true;
    ok &= expect_equal("chunk width", CHUNK_VIEW_CHUNK_WIDTH, 32);
    ok &= expect_equal("voxel chunk width", voxel::ChunkWidthBlocks, 32);
    ok &= expect_equal("chunk voxel count", voxel::ChunkVoxelCount, 32768);
    ok &= expect_equal("column height chunks", CHUNK_VIEW_COLUMN_HEIGHT_CHUNKS,
                       32);
    ok &= expect_equal("world height blocks", CHUNK_VIEW_WORLD_HEIGHT_BLOCKS,
                       1024);
    ok &= expect_equal("voxel column slots", voxel::ColumnVoxelSlots,
                       1048576);
    return ok;
}

bool validate_render_distance_options()
{
    const int* options = render_distance_options();
    const int count = render_distance_option_count();
    bool ok = true;

    ok &= expect_equal("render distance min", RENDER_DISTANCE_MIN_CHUNKS, 4);
    ok &= expect_equal("render distance max", RENDER_DISTANCE_MAX_CHUNKS, 128);
    ok &= expect_equal("render distance target max",
                       RENDER_DISTANCE_TARGET_MAX_CHUNKS, 128);
    ok &= expect_equal("render distance step", RENDER_DISTANCE_STEP_CHUNKS, 4);
    constexpr int choices[]={4,8,12,16,20,24,32};
    ok &= expect_equal("render distance option count", count, 7);
    ok &= expect_true("render distance options pointer", options != nullptr);
    if (options == nullptr) {
        return false;
    }

    for (int index = 0; index < count && index < 7; ++index) {
        const int expected = choices[index];
        ok &= expect_equal("render distance option", options[index], expected);
        ok &= expect_equal("render distance sanitize exact",
                           render_distance_sanitize(expected), expected);
        ok &= expect_true("voxel render distance accepted",
                          voxel::is_valid_render_distance(expected));
    }

    ok &= expect_equal("render distance sanitize low",
                       render_distance_sanitize(-1),
                       RENDER_DISTANCE_MIN_CHUNKS);
    ok &= expect_equal("render distance sanitize high",
                       render_distance_sanitize(999),
                       32);
    ok &= expect_equal("render distance sanitize floor",
                       render_distance_sanitize(31), 24);
    ok &= expect_equal("render distance next step",
                       render_distance_next_step(4, 128), 8);
    ok &= expect_equal("render distance next clamps target",
                       render_distance_next_step(24, 999), 32);
    return ok;
}

bool validate_chunk_view()
{
    bool ok = true;
    const chunk_view view =
        chunk_view_for_camera(0.0f, 0.0f, RENDER_DISTANCE_MAX_CHUNKS);
    ok &= expect_equal("chunk view target max width", CHUNK_VIEW_MAX_WIDTH,
                       257);
    ok &= expect_equal("render distance max view width", view.width, 257);
    ok &= expect_equal("render distance max origin x", view.origin_x, -128);
    ok &= expect_equal("render distance max origin z", view.origin_z, -128);
    ok &= expect_equal("render distance max columns", view.width * view.width,
                       66049);

    const chunk_view oversized = chunk_view_for_camera(0.0f, 0.0f, 999);
    ok &= expect_equal("oversized render distance has no hidden buffer",
                       oversized.width, CHUNK_VIEW_MAX_WIDTH);
    return ok;
}

bool validate_ring_priority()
{
    bool ok = true;
    ok &= expect_true("origin is inside render square",
                      voxel::is_inside_render_square(0, 0, 4));
    ok &= expect_true("edge is inside render square",
                      voxel::is_inside_render_square(4, -4, 4));
    ok &= expect_true("outside render square rejected",
                      !voxel::is_inside_render_square(5, 0, 4));

    const auto center = voxel::column_load_priority(0, 0);
    const auto near = voxel::column_load_priority(1, 0);
    const auto diagonal = voxel::column_load_priority(1, 1);
    ok &= expect_equal("center ring", center.ring, 0);
    ok &= expect_equal("near ring", near.ring, 1);
    ok &= expect_equal("diagonal manhattan", diagonal.manhattan, 2);
    ok &= expect_true("center sorts before near",
                      voxel::compare_column_load_priority(center, near) < 0);
    ok &= expect_true("near sorts before diagonal",
                      voxel::compare_column_load_priority(near, diagonal) < 0);
    return ok;
}

bool validate_palette()
{
    bool ok = true;
    ok &= expect_equal("empty chunk bits", voxel::choose_bits_per_voxel(0), 0);
    ok &= expect_equal("uniform chunk bits", voxel::choose_bits_per_voxel(1),
                       0);
    ok &= expect_equal("small palette bits", voxel::choose_bits_per_voxel(16),
                       4);
    ok &= expect_equal("byte palette bits", voxel::choose_bits_per_voxel(256),
                       8);
    ok &= expect_equal("wide palette bits", voxel::choose_bits_per_voxel(257),
                       16);
    ok &= expect_equal("empty payload bytes", static_cast<int>(voxel::packed_voxel_payload_bytes(0)),
                       0);
    ok &= expect_equal("8-bit payload bytes", static_cast<int>(voxel::packed_voxel_payload_bytes(8)),
                       32768);
    ok &= expect_true("empty header valid",
                      voxel::chunk_palette_header_valid({0, 0,
                          voxel::ChunkPaletteEmpty}));
    ok &= expect_true("uniform header valid",
                      voxel::chunk_palette_header_valid({1, 0,
                          voxel::ChunkPaletteUniform}));
    ok &= expect_true("mixed header valid",
                      voxel::chunk_palette_header_valid({17, 8,
                          voxel::ChunkPaletteMixed}));
    ok &= expect_true("wrong mixed bits invalid",
                      !voxel::chunk_palette_header_valid({4, 8,
                          voxel::ChunkPaletteMixed}));
    ok &= expect_true("empty mixed invalid",
                      !voxel::chunk_palette_header_valid({0, 0,
                          voxel::ChunkPaletteMixed}));
    return ok;
}

bool validate_gpu_chunk_payload()
{
    const auto empty = voxel::make_empty_gpu_chunk(0, 0);
    const auto uniform = voxel::make_uniform_gpu_chunk(1, 0, 42);
    const std::uint8_t bits_per_voxel = voxel::choose_bits_per_voxel(4);
    const std::uint32_t payload_size =
        voxel::packed_voxel_payload_bytes(bits_per_voxel);
    std::vector<std::uint8_t> payload(payload_size, 0x5au);
    const voxel::GpuChunkPaletteEntry palette[] = {
        {11, 110, 1, 0},
        {12, 120, 1, 0},
        {13, 130, 1, 0},
        {14, 140, 1, 0},
    };
    const auto mixed = voxel::make_mixed_gpu_chunk(
        2, 0, palette, 4, payload.data(), payload_size);

    bool ok = true;
    ok &= expect_equal("gpu chunk header bytes",
                       static_cast<int>(sizeof(voxel::GpuChunkHeader)), 32);
    ok &= expect_equal("gpu palette entry bytes",
                       static_cast<int>(sizeof(voxel::GpuChunkPaletteEntry)),
                       16);
    ok &= expect_true("empty gpu chunk valid",
                      voxel::gpu_chunk_payload_valid(empty));
    ok &= expect_true("uniform gpu chunk valid",
                      voxel::gpu_chunk_payload_valid(uniform));
    ok &= expect_true("mixed gpu chunk valid",
                      voxel::gpu_chunk_payload_valid(mixed));
    ok &= expect_equal("uniform gpu block",
                       static_cast<int>(uniform.header.uniform_block_id), 42);
    ok &= expect_equal("mixed gpu payload bytes",
                       static_cast<int>(mixed.payload.size()),
                       static_cast<int>(payload_size));
    return ok;
}

bool validate_column_streaming()
{
    bool ok = true;
    const auto budget = voxel::default_streaming_budget();
    ok &= expect_equal("streaming request budget",
                       static_cast<int>(budget.max_new_column_requests), 8);
    ok &= expect_equal("streaming chunk upload budget",
                       static_cast<int>(budget.max_chunk_uploads), 64);
    ok &= expect_true("missing allows generation",
                      voxel::column_state_allows_generation(
                          voxel::ColumnState::Missing));
    ok &= expect_true("resident allows eviction",
                      voxel::column_state_allows_eviction(
                          voxel::ColumnState::GpuResident));

    const auto columns = voxel::build_required_columns({10, -20}, 4);
    ok &= expect_equal("radius 4 column count",
                       static_cast<int>(columns.size()), 81);
    if (!columns.empty()) {
        ok &= expect_equal("first column center x", columns.front().coord.x,
                           10);
        ok &= expect_equal("first column center z", columns.front().coord.z,
                           -20);
        ok &= expect_equal("first column ring", columns.front().priority.ring,
                           0);
        ok &= expect_equal("last column ring", columns.back().priority.ring, 4);
    }

    const auto max_columns = voxel::build_required_columns({0, 0}, 128);
    ok &= expect_equal("radius 128 column count",
                       static_cast<int>(max_columns.size()), 66049);
    const auto rejected = voxel::build_required_columns({0, 0}, 129);
    ok &= expect_equal("invalid radius has no hidden buffer",
                       static_cast<int>(rejected.size()), 0);
    return ok;
}

bool validate_quad_packing()
{
    const voxel::DecodedVoxelQuad quad = {
        31, 30, 29, 32, 31, 5, 3, 0xabcde, 7, 0x5a, 9, 11, 1024, 42};
    bool ok = true;
    ok &= expect_true("quad fields valid",
                      voxel::voxel_quad16_fields_valid(quad));
    const auto packed = voxel::pack_voxel_quad16(quad);
    const auto unpacked = voxel::unpack_voxel_quad16(packed);
    ok &= expect_equal("quad x", static_cast<int>(unpacked.local_x), 31);
    ok &= expect_equal("quad y", static_cast<int>(unpacked.local_y), 30);
    ok &= expect_equal("quad z", static_cast<int>(unpacked.local_z), 29);
    ok &= expect_equal("quad width", static_cast<int>(unpacked.width), 32);
    ok &= expect_equal("quad height", static_cast<int>(unpacked.height), 31);
    ok &= expect_equal("quad face", static_cast<int>(unpacked.face_dir), 5);
    ok &= expect_equal("quad lod", static_cast<int>(unpacked.lod_tier), 3);
    ok &= expect_equal("quad material", static_cast<int>(unpacked.material_id),
                       0xabcde);
    ok &= expect_equal("quad material bin",
                       static_cast<int>(unpacked.material_bin), 42);
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= validate_world_layout();
    ok &= validate_render_distance_options();
    ok &= validate_chunk_view();
    ok &= validate_ring_priority();
    ok &= validate_palette();
    ok &= validate_gpu_chunk_payload();
    ok &= validate_column_streaming();
    ok &= validate_quad_packing();

    if (!ok) {
        return 1;
    }

    std::puts("client voxel invariants probe passed");
    return 0;
}
