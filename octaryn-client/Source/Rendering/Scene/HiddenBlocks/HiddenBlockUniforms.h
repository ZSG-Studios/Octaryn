#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIDDEN_BLOCK_CAPACITY 32u

typedef struct hidden_block_position
{
    int32_t x;
    int32_t y;
    int32_t z;
} hidden_block_position;

typedef struct hidden_block_uniforms
{
    uint32_t count;
    int32_t pad[3];
    int32_t blocks[HIDDEN_BLOCK_CAPACITY][4];
} hidden_block_uniforms;

void hidden_block_uniforms_clear(hidden_block_uniforms* uniforms);
void hidden_block_uniforms_fill(
    hidden_block_uniforms* uniforms,
    const hidden_block_position* positions,
    uint32_t position_count);

#ifdef __cplusplus
}
#endif
