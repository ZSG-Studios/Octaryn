#include "world/runtime/private.h"

void world_chunk_runtime_generate_blocks(chunk_t* chunk);

void world_gen_chunk_blocks_internal(chunk_t* chunk)
{
    world_chunk_runtime_generate_blocks(chunk);
}
