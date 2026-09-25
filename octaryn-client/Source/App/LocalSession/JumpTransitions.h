#pragma once
#include <array>
#include <cstdint>

namespace octaryn::client::app {
struct JumpTransitions {
 std::array<bool,32> pressed{};
 uint8_t count{};
 bool reset{};
};
}
