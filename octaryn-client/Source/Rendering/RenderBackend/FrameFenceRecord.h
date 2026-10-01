#pragma once
#include <array>
#include <cstdint>
namespace octaryn::client::rendering {
struct FrameFenceRecord {
  static constexpr std::uint64_t Unknown=UINT64_MAX;
  unsigned slot{};
  std::uint64_t value{},source_frame{Unknown},before{Unknown},after{Unknown};
  std::array<std::uint64_t,4> times{};
  bool waited{},success{};
  std::int64_t result{};
};
}
