#pragma once
#include "BlockEditService.h"

extern "C" {
struct octaryn_server_fluid_config {
  uint32_t version, size;
  uint16_t water[8], lava[8];
  uint16_t stone, reserved;
  uint32_t replaceable_count, solid_count;
  const uint16_t *replaceable, *solid;
};
struct octaryn_server_fluid_tick_report {
  uint32_t version, size, evaluations, apply_attempts;
  uint32_t changed, pending, reads, repair_samples;
  uint32_t capacity_deferrals, budget_exhausted;
  uint64_t now_ms;
};
OCTARYN_SERVER_BLOCK_STORE_API void *octaryn_server_fluid_create(
    const octaryn_server_fluid_config *config);
OCTARYN_SERVER_BLOCK_STORE_API void octaryn_server_fluid_destroy(void *simulation);
OCTARYN_SERVER_BLOCK_STORE_API int32_t octaryn_server_fluid_set_region(
    void *simulation, int32_t center_x, int32_t center_z, uint32_t radius);
OCTARYN_SERVER_BLOCK_STORE_API int32_t octaryn_server_fluid_notify(
    void *simulation, const octaryn_server_block_edit *changes, uint32_t count);
OCTARYN_SERVER_BLOCK_STORE_API int32_t octaryn_server_fluid_tick(
    void *simulation, void *store, void *change_queue, double delta_seconds,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *context, octaryn_server_fluid_tick_report *report);
}
