#pragma once
#include "octaryn_shared_abi_types.h"
#include <stdint.h>
#define OCTARYN_HOST_API_RESIDENCY 15u
#define OCTARYN_HOST_RESIDENCY_API_VERSION 1u
#define OCTARYN_HOST_RESIDENCY_API_SIZE 40u
#define OCTARYN_HOST_REGION_STATUS_SIZE 192u
#define OCTARYN_REGION_WANTED 1u
#define OCTARYN_REGION_RETAINED 2u
#define OCTARYN_REGION_RENDER_READY 4u
#define OCTARYN_REGION_COLLISION_READY 8u
#define OCTARYN_REGION_FAILED 16u
#ifdef __cplusplus
extern "C" {
#endif
typedef struct octaryn_host_region_status {
 uint32_t version,size,index,phase,flags,reserved;
 uint64_t generation;
 float bounds[6];
 char id[129];
 uint8_t padding[7];
} octaryn_host_region_status;
typedef struct octaryn_host_region_anchor {float x,y,z;} octaryn_host_region_anchor;
typedef struct octaryn_host_residency_api {
 uint32_t version,size;
 int (OCTARYN_ABI_CALL* count)(uint32_t*,uint64_t*);
 int (OCTARYN_ABI_CALL* query)(uint32_t,octaryn_host_region_status*);
 int (OCTARYN_ABI_CALL* set_desired)(const uint32_t*,uint32_t,const uint32_t*,uint32_t);
 int (OCTARYN_ABI_CALL* actor_position)(octaryn_host_region_anchor*);
} octaryn_host_residency_api;
#ifdef __cplusplus
}
static_assert(sizeof(octaryn_host_region_anchor)==12);
static_assert(sizeof(octaryn_host_region_status)==OCTARYN_HOST_REGION_STATUS_SIZE);
static_assert(sizeof(octaryn_host_residency_api)==OCTARYN_HOST_RESIDENCY_API_SIZE);
#endif
