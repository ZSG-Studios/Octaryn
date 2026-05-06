#include "Clock.h"

#include <glaze/glaze.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace octaryn::server::world::time {

struct world_time_intent_file {
  int32_t version = 1;
  int32_t speedIndex = 2;
  double speedMultiplier = 1.0;
};

} // namespace octaryn::server::world::time

namespace {

constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};

enum world_time_intent_process_plan_reason : uint32_t {
  world_time_intent_process_plan_reason_none = 0u,
  world_time_intent_process_plan_reason_missing_intent = 1u,
  world_time_intent_process_plan_reason_invalid_intent = 2u,
  world_time_intent_process_plan_reason_unsupported_intent = 3u,
};

using octaryn::server::world::time::world_time_intent_file;

bool read_text_file(const std::filesystem::path &path, std::string &text) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }

  text.assign(std::istreambuf_iterator<char>{input},
              std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

bool is_supported(const world_time_intent_file &file) {
  return file.version == 1 && std::isfinite(file.speedMultiplier) &&
         file.speedMultiplier >= 0.0 && file.speedMultiplier <= 24000.0;
}

octaryn_server_world_time_intent to_native_intent(
    const world_time_intent_file &file) {
  return octaryn_server_world_time_intent{
      .version = file.version,
      .speed_index = file.speedIndex,
      .speed_multiplier = file.speedMultiplier,
  };
}

const char *intent_process_reason_name(uint32_t reason) {
  switch (reason) {
  case world_time_intent_process_plan_reason_missing_intent:
    return "missing_intent";
  case world_time_intent_process_plan_reason_unsupported_intent:
    return "unsupported_intent";
  case world_time_intent_process_plan_reason_invalid_intent:
  default:
    return "invalid_intent";
  }
}

} // namespace

extern "C" {

int32_t octaryn_server_world_time_read_intent_file(
    const char *intent_path, octaryn_server_world_time_intent *intent) {
  if (intent_path == nullptr || intent_path[0] == '\0' || intent == nullptr) {
    return -1;
  }

  const std::filesystem::path path{intent_path};
  if (!std::filesystem::exists(path)) {
    return 1;
  }

  std::string payload;
  if (!read_text_file(path, payload)) {
    return -2;
  }

  world_time_intent_file file{};
  if (glz::read<JsonReadOptions>(file, payload)) {
    return -3;
  }

  if (!is_supported(file)) {
    return -4;
  }

  *intent = to_native_intent(file);
  return 0;
}

int32_t octaryn_server_world_time_plan_intent(
    int32_t intent_read_result,
    const octaryn_server_world_time_intent *intent,
    octaryn_server_world_time_intent_process_plan *plan) {
  if (plan == nullptr) {
    return -1;
  }

  if (intent_read_result == 1) {
    *plan = octaryn_server_world_time_intent_process_plan{
        .should_apply = 0u,
        .reason = world_time_intent_process_plan_reason_missing_intent,
    };
    return 0;
  }
  if (intent_read_result == -4) {
    *plan = octaryn_server_world_time_intent_process_plan{
        .should_apply = 0u,
        .reason = world_time_intent_process_plan_reason_unsupported_intent,
    };
    return 0;
  }
  if (intent_read_result != 0 || intent == nullptr) {
    *plan = octaryn_server_world_time_intent_process_plan{
        .should_apply = 0u,
        .reason = world_time_intent_process_plan_reason_invalid_intent,
    };
    return 0;
  }

  *plan = octaryn_server_world_time_intent_process_plan{
      .should_apply = 1u,
      .reason = world_time_intent_process_plan_reason_none,
  };
  return 0;
}

const char *
octaryn_server_world_time_intent_process_reason_name(uint32_t reason) {
  return intent_process_reason_name(reason);
}

}
