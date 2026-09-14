#include "SessionIo.h"
#include "SessionFiles.h"
#include "PoseHistory.h"
#include "SessionIoWait.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

// Link SessionFiles.cpp with read_text=probe_read_text and write_text=probe_write_text.
// Only the transport wrapper injects stalls; production worker code is unchanged.
namespace octaryn::client::app::local_session {
bool probe_read_text(const std::filesystem::path&, std::string&);
bool probe_write_text(const std::filesystem::path&, std::string_view);
std::atomic<bool> block_reads{true}, read_waiting{};
std::atomic<unsigned> input_writes{}, reads{}, main_io{};
std::atomic<bool> record_reads{};
std::mutex timestamps_mutex;
std::vector<std::chrono::steady_clock::time_point> timestamps;
std::thread::id main_thread;
bool read_text(const std::filesystem::path& path, std::string& text) {
  if (std::this_thread::get_id() == main_thread) ++main_io;
  ++reads;
  if (record_reads) {
    std::lock_guard lock(timestamps_mutex);
    timestamps.push_back(std::chrono::steady_clock::now());
  }
  if (block_reads) {
    read_waiting = true;
    while (block_reads) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    read_waiting = false;
  }
  return probe_read_text(path, text);
}
bool write_text(const std::filesystem::path& path, std::string_view text) {
  if (std::this_thread::get_id() == main_thread) ++main_io;
  if (path.filename() == "input.json") ++input_writes;
  return probe_write_text(path, text);
}
}
namespace {
using namespace octaryn::client::app;
using namespace local_session;
using Clock = std::chrono::steady_clock;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
template<class Predicate> void until(Predicate predicate, const char* message) {
  const auto deadline = Clock::now() + std::chrono::seconds(3);
  while (!predicate()) {
    if (Clock::now() >= deadline) throw std::runtime_error(message);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
}
int main(int argc, char** argv) {
  using namespace octaryn::client::app;
  using namespace local_session;
  try {
    require(argc == 2, "provide fresh isolated probe directory");
    const std::filesystem::path root = argv[1];
    require(!std::filesystem::exists(root), "probe directory must be fresh");
    std::filesystem::create_directories(root);
    main_thread = std::this_thread::get_id();
    require(probe_write_text(root / "pose.json",
      R"({"version":1,"source":"server_player_state_stream","sourceTick":1,"sourceSeconds":1,"playerX":1,"playerY":35.62,"worldTimeDayFraction":0.5,"worldTimeTotalSeconds":43200})"), "seed pose");
    SessionIo io(root / "pose.json", root / "input.json", root / "window.json", root / "edit.json");
    struct UnblockOnExit { ~UnblockOnExit() { block_reads = false; } } unblock_on_exit;
    until([] { return read_waiting.load(); }, "worker entered slow read");
    double worst_us = 0;
    for (int i = 0; i < 2400; ++i) {
      const auto begin = Clock::now();
      io.publish_input(std::to_string(i));
      io.poll();
      worst_us = std::max(worst_us, std::chrono::duration<double, std::micro>(Clock::now() - begin).count());
    }
    require(input_writes == 0, "blocked worker must not write inputs");
    block_reads = false;
    until([] { return input_writes.load() == 1; }, "latest input published");
    std::string text;
    until([&] { return probe_read_text(root / "input.json", text) && text == "2399"; }, "obsolete unsent inputs discarded");
    std::optional<LocalPlayerPose> pose;
    until([&] { pose = io.poll().pose; return pose.has_value(); }, "pose delivered");
    require(pose->source_tick == 1 && pose->source_seconds == 1, "source identity preserved");
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    require(!io.poll().pose && input_writes == 1, "duplicate pose and input heartbeat suppressed");

    block_reads = true;
    until([] { return read_waiting.load(); }, "worker blocked again");
    io.publish_input("stale");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    block_reads = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    require(input_writes == 1, "input older than server freshness window discarded");
    io.publish_input("fresh");
    until([&] { return probe_read_text(root / "input.json", text) && text == "fresh"; }, "fresh input recovery");
    record_reads = true;
    until([] { std::lock_guard lock(timestamps_mutex); return timestamps.size() >= 121; },
        "actual worker must complete120 precise polling intervals within3seconds");
    record_reads = false;
    std::vector<Clock::time_point> poll_times;
    { std::lock_guard lock(timestamps_mutex); poll_times = timestamps; }
    std::vector<double> intervals;
    for (size_t index = 1; index < poll_times.size(); ++index)
      intervals.push_back(std::chrono::duration<double, std::milli>(poll_times[index] - poll_times[index - 1]).count());
    const double poll_seconds = std::chrono::duration<double>(poll_times.back() - poll_times.front()).count();
    std::sort(intervals.begin(), intervals.end());
    const auto median_ms = intervals[intervals.size() / 2];
    require(median_ms >= 12 && median_ms < 24 && poll_seconds < 2.8,
        "actual SessionIo polling must not quantize to31ms");
    const auto stop_started = Clock::now();
    io.stop();
    const auto stop_ms = std::chrono::duration<double, std::milli>(Clock::now() - stop_started).count();
    require(stop_ms < 100, "worker stop must interrupt its polling wait");
    const auto count = reads.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    require(reads == count && main_io == 0, "joined worker and no frame thread I/O");

    const Clock::time_point origin{};
    require(SessionIoWait::next(origin, origin + std::chrono::milliseconds(3)) ==
        origin + std::chrono::microseconds(16667), "poll deadline absorbs work duration");
    require(SessionIoWait::next(origin, origin + std::chrono::milliseconds(100)) ==
        origin + std::chrono::microseconds(100002), "missed polls skip without catch-up or phase reset");
    SessionIoWait interruptible;
    std::atomic<bool> entered{}, interrupted{};
    std::thread waiting([&] {
      entered = true;
      interrupted = !interruptible.until(Clock::now() + std::chrono::seconds(5));
    });
    until([&] { return entered.load(); }, "long wait entered");
    const auto interrupt_started = Clock::now();
    interruptible.stop();waiting.join();
    require(interrupted && Clock::now() - interrupt_started < std::chrono::milliseconds(100),
        "stop event must interrupt even a five-second pending timer");

    PoseHistory history;
    LocalPlayerPose first, last;
    first.source_seconds = 10; first.source_tick = 10;
    last.source_seconds = 10.2; last.source_tick = 11; last.x = 2;
    require(history.push(first) && history.push(last), "history setup");
    history.advance(.1);
    LocalPlayerPose presented;
    require(history.sample(presented) && std::abs(presented.source_seconds - 10.1) < 1e-8,
            "main presentation advances source clock exactly 1x");
    std::cout << "session_io_probe=passed latest_only=passed stale_input=passed no_heartbeat=passed"
                 " joined=passed frame_io=0 source_clock_1x=passed mailbox_worst_us=" << worst_us << '\n';
    std::cout << "session_io_cadence=passed intervals=" << intervals.size()
        << " median_ms=" << median_ms << " p95_ms=" << intervals[intervals.size() * 95 / 100]
        << " elapsed_s=" << poll_seconds << " stop_ms=" << stop_ms << " stop_event=passed\n";
    return 0;
  } catch (const std::exception& error) {
    block_reads = false;
    std::cerr << "session_io_probe=failed " << error.what() << '\n';
    return 1;
  }
}
