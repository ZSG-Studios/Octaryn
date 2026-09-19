#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
// Freeze safety: bounded fence waits and a frame wall-time watchdog so a hung
// or pathologically slow GPU submission exits the session instead of freezing
// the desktop through repeated driver timeouts.
// OCTARYN_CLIENT_FENCE_TIMEOUT_MS (default 8000) bounds each fence wait.
// OCTARYN_CLIENT_FRAME_WATCHDOG_MS (default 5000, 0 disables) fails any frame
// whose CPU wall time exceeds the budget.
std::uint64_t frame_fence_timeout_ms();
std::uint64_t frame_watchdog_ms();
// Do not release resources still referenced by a nonresponsive GPU or enter
// driver destructors that wait forever. This is terminal, not device recovery.
[[noreturn]] void frame_gpu_shutdown_failed(const char* stage);
}
