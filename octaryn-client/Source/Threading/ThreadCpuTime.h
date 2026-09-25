#pragma once
#include <cstdint>

namespace octaryn::client::threading {
// Current-thread kernel plus user CPU time; -1 when unavailable.
std::int64_t current_thread_cpu_nanoseconds() noexcept;
}
