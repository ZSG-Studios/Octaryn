#pragma once
#include <cstdint>
#include <functional>
#include <string_view>

namespace octaryn::client::app::local_session {
struct SessionChannels {
  std::function<bool(uint8_t, std::string_view)> publish;
  std::function<bool(uint64_t&, uint64_t&)> poll_action_ack;
};
}
