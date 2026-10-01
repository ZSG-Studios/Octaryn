#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace octaryn::scene_geometry {
using Bounds=std::array<float,6>;
struct Part {
  std::uint32_t mesh{},primitive{};
  std::uint64_t first_triangle{},triangle_count{},reservation_bytes{};
  Bounds bounds{};
  bool cooked{},bounds_prepared{};
};
struct Instance {
  std::uint32_t node{},mesh{};
  std::array<float,16> transform{};
  Bounds bounds{};
};
struct Query {
  std::array<float,3> camera{},actor{};
  float load_radius{128},keep_radius{160},actor_radius{24};
  std::uint64_t budget_bytes{512ull*1024*1024};
  bool ignore_vertical{};
  // Exact world-space swept/ray envelope. Overrides camera/actor sphere queries.
  std::optional<Bounds> region,keep_region;
};
struct Selection {
  std::uint32_t part{};
  // Original catalog instance indices; geometry is owned once per part.
  std::vector<std::uint32_t> instances;
  bool operator==(const Selection&) const=default;
};
struct Plan {
  std::vector<Selection> wanted,retained;
  std::vector<std::uint32_t> pending_parts;
  std::uint64_t reservation_bytes{};
  bool admitted{};
  std::string error;
};
class ResidencyIndex {
public:
  ResidencyIndex();
  ~ResidencyIndex();
  ResidencyIndex(ResidencyIndex&&) noexcept;
  ResidencyIndex& operator=(ResidencyIndex&&) noexcept;
  bool reset(std::span<const Part>,std::span<const Instance>,std::string& error);
  Plan plan(const Query&,std::span<const Selection> current={}) const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
