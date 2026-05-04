#include "octaryn_client_native_empty_world_mesh.h"

#include "octaryn_client_native_empty_world_mesh_packing.h"

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::has_block_override;
using octaryn_client_app::world_block_record;

uint16_t native_empty_generated_block(const block_position_key &key) {
  return key.y >= kNativeEmptyWorldMinY &&
                 key.y < 0 &&
                 key.y < kNativeEmptyWorldMaxYExclusive
             ? 1u
             : 0u;
}

uint16_t native_empty_effective_block(const block_lookup &overrides,
                                      const block_position_key &key) {
  uint16_t block = 0u;
  return has_block_override(overrides, key, block)
             ? block
             : native_empty_generated_block(key);
}

void apply_native_empty_overrides_from_records(
    const std::vector<world_block_record> &records, block_lookup &overrides) {
  for (const world_block_record &record : records) {
    if (record.y < kNativeEmptyWorldMinY ||
        record.y >= kNativeEmptyWorldMaxYExclusive) {
      continue;
    }

    overrides[block_position_key{record.x, record.y, record.z}] = record.block;
  }
}
