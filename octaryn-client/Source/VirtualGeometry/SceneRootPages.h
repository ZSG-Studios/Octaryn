#pragma once
#include "GeometryFormat.h"
#include <span>

namespace octaryn::client::rendering::virtual_geometry {
struct SceneRootSpan {
  std::uint32_t slot{invalid_id},offset{},bytes{},generation{};
  explicit operator bool() const {return slot!=invalid_id && generation!=0;}
  bool operator==(const SceneRootSpan&) const=default;
};
// The caller supplies completed consumer fences before releasing a packed range.
class SceneRootPages {
public:
  explicit SceneRootPages(std::uint32_t slots);
  SceneRootSpan reserve(std::uint32_t bytes);
  bool release(SceneRootSpan,std::uint64_t required,std::uint64_t completed);
  bool valid(SceneRootSpan) const;
  std::uint64_t bytes() const {return bytes_;}
  static std::uint32_t required_slots(std::span<const std::uint32_t> sizes);
private:
  struct Range {std::uint32_t offset{},bytes{},generation{};};
  std::vector<std::vector<Range>> slabs_;
  std::uint64_t bytes_{};
  std::uint32_t generation_{};
};
// Exact decoded byte extent for every page, including clusters outside root groups.
std::vector<std::uint32_t> geometry_page_payload_bytes(const GeometryAsset&);
}
