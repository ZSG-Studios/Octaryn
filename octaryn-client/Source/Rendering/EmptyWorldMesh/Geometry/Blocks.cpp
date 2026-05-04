#include "EmptyWorldMesh.h"

#include "Packing.h"

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::has_block_override;
using octaryn_client_app::world_block_record;

uint16_t empty_world_generated_block(const block_position_key &key) {
  return key.y >= kEmptyWorldMinY && key.y < 0 &&
                 key.y < kEmptyWorldMaxYExclusive
             ? 1u
             : 0u;
}

uint16_t empty_world_effective_block(const block_lookup &overrides,
                                     const block_position_key &key) {
  uint16_t block = 0u;
  return has_block_override(overrides, key, block)
             ? block
             : empty_world_generated_block(key);
}

void apply_empty_world_overrides_from_records(
    const std::vector<world_block_record> &records, block_lookup &overrides) {
  for (const world_block_record &record : records) {
    if (record.y < kEmptyWorldMinY || record.y >= kEmptyWorldMaxYExclusive) {
      continue;
    }

    overrides[block_position_key{record.x, record.y, record.z}] = record.block;
  }
}
