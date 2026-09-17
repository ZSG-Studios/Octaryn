#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace octaryn::client::world_presentation { struct StreamColumn; }
namespace octaryn::client::rendering {
struct WorldRenderer;
struct TracePublicationStats {
  std::uint64_t published{},discarded{},capacity_rejections{};
  std::size_t pending{},queued{};
};
class WorldTracePublication {
public:
  WorldTracePublication();
  ~WorldTracePublication();
  void offer(WorldRenderer&,const world_presentation::StreamColumn&);
  void retire(WorldRenderer&,const world_presentation::StreamColumn&);
  void predictions_changed(WorldRenderer&,std::int32_t x,std::int32_t z);
  void retain_window(WorldRenderer&);
  bool pump(WorldRenderer&);
  TracePublicationStats stats() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
