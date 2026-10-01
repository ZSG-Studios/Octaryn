#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace octaryn::client::rendering::virtual_geometry {
inline constexpr std::uint32_t invalid_page = ~std::uint32_t{};
struct PageHandle {
  std::uint32_t slot{invalid_page}, generation{};
  bool operator==(const PageHandle&) const = default;
  explicit operator bool() const { return slot != invalid_page && generation != 0; }
};
struct FenceValues { std::uint64_t upload{}, raster{}, ray{}, pose{}; };
enum class PageState : std::uint32_t { Free, Reserved, Uploading, Resident, Retiring };
struct GpuPage {
  std::uint32_t slot{invalid_page}, generation{}, resident{}, reserved{};
};
static_assert(sizeof(GpuPage) == 16);
struct PageRequest { std::uint32_t page{}; float priority{}; };
struct ResidencyStats {
  std::uint64_t bytes{}, budget{}, feedback_overflow{}, invalid_feedback{}, evictions{};
  std::uint32_t resident{}, pending{}, retiring{};
};
// Owner-thread state. Fence values are monotonic timelines, one per consumer.
class PageResidency {
public:
  PageResidency(std::uint32_t pages, std::uint32_t slots, std::uint64_t byte_budget,
                std::uint32_t feedback_capacity);
  PageHandle reserve(std::uint32_t page, std::uint64_t bytes, bool pin = false);
  bool begin_upload(PageHandle, std::uint64_t upload_fence);
  bool reference(PageHandle, FenceValues);
  // GPU-reported use from the selection cut; this is the only LRU touch.
  void touch(std::uint32_t page);
  bool pin(std::uint32_t page, bool value);
  bool evict(std::uint32_t page);
  bool evict_oldest();
  void complete(FenceValues);
  // Only after every reservation/upload/retirement and consumer has completed.
  bool release_upload_timeline();
  bool resident(std::uint32_t page) const;
  bool valid(PageHandle) const;
  PageHandle handle(std::uint32_t page) const;
  std::vector<GpuPage> page_table() const;
  // Feedback is capped before allocation; repeated requests consume one slot.
  void feedback(std::span<const PageRequest>);
  void discard_feedback(std::span<const std::uint32_t> pages);
  std::vector<PageRequest> take_requests(std::uint32_t maximum);
  ResidencyStats stats() const;
private:
  struct Slot {
    std::uint32_t page{invalid_page}, generation{};
    PageState state{PageState::Free};
    bool pinned{};
    std::uint64_t bytes{}, touched{};
    FenceValues fences;
  };
  std::vector<Slot> slots_;
  std::vector<PageHandle> pages_;
  std::vector<PageRequest> requests_;
  std::uint64_t budget_{}, bytes_{}, clock_{}, overflow_{}, invalid_{}, evictions_{};
  std::uint32_t feedback_capacity_{};
  FenceValues completed_;
};
}
