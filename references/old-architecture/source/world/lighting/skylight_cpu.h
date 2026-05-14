#pragma once

#include <cstddef>

#include <SDL3/SDL.h>

#include "world/block/block.h"
#include "world/runtime/world.h"

namespace skylight_cpu {

constexpr int kNeighborhoodWidth = CHUNK_WIDTH * 3;
constexpr int kNeighborhoodDepth = CHUNK_WIDTH * 3;
constexpr int kHalo = 1;
constexpr int kOutputWidth = CHUNK_WIDTH + kHalo * 2;
constexpr int kOutputDepth = CHUNK_WIDTH + kHalo * 2;
constexpr Uint8 kMaxValue = 15;

auto output_value_count() -> std::size_t;
auto output_index(int x, int y, int z) -> std::size_t;
void build(const block_t* neighborhood_blocks, Uint8* out_values);

} // namespace skylight_cpu
