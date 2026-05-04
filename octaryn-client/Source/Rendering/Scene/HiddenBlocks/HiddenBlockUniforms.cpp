#include "HiddenBlockUniforms.h"

void hidden_block_uniforms_clear(hidden_block_uniforms* uniforms)
{
    if (uniforms == nullptr)
    {
        return;
    }

    uniforms->count = 0u;
    uniforms->pad[0] = 0;
    uniforms->pad[1] = 0;
    uniforms->pad[2] = 0;

    for (uint32_t i = 0u; i < HIDDEN_BLOCK_CAPACITY; ++i)
    {
        uniforms->blocks[i][0] = 0;
        uniforms->blocks[i][1] = 0;
        uniforms->blocks[i][2] = 0;
        uniforms->blocks[i][3] = 0;
    }
}

void hidden_block_uniforms_fill(
    hidden_block_uniforms* uniforms,
    const hidden_block_position* positions,
    uint32_t position_count)
{
    hidden_block_uniforms_clear(uniforms);

    if (uniforms == nullptr || positions == nullptr)
    {
        return;
    }

    const uint32_t copied_count =
        position_count < HIDDEN_BLOCK_CAPACITY ? position_count : HIDDEN_BLOCK_CAPACITY;
    uniforms->count = copied_count;

    for (uint32_t i = 0u; i < copied_count; ++i)
    {
        uniforms->blocks[i][0] = positions[i].x;
        uniforms->blocks[i][1] = positions[i].y;
        uniforms->blocks[i][2] = positions[i].z;
        uniforms->blocks[i][3] = 0;
    }
}
