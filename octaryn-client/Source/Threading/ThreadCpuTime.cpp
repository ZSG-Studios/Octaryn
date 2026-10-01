#include "ThreadCpuTime.h"
#include <limits>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace octaryn::client::threading {
std::int64_t current_thread_cpu_nanoseconds() noexcept {
#if defined(_WIN32)
  FILETIME created{},exited{},kernel{},user{};
  if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))return -1;
  const auto ticks=[](const FILETIME& value) {
    return (std::uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;
  };
  const auto kernel_ticks=ticks(kernel),user_ticks=ticks(user);
  constexpr auto limit=std::uint64_t(std::numeric_limits<std::int64_t>::max())/100;
  if(kernel_ticks>limit || user_ticks>limit-kernel_ticks)return -1;
  return std::int64_t((kernel_ticks+user_ticks)*100);
#else
  return -1;
#endif
}
}
