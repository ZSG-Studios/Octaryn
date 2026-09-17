#pragma once
#include "ColumnBlocks.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace octaryn::client::world_presentation {

inline constexpr int StreamColumnWidth = 32;
inline constexpr int StreamWorldMinY = -256;
inline constexpr int StreamWorldHeight = 512;

struct StreamColumn {
  std::int32_t x{}, z{};
  std::int32_t min_y{StreamWorldMinY}, height{StreamWorldHeight};
  std::uint64_t epoch{}, revision{};
  // revision is the content hash; this is the server's durable world revision.
  std::uint64_t authoritative_revision{};
  // Block IDs, including edited air. x + 32 * ((world_y - min_y) + height * z).
  ColumnBlocks blocks;
};

enum class StreamPublication { Busy, Retired, Published };

class WorldStream {
public:
  explicit WorldStream(std::filesystem::path binary_snapshot_path);
  ~WorldStream();
  WorldStream(const WorldStream&) = delete;
  WorldStream& operator=(const WorldStream&) = delete;
  void request(std::int32_t center_x, std::int32_t center_z, std::uint32_t radius);
  // Publishes this column's query view. The presentation thread must complete
  // its GPU replacement before camera/target queries, as OpenWorld does.
  bool poll(StreamColumn& column);
  // Peek keeps the bounded mailbox and old query intact during GPU submission.
  bool peek(StreamColumn& column,const StreamColumn* excluded=nullptr) const;
  StreamPublication publish(const StreamColumn& column);
  // Only delivered, still-visible columns are queryable; worker-ready data is not.
 bool try_block(std::int32_t x, std::int32_t y, std::int32_t z, std::uint16_t& block) const;
 bool can_predict() const;
 bool predict_block(std::uint64_t command,std::int32_t x,std::int32_t y,std::int32_t z,std::uint16_t block);
 void resolve_block(std::uint64_t command,bool accepted,std::uint64_t revision);
 void reset_predictions();
  std::string status() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace octaryn::client::world_presentation
