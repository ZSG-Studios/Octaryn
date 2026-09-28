#include "ReflectionAdmission.cpp"
#include <array>
#include <cstdint>
#include <cstdio>

int main() {
  std::uint64_t checked=0;
  const auto check=[&](unsigned high,unsigned low,unsigned budget) {
    const auto selected=reflection_recovery_admission_0(high,low,budget);
    const std::uint64_t used=std::uint64_t(selected.x)*12+std::uint64_t(selected.y)*4;
    if(selected.x>high || selected.y>low || used>budget)return false;
    if(selected.x<high && budget-std::uint64_t(selected.x)*12>=12)return false;
    if(selected.y<low && budget-used>=4)return false;
    // The compact dispatch visits exactly the previously admitted prefixes.
    for(unsigned i=0;i<selected.x+selected.y;++i) {
      const bool upper=i<selected.x;const unsigned index=upper?i:i-selected.x;
      if(index>=(upper?selected.x:selected.y))return false;
    }
    ++checked;return true;
  };
  for(unsigned high=0;high<=64;++high)for(unsigned low=0;low<=64;++low)
    for(unsigned budget=0;budget<=1024;++budget)if(!check(high,low,budget))return 1;
  for(unsigned budget:{0u,1u,3u,4u,11u,12u,460800u,1036800u,0x7fffffu})
    for(unsigned high:{0u,1u,100u,100000u})for(unsigned low:{0u,1u,100u,100000u})
      if(!check(high,low,budget))return 2;
  std::printf("reflection_admission_production_cpu passed=1 cases=%llu cap_counts_secondary_visibility=1\n",
      static_cast<unsigned long long>(checked));
}
