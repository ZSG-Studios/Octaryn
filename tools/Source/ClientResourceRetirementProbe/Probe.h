#pragma once
#include <stdexcept>

namespace retirement_probe {
inline void require(bool condition,const char* message) {
  if(!condition)throw std::runtime_error(message);
}
void worker_cases();
void descriptor_cases();
void progress_cases();
}
