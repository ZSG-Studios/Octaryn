#pragma once

#include <cstdlib>

namespace octaryn::tools {

inline bool set_process_environment(const char *name, const char *value) {
#if defined(_WIN32)
  return _putenv_s(name, value) == 0;
#else
  return setenv(name, value, 1) == 0;
#endif
}

} // namespace octaryn::tools
