struct terrain_side_cell {
  uint16_t block;
  bool used;
};

bool side_cell_matches(const std::array<terrain_side_cell, 1024> &cells,
                       int32_t x, int32_t y, uint16_t block) {
  const terrain_side_cell &cell = cells[static_cast<size_t>(y * 32 + x)];
  return !cell.used && cell.block == block;
}

void mark_side_cells_used(std::array<terrain_side_cell, 1024> &cells,
                          int32_t x, int32_t y, int32_t width,
                          int32_t height) {
  for (int32_t dy = 0; dy < height; ++dy) {
    for (int32_t dx = 0; dx < width; ++dx) {
      cells[static_cast<size_t>((y + dy) * 32 + x + dx)].used = true;
    }
  }
}

void append_side_rectangle(terrain_face_batches &batches, int32_t origin_x,
                           int32_t origin_z, int32_t row, int32_t step,
                           int32_t world_y, uint32_t direction,
                           bool run_along_x, int32_t width, int32_t height,
                           uint16_t block) {
  const int32_t local_x = run_along_x ? step : row;
  const int32_t local_z = run_along_x ? row : step;
  append_face(batches, origin_x + local_x, world_y, origin_z + local_z,
              direction, static_cast<uint32_t>(width),
              static_cast<uint32_t>(height), block);
}

void append_side_cell_rectangles(terrain_face_batches &batches,
                                 const std::array<terrain_side_cell, 1024> &src,
                                 int32_t origin_x, int32_t origin_z,
                                 int32_t row, int32_t section_y0,
                                 uint32_t direction, bool run_along_x) {
  std::array<terrain_side_cell, 1024> cells = src;
  for (int32_t y = 0; y < kEmptyWorldChunkSize; ++y) {
    for (int32_t x = 0; x < kEmptyWorldChunkSize; ++x) {
      const terrain_side_cell &cell = cells[static_cast<size_t>(y * 32 + x)];
      if (cell.used || cell.block == kBlockAir) {
        continue;
      }
      int32_t width = 1;
      while (x + width < kEmptyWorldChunkSize &&
             side_cell_matches(cells, x + width, y, cell.block)) {
        ++width;
      }
      int32_t height = 1;
      bool can_extend = true;
      while (y + height < kEmptyWorldChunkSize && can_extend) {
        for (int32_t dx = 0; dx < width; ++dx) {
          if (!side_cell_matches(cells, x + dx, y + height, cell.block)) {
            can_extend = false;
            break;
          }
        }
        if (can_extend) {
          ++height;
        }
      }
      mark_side_cells_used(cells, x, y, width, height);
      append_side_rectangle(batches, origin_x, origin_z, row, x,
                            section_y0 + y, direction, run_along_x, width,
                            height, cell.block);
    }
  }
}

void append_terrain_side_runs(terrain_face_batches &batches,
                              const terrain_chunk_samples &samples,
                              int32_t origin_x, int32_t origin_z,
                              uint32_t direction, int32_t dx, int32_t dz,
                              bool run_along_x) {
  for (int32_t row = 0; row < kEmptyWorldChunkSize; ++row) {
    int32_t min_visible_y = kEmptyWorldMaxYExclusive;
    int32_t max_visible_y = kEmptyWorldMinChunkY * kEmptyWorldChunkSize;
    for (int32_t step = 0; step < kEmptyWorldChunkSize; ++step) {
      const int32_t local_x = run_along_x ? step : row;
      const int32_t local_z = run_along_x ? row : step;
      const empty_world_terrain_column &column = samples.at(local_x, local_z);
      const int32_t neighbor_height =
          samples.at(local_x + dx, local_z + dz).height;
      if (neighbor_height < column.height) {
        min_visible_y = std::min(min_visible_y, neighbor_height + 1);
        max_visible_y = std::max(max_visible_y, column.height);
      }
    }
    if (min_visible_y > max_visible_y) {
      continue;
    }
    const int32_t min_chunk_y =
        std::max(kEmptyWorldMinChunkY,
                 floor_div_int32(min_visible_y, kEmptyWorldChunkSize));
    const int32_t max_chunk_y =
        std::min(kEmptyWorldMaxChunkY,
                 floor_div_int32(max_visible_y, kEmptyWorldChunkSize));
    for (int32_t chunk_y = min_chunk_y; chunk_y <= max_chunk_y; ++chunk_y) {
      const int32_t section_y0 = chunk_y * kEmptyWorldChunkSize;
      const int32_t section_y1 = section_y0 + kEmptyWorldChunkSize - 1;
      std::array<terrain_side_cell, 1024> cells{};
      for (int32_t step = 0; step < kEmptyWorldChunkSize; ++step) {
        const int32_t local_x = run_along_x ? step : row;
        const int32_t local_z = run_along_x ? row : step;
        const empty_world_terrain_column &column = samples.at(local_x, local_z);
        const int32_t neighbor_height =
            samples.at(local_x + dx, local_z + dz).height;
        if (neighbor_height >= column.height) {
          continue;
        }
        const int32_t start_y = std::max(neighbor_height + 1, section_y0);
        const int32_t end_y = std::min(column.height, section_y1);
        for (int32_t y = start_y; y <= end_y; ++y) {
          const uint16_t block = y == column.height ? column.surface : column.fill;
          if (block != kBlockAir) {
            cells[static_cast<size_t>((y - section_y0) * 32 + step)].block = block;
          }
        }
      }
      append_side_cell_rectangles(batches, cells, origin_x, origin_z, row,
                                  section_y0, direction, run_along_x);
    }
  }
}
