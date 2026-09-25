#include "Probe.h"
#include <cstdio>
#include <exception>

int main() {
  try {
    retirement_probe::worker_cases();
    retirement_probe::descriptor_cases();
    retirement_probe::progress_cases();
    std::puts("resource_retirement_cpu=passed gpu_runtime=0");
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"resource_retirement_cpu=failed reason=%s\n",error.what());
    return 1;
  }
}
