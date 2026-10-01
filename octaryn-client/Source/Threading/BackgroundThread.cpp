#include "BackgroundThread.h"
#include <cstdio>
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
void set_background_thread_priority(const char* role) {
#if defined(_WIN32)
  const auto thread=GetCurrentThread();
  const bool applied=SetThreadPriority(thread,THREAD_PRIORITY_BELOW_NORMAL)!=0;
  const auto error=applied?0ul:GetLastError();
  std::printf("client_worker_priority role=%s thread=%lu requested=below_normal actual=%d status=%s error=%lu\n",
      role,GetCurrentThreadId(),GetThreadPriority(thread),applied?"applied":"failed",error);
#else
  std::printf("client_worker_priority role=%s status=platform_default\n",role);
#endif
  std::fflush(stdout);
}
}
