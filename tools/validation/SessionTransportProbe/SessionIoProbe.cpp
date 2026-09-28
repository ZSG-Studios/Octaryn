#include "SessionIo.h"
#include "SessionFiles.h"
#include <glaze/glaze.hpp>
#include <chrono>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace octaryn::client::app::local_session;
struct Actions { int version{}; uint64_t epoch{}, seq{}; std::vector<std::string> actions; };
struct Event { uint64_t seq{}, id{}, kind{}, p1{}, p2{}; };
struct Events { int version{1}; std::vector<Event> events; };
struct Ack { uint64_t epoch{}, seq{}; };

void require(bool condition, const char* label) {
  if (!condition) throw std::runtime_error(label);
}
template <typename Predicate> void until(Predicate predicate, const char* label) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < end) {
    if (predicate()) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  throw std::runtime_error(label);
}

void probe_channels(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::mutex mutex;
  Actions journal;
  Ack acknowledgement;
  SessionChannels channels{
    [&](uint8_t kind, std::string_view payload) {
      if (kind == 5) {
        std::lock_guard lock(mutex);
        require(!glz::read_json(journal, payload), "invalid typed action journal");
      }
      return true;
    },
    [&](uint64_t& epoch, uint64_t& sequence) {
      std::lock_guard lock(mutex);
      epoch = acknowledgement.epoch; sequence = acknowledgement.seq;
      return true;
    }};
  SessionIo io(root / "pose.json", root / "input.json", root / "window.json", false, channels);
  for (unsigned i = 0; i < 256; ++i)
    require(io.publish_ui_action("typed." + std::to_string(i)), "typed capacity rejected");
  require(!io.publish_ui_action("overflow"), "typed overflow admitted");
  until([&] {
    std::lock_guard lock(mutex);
    return journal.seq == 256 && journal.actions.size() == 256;
  }, "typed journal not delivered");
  {
    std::lock_guard lock(mutex);
    acknowledgement = {journal.epoch + 1, 256};
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  require(!io.publish_ui_action("stale"), "typed stale epoch cleared queue");
  {
    std::lock_guard lock(mutex);
    acknowledgement = {journal.epoch, 128};
  }
  until([&] {
    std::lock_guard lock(mutex);
    return journal.actions.size() == 128 && journal.actions.front() == "typed.128";
  }, "typed partial acknowledgement violated ordering");
  require(io.publish_ui_action("typed.256"), "typed capacity not returned");
  io.stop();
  require(std::filesystem::is_empty(root), "in-memory session created mailbox files");
}

int main(int argc, char** argv) {
  try {
    require(argc == 2, "expected evidence directory");
    const auto root = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(root);
    SessionIo io(root / "player_state.json", root / "player_input.json", root / "chunk_view.json");
    Actions journal;
    const auto read_actions = [&] {
      std::string text;
      return read_text(root / "ui_action.json", text, 65536) && !glz::read_json(journal, text);
    };
    for (unsigned i = 0; i < 256; ++i)
      require(io.publish_ui_action("action." + std::to_string(i)), "valid bounded action was refused");
    require(!io.publish_ui_action("overflow"), "action overflow silently accepted");
    until([&] { return read_actions() && journal.seq == 256 && journal.actions.size() == 256; }, "action journal did not publish");
    const auto epoch = journal.epoch;
    const auto acknowledge = [&](uint64_t ack_epoch, uint64_t seq) {
      std::string text;
      require(!glz::write_json(Ack{ack_epoch, seq}, text), "ACK serialization");
      require(write_text(root / "ui_action.json.ack", text), "ACK write");
    };
    acknowledge(epoch + 1, 256);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    require(!io.publish_ui_action("stale.ack"), "old epoch ACK removed current actions");
    require(read_actions() && journal.actions.size() == 256, "old epoch ACK changed journal");
    acknowledge(epoch, 128);
    until([&] { return read_actions() && journal.actions.size() == 128; }, "partial ACK not consumed");
    require(journal.actions.front() == "action.128", "partial ACK removed wrong prefix");
    require(io.publish_ui_action("action.256"), "capacity not reusable after ACK");
    until([&] { return read_actions() && journal.seq == 257 && journal.actions.size() == 129; }, "suffix publication failed");
    acknowledge(epoch, 257);
    until([&] { return read_actions() && journal.actions.empty(); }, "full ACK not consumed");

    Events batch;
    for (uint64_t i = 1; i <= 256; ++i)
      batch.events.push_back({i, i, std::numeric_limits<uint64_t>::max() - 1,
                             std::numeric_limits<uint64_t>::max() - 2, std::numeric_limits<uint64_t>::max() - 3});
    std::string text;
    require(!glz::write_json(batch, text) && text.size() > 16384 && text.size() < 65536, "event read-bound regression setup");
    require(write_text(root / "module_events.json", text), "event publication");
    uint64_t count = 0;
    until([&] {
      uint64_t id{}, kind{}, p1{}, p2{};
      while (io.poll_module_event(id, kind, p1, p2)) {
        require(id == ++count && p2 == std::numeric_limits<uint64_t>::max() - 3, "event order or payload corruption");
      }
      return count == 256;
    }, "full event journal not delivered");
    until([&] {
      std::string ack;
      return read_text(root / "module_events.json.ack", ack) && ack == "256";
    }, "consumer event ACK missing");
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    uint64_t id{}, kind{}, p1{}, p2{};
    require(!io.poll_module_event(id, kind, p1, p2), "unchanged event journal replayed");
    io.stop();
    probe_channels(root / "typed");
    require(write_text(root / "result.json", "{\"status\":\"passed\",\"actions\":257,\"events\":256,\"staleEpochRejected\":true}"), "result write");
    std::cout << "session_io=passed actions=257 events=256 stale_epoch_rejected=1\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "session_io=failed reason=" << error.what() << '\n';
    return 1;
  }
}
