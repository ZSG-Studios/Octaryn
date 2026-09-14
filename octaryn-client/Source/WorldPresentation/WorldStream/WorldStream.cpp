#include "WorldStream.h"
#include "StreamSnapshot.h"
#include "StreamResidency.h"
#include "StreamGenerationOrder.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <tuple>

namespace octaryn::client::world_presentation {

struct WorldStream::State : StreamResidency {
  std::filesystem::path path;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::string message{"waiting_for_server_snapshot"};
  bool stopping{};
  std::thread worker;

  explicit State(std::filesystem::path source) : path(std::move(source)), worker([this] { run(); }) {}
  ~State() {
    { std::lock_guard lock(mutex); stopping = true; }
    wake.notify_all();
    worker.join();
  }
  void run() {
    StreamSnapshot snapshot;
    std::filesystem::file_time_type last_write{};
    auto last_read = std::chrono::steady_clock::time_point{};
    StreamGenerationOrder order;
    RetiredPayloads retired;
    bool reorder=true;
    std::uint64_t planned_window{};
    for (;;) {
      std::unique_lock lock(mutex);
      if (stopping) return;
      collect_retired(retired);
      if(!retired.empty()) {
        lock.unlock();
        retired.clear();
        lock.lock();
        if(stopping) return;
      }
      if (ready.size() >= 2) {
        wake.wait_for(lock, std::chrono::milliseconds(50));
        continue;
      }
      const auto now = std::chrono::steady_clock::now();
      lock.unlock();
      if (now - last_read >= std::chrono::milliseconds(100)) {
        last_read = now;
        std::error_code ec;
        const auto write = std::filesystem::last_write_time(path, ec);
        if (!ec && (snapshot.columns.empty() || write != last_write)) {
          std::string error;
          try {
            if (read_stream_snapshot(path, snapshot, error)) {last_write=write;reorder=true;}
          } catch (const std::exception&) { error = "invalid_server_snapshot"; }
          std::lock_guard status_lock(mutex);
          message = error.empty() ? "streaming_authoritative_columns" : error;
        }
      }
      lock.lock();
      if (stopping) return;
      if(reorder || planned_window!=window_revision) {
        const auto requested_window=window_revision;
        const auto center_x=x,center_z=z;
        const auto requested_radius=radius;
        lock.unlock();
        try {order.reset(snapshot,center_x,center_z,requested_radius);}
        catch(const std::exception&) {
          lock.lock();message="terrain_schedule_failed";
          wake.wait_for(lock,std::chrono::milliseconds(100));continue;
        }
        lock.lock();
        if(stopping) return;
        // Even A->B->A must restart selection after synchronous completion invalidation.
        if(requested_window!=window_revision) {reorder=true;continue;}
        planned_window=requested_window;reorder=false;
      }
      const SnapshotColumn* selected=order.next(*this);
      if (selected == nullptr) {
        wake.wait_for(lock, std::chrono::milliseconds(50));
        continue;
      }
      lock.unlock();
      try {
        auto column = generate_stream_column(*selected, snapshot.epoch);
        auto query_column = std::make_shared<const StreamColumn>(column);
        lock.lock();
        if(!wanted(column.x,column.z)) {lock.unlock();continue;}
        if(retain(std::move(column), std::move(query_column))) order.consumed();
      } catch (const std::exception&) {
        if (!lock.owns_lock()) lock.lock();
        message = "terrain_generation_failed";
        wake.wait_for(lock, std::chrono::milliseconds(100));
      }
    }
  }
};

WorldStream::WorldStream(std::filesystem::path path) : state_(std::make_unique<State>(std::move(path))) {}
WorldStream::~WorldStream() = default;
void WorldStream::request(std::int32_t x, std::int32_t z, std::uint32_t radius) {
  std::lock_guard lock(state_->mutex);
  if (!state_->change_window(x, z, radius)) return;
  state_->wake.notify_all();
}
bool WorldStream::poll(StreamColumn& column) {
  std::lock_guard lock(state_->mutex);
  const bool delivered=state_->deliver(column);
  if(delivered) state_->wake.notify_all();
  return delivered;
}
bool WorldStream::peek(StreamColumn& column,const StreamColumn* excluded) const {
  std::lock_guard lock(state_->mutex);
  return state_->peek(column,excluded);
}
StreamPublication WorldStream::publish(const StreamColumn& column) {
  std::lock_guard lock(state_->mutex);
  const auto result=state_->publish(column);
  state_->wake.notify_all();
  return result;
}
std::string WorldStream::status() const {
  std::lock_guard lock(state_->mutex);
  return state_->message;
}
bool WorldStream::try_block(std::int32_t x, std::int32_t y, std::int32_t z, std::uint16_t& block) const {
  const int cx = x / 32 - (x % 32 < 0 ? 1 : 0);
  const int cz = z / 32 - (z % 32 < 0 ? 1 : 0);
  std::shared_ptr<const StreamColumn> column;
  {
    std::lock_guard lock(state_->mutex);
    column = state_->query(cx,cz);
  }
  if(!column) return false;
  block = y < StreamWorldMinY || y >= StreamWorldMinY + StreamWorldHeight ? 0 :
      column->blocks[static_cast<std::size_t>((x - cx * 32) + 32 * (y - StreamWorldMinY + StreamWorldHeight * (z - cz * 32)))];
  return true;
}

} // namespace octaryn::client::world_presentation
