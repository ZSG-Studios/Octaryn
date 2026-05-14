#include "core/memory_mimalloc.h"

#include <SDL3/SDL.h>

#include <mutex>

#include <mimalloc.h>

#include "core/log.h"

namespace {

void* oct_mi_malloc(size_t size) { return mi_malloc(size); }
void* oct_mi_calloc(size_t count, size_t size) { return mi_calloc(count, size); }
void* oct_mi_realloc(void* ptr, size_t size) { return mi_realloc(ptr, size); }
void oct_mi_free(void* ptr) { mi_free(ptr); }

} // namespace

void oct_memory_init(void)
{
  static std::once_flag once;
  std::call_once(once, []() {
    if (!SDL_SetMemoryFunctions(oct_mi_malloc, oct_mi_calloc, oct_mi_realloc, oct_mi_free))
    {
      oct_log_warnf("Failed to install mimalloc-backed SDL memory functions: %s", SDL_GetError());
      return;
    }
    oct_log_infof("mimalloc-backed SDL memory functions installed (version %d)", mi_version());
  });
}
