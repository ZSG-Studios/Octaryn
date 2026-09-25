#pragma once

#include <cstdio>
#include <string_view>

namespace octaryn::tools::server_world_persistence_probe {

template <typename Actual, typename Expected>
bool expect_equal(std::string_view label, Actual actual, Expected expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

} // namespace octaryn::tools::server_world_persistence_probe
