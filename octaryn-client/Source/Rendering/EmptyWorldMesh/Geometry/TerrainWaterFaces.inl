void append_water_surface_faces(terrain_face_batches &batches,
                                const block_lookup &overrides,
                                int32_t origin_x, int32_t origin_z,
                                const terrain_chunk_samples &samples) {
  const int32_t water_y = kTerrainWaterHeight - 1;
  std::array<bool, 1024> water{};
  for (int32_t local_z = 0; local_z < kEmptyWorldChunkSize; ++local_z) {
    for (int32_t local_x = 0; local_x < kEmptyWorldChunkSize; ++local_x) {
      const int32_t world_x = origin_x + local_x;
      const int32_t world_z = origin_z + local_z;
      const empty_world_terrain_column &column = samples.at(local_x, local_z);
      if (column.height >= kTerrainWaterHeight ||
          effective_block_at(overrides, world_x, water_y, world_z) !=
              kBlockWater) {
        continue;
      }
      water[static_cast<size_t>(local_z * kEmptyWorldChunkSize + local_x)] =
          true;
    }
  }
  std::array<bool, 1024> used{};
  for (int32_t local_z = 0; local_z < kEmptyWorldChunkSize; ++local_z) {
    for (int32_t local_x = 0; local_x < kEmptyWorldChunkSize; ++local_x) {
      const size_t index =
          static_cast<size_t>(local_z * kEmptyWorldChunkSize + local_x);
      if (used[index] || !water[index]) {
        continue;
      }
      int32_t width = 1;
      while (local_x + width < kEmptyWorldChunkSize) {
        const size_t next = static_cast<size_t>(
            local_z * kEmptyWorldChunkSize + local_x + width);
        if (used[next] || !water[next]) break;
        ++width;
      }
      int32_t depth = 1;
      bool can_extend = true;
      while (local_z + depth < kEmptyWorldChunkSize && can_extend) {
        for (int32_t x = 0; x < width; ++x) {
          const size_t next = static_cast<size_t>(
              (local_z + depth) * kEmptyWorldChunkSize + local_x + x);
          if (used[next] || !water[next]) {
            can_extend = false;
            break;
          }
        }
        if (can_extend) ++depth;
      }
      for (int32_t z = 0; z < depth; ++z) {
        for (int32_t x = 0; x < width; ++x) {
          used[static_cast<size_t>((local_z + z) * kEmptyWorldChunkSize +
                                   local_x + x)] = true;
        }
      }
      append_water_face(batches, origin_x + local_x, water_y,
                        origin_z + local_z, 4u,
                        static_cast<uint32_t>(width),
                        static_cast<uint32_t>(depth));
    }
  }
}
