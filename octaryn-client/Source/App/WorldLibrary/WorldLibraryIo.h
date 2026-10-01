#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace octaryn::client::app {
enum class WorldLibraryIo { SourceParse,ResourceHash,ModelLoad,PreparedCatalog };
struct WorldLibraryIoStats {
  uint64_t source_parses{},resource_hashes{},model_loads{},prepared_catalog_reads{};
};
inline std::array<std::atomic_uint64_t,4> world_library_io_counts{};
inline void world_library_note_io(WorldLibraryIo operation) {
  world_library_io_counts[static_cast<unsigned>(operation)].fetch_add(1,std::memory_order_relaxed);
}
inline WorldLibraryIoStats world_library_io_stats() {
  return {world_library_io_counts[0].load(),world_library_io_counts[1].load(),
      world_library_io_counts[2].load(),world_library_io_counts[3].load()};
}
inline void world_library_reset_io_stats() {for(auto& count:world_library_io_counts)count.store(0);}
}
