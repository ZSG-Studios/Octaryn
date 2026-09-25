#include "WorldStream.h"
#include "PredictedBlocks.h"
#include "StreamSnapshot.h"
#include "StreamResidency.h"
#include "StreamGenerationOrder.h"
#include "StreamNeighborhood.h"
#include "../../Threading/BackgroundThread.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

namespace octaryn::client::world_presentation {
namespace {
std::shared_ptr<const StreamColumn> query_copy(const StreamColumn& column) {
  auto query=std::make_shared<StreamColumn>(column);
  query->mesh_halo.reset();
  return query;
}
}

struct WorldStream::State : StreamResidency {
 PredictedBlocks predictions;
  std::filesystem::path path;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::condition_variable generation_wake;
  std::deque<std::packaged_task<StreamColumn()>> generation_queue;
  std::string message{"waiting_for_server_snapshot"};
  std::uint64_t generated_columns{};
  bool requested{};
  bool stopping{};
  std::array<std::thread,StreamGenerationWorkers> generators;
  std::thread worker;

  struct GenerationTask {
    SnapshotColumn source;
    std::uint64_t epoch{};
    std::uint64_t window{};
    std::uint64_t snapshot{};
    std::future<StreamColumn> result;
  };

  explicit State(std::filesystem::path source) : path(std::move(source)) {
    for(auto& generator:generators)
      generator=std::thread([this] { generation_run(); });
    worker=std::thread([this] { run(); });
  }
  ~State() {
    { std::lock_guard lock(mutex); stopping = true; }
    wake.notify_all();
    generation_wake.notify_all();
    worker.join();
    for(auto& generator:generators)if(generator.joinable())generator.join();
  }
  void generation_run() {
    threading::set_background_thread_priority("terrain_generation");
    for(;;) {
      std::packaged_task<StreamColumn()> task;
      {
        std::unique_lock lock(mutex);
        generation_wake.wait(lock,[this] {return stopping || !generation_queue.empty();});
        if(stopping && generation_queue.empty())return;
        task=std::move(generation_queue.front());
        generation_queue.pop_front();
      }
      task();
      // The future is ready before taking the mutex, so the scheduler cannot
      // miss this notification between its readiness check and wait.
      {std::lock_guard lock(mutex);wake.notify_one();}
    }
  }
  void run() {
    StreamSnapshot snapshot;
    std::filesystem::file_time_type last_write{};
    auto last_read = std::chrono::steady_clock::time_point{};
    StreamGenerationOrder order;
    StreamNeighborhood neighborhood;
    RetiredPayloads retired;
    bool reorder=true;
    std::uint64_t planned_window{};
    std::uint64_t snapshot_revision{};
    // GPU count/emit holds ready payloads until publication. Keep a second
    // batch ready for dispatch and independent bounded CPU tasks behind it.
    std::vector<GenerationTask> tasks;
    tasks.reserve(StreamGenerationWorkers);
    for (;;) {
      std::unique_lock lock(mutex);
      // Prepare borders only after the owner supplies the actual load window.
      wake.wait(lock,[this] {return stopping || requested;});
      if (stopping) return;
      collect_retired(retired);
      if(!retired.empty()) {
        lock.unlock();
        retired.clear();
        lock.lock();
        if(stopping) return;
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
            if (read_stream_snapshot(path, snapshot, error)) {
              last_write=write;reorder=true;++snapshot_revision;
            }
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
        try {
          order.reset(snapshot,center_x,center_z,requested_radius);
          neighborhood.reset(snapshot,center_x,center_z,requested_radius);
        }
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

      // Publish completed work in dispatch order. This preserves the existing
      // center-out ring priority even when the second task finishes first.
      if (ready.size()<StreamReadyCapacity && !tasks.empty() &&
          tasks.front().result.wait_for(std::chrono::milliseconds(0)) ==
              std::future_status::ready) {
        auto task=std::move(tasks.front());
        tasks.erase(tasks.begin());
        lock.unlock();
        try {
          auto column=task.result.get();
          lock.lock();
          if (task.window!=window_revision || task.snapshot!=snapshot_revision ||
              !wanted(column.x,column.z)) {
            reorder=true;
            lock.unlock();
            continue;
          }
          column.epoch=task.epoch;
          column.authoritative_revision=task.source.authoritative_revision;
          auto query_column=query_copy(column);
          retain(std::move(column),std::move(query_column));
        } catch (const std::exception&) {
          if (!lock.owns_lock()) lock.lock();
          message="terrain_generation_failed";
          reorder=true;
        }
        continue;
      }

      // Completed futures retain their payload until the bounded mailbox has
      // room. Snapshot copies keep running tasks independent of reloads.
      while (tasks.size()<StreamGenerationWorkers) {
        std::array<const SnapshotColumn*,StreamGenerationWorkers> pending{};
        std::size_t pending_count{};
        for(const auto& task:tasks)
          if(task.window==window_revision && task.snapshot==snapshot_revision)
            pending[pending_count++]=&task.source;
        const SnapshotColumn* selected=order.next(*this,std::span<const SnapshotColumn* const>(pending.data(),pending_count));
        if (selected==nullptr) break;
        const auto cached=cached_column(selected->x,selected->z,selected->revision);
        if (cached) {
          // Cached metadata must not overtake an earlier generated column.
          if(!tasks.empty() || ready.size()>=StreamReadyCapacity)break;
          StreamColumn column=*cached;
          column.epoch=snapshot.epoch;
          column.authoritative_revision=selected->authoritative_revision;
          auto query_column=query_copy(column);
          if (!retain(std::move(column),std::move(query_column))) break;
          order.consumed();
          continue;
        }
        GenerationTask task;
        task.source=*selected;
        task.epoch=snapshot.epoch;
        task.window=window_revision;
        task.snapshot=snapshot_revision;
        order.consumed();
        try {
          std::packaged_task<StreamColumn()> generated([this,source=task.source,epoch=task.epoch,
              neighbors=neighborhood.capture(task.source)] {
            auto column=generate_stream_column(source,epoch);
            prepare_stream_halo(column,neighbors);
            {std::lock_guard lock(mutex);++generated_columns;}
            return column;
          });
          task.result=generated.get_future();
          generation_queue.push_back(std::move(generated));
          generation_wake.notify_one();
          tasks.push_back(std::move(task));
        } catch (const std::exception&) {
          message="terrain_generation_failed";
          reorder=true;
          break;
        }
      }

      wake.wait_for(lock,std::chrono::milliseconds(50));
    }
  }
};

WorldStream::WorldStream(std::filesystem::path path) : state_(std::make_unique<State>(std::move(path))) {}
WorldStream::~WorldStream() = default;
void WorldStream::request(std::int32_t x, std::int32_t z, std::uint32_t radius) {
  std::lock_guard lock(state_->mutex);
  const bool changed=state_->change_window(x,z,radius);
  if(state_->requested && !changed)return;
  state_->requested=true;
  state_->wake.notify_all();
}
bool WorldStream::poll(StreamColumn& column) {
  std::lock_guard lock(state_->mutex);
  const bool delivered=state_->deliver(column);
 if(delivered) {
 state_->predictions.cover(column.x,column.z,column.authoritative_revision);
 state_->wake.notify_all();
 }
  return delivered;
}
bool WorldStream::peek(StreamColumn& column,const StreamColumn* excluded) const {
  std::lock_guard lock(state_->mutex);
  return state_->peek(column,excluded);
}
bool WorldStream::peek(StreamColumn& column,std::span<const StreamColumn* const> excluded) const {
  std::lock_guard lock(state_->mutex);
  return state_->peek(column,excluded);
}
StreamPublication WorldStream::publish(const StreamColumn& column) {
  std::lock_guard lock(state_->mutex);
 const auto result=state_->publish(column);
 if(result==StreamPublication::Published) state_->predictions.cover(column.x,column.z,column.authoritative_revision);
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
 if(column && state_->predictions.query(x,y,z,block)) return true;
  }
  if(!column) return false;
  block = y < StreamWorldMinY || y >= StreamWorldMinY + StreamWorldHeight ? 0 :
      column->blocks[static_cast<std::size_t>((x - cx * 32) + 32 * (y - StreamWorldMinY + StreamWorldHeight * (z - cz * 32)))];
  return true;
}

bool WorldStream::can_predict() const {
 std::lock_guard lock(state_->mutex);
 return state_->predictions.can_submit();
}
std::uint64_t WorldStream::generated_columns() const {
  std::lock_guard lock(state_->mutex);
  return state_->generated_columns;
}
bool WorldStream::predict_block(std::uint64_t command,std::int32_t x,std::int32_t y,std::int32_t z,std::uint16_t block) {
 std::lock_guard lock(state_->mutex);
 if(!state_->query(PredictedBlocks::column(x),PredictedBlocks::column(z))) return false;
 return state_->predictions.add(command,x,y,z,block);
}
void WorldStream::resolve_block(std::uint64_t command,bool accepted,std::uint64_t revision) {
 std::lock_guard lock(state_->mutex);
 state_->predictions.resolve(command,accepted,revision);
 // The baseline may have arrived before its receipt.
 const auto edits=state_->predictions.edits();
 for(const auto& edit:edits) {
 const auto x=PredictedBlocks::column(edit.x),z=PredictedBlocks::column(edit.z);
 if(const auto source=state_->query(x,z)) state_->predictions.cover(x,z,source->authoritative_revision);
 }
}
void WorldStream::reset_predictions() {
 std::lock_guard lock(state_->mutex);
 state_->predictions.clear();
}
} // namespace octaryn::client::world_presentation
