#include "TerrainGeneration.h"
#include "TerrainDensity.h"
#include <array>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using Rules=OctarynServerTerrainMaterialRules;
using namespace octaryn::basegame::terrain;
constexpr std::array<Rules,3> variants{{
    {30,14,3,1,2,5,4,2}, {-100,701,313,119,223,557,419,2}, {255,907,631,439,227,823,521,2}}};
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
#if defined(_MSC_VER)
#define CACHE_NOINLINE __declspec(noinline)
#else
#define CACHE_NOINLINE __attribute__((noinline))
#endif
// Deliberately preserve the pre-cache scalar operation. A separate non-inlined
// function and varying runtime inputs keep column sampling inside each call.
CACHE_NOINLINE uint16_t uncached(int32_t x,int32_t y,int32_t z,const Rules& rules) {
  return sample_block(sample_column(x,z),y,rules);
}
uint16_t cached(int32_t x,int32_t y,int32_t z,const Rules& rules) {
  uint16_t block=65535;
  require(octaryn_server_terrain_generated_block(x,y,z,&rules,&block)==0,"cached C ABI sampling failed");
  return block;
}
void plan(int32_t x,int32_t z,const Rules& rules) {
  const auto sample=sample_column(x,z);const auto materials=classify_materials(sample,rules);
  OctarynServerTerrainColumnPlan value{};
  require(octaryn_server_terrain_plan_column(x,z,&rules,&value)==0,"cached column plan failed");
  require(value.world_x==x && value.world_z==z && value.local_x==sample.local_x && value.local_z==sample.local_z &&
      value.local_width==32 && value.local_depth==32 && value.terrain_height==sample.terrain_height &&
      value.decoration_y==std::max(sample.terrain_height,rules.water_height) &&
      value.surface_block==materials.surface_block && value.fill_block==materials.fill_block &&
      value.is_lowland==static_cast<uint32_t>(sample.is_lowland) &&
      value.has_grass_surface==static_cast<uint32_t>(materials.has_grass_surface),
      "cached plan differs from uncached geometry/current material rules");
}
uint64_t full_height() {
  constexpr int32_t low=std::numeric_limits<int32_t>::min(),high=std::numeric_limits<int32_t>::max();
  constexpr std::array<std::pair<int32_t,int32_t>,18> coordinates{{
      {low,low},{high,high},{low,high},{high,low},{-32000000,32000000},{-1000000,1000000},
      {-33,-33},{-32,-32},{-31,-31},{-1,-1},{0,0},{1,1},{31,31},{32,32},{33,33},
      {256,0},{-256,0},{99,-105}}};
  uint64_t checked=0;
  for(const auto& [x,z]:coordinates) {
    for(const auto& rules:variants) plan(x,z,rules);
    for(int y=WorldMinY;y<WorldMaxYExclusive;++y) for(const auto& rules:variants) {
      require(cached(x,y,z,rules)==uncached(x,y,z,rules),"full-height cached/uncached terrain mismatch");++checked;
    }
    for(int y:{std::numeric_limits<int>::min(),WorldMinY-1,WorldMaxYExclusive,std::numeric_limits<int>::max()})
      for(const auto& rules:variants) {
        require(cached(x,y,z,rules)==AirBlock,"out-of-world cache lookup must return air");++checked;
      }
  }
  // x+256 shares a direct-mapped slot. Alternate on every read, including rule
  // changes, rather than only revisiting after a complete column was consumed.
  for(int y=WorldMinY;y<WorldMaxYExclusive;++y) for(int x:{-1,255,-257,511}) {
    const auto& rules=variants[static_cast<size_t>(y-WorldMinY)%variants.size()];
    require(cached(x,y,-33,rules)==uncached(x,y,-33,rules),"colliding cache slots changed terrain");++checked;
  }
  return checked;
}
void concurrent_callers() {
  constexpr size_t count=4;std::barrier start(static_cast<ptrdiff_t>(count));
  std::array<std::exception_ptr,count> errors{};std::vector<std::jthread> threads;
  for(size_t worker=0;worker<count;++worker) threads.emplace_back([&,worker] {
    start.arrive_and_wait();
    try {
      for(int i=0;i<512;++i) {
        const auto x=static_cast<int32_t>(worker)*256+(i%3)*256-513;
        const auto z=-33+(i%2)*256;const auto y=i-256;
        const auto& rules=variants[(worker+static_cast<size_t>(i))%variants.size()];
        require(cached(x,y,z,rules)==uncached(x,y,z,rules),"concurrent cache callers disagree with oracle");
        if(i%64==0)plan(x,z,rules);
      }
    } catch(...) {errors[worker]=std::current_exception();}
  });
  threads.clear();
  for(const auto& error:errors)if(error)std::rethrow_exception(error);
}
struct Sample {int32_t x,y,z;uint32_t rule;};
struct Measurement {double ms;uint64_t checksum;};
Measurement measure(const std::vector<Sample>& work,bool use_cache) {
  uint64_t checksum=1469598103934665603ull;
  const auto begin=std::chrono::steady_clock::now();
  for(const auto& p:work) {
    const auto value=use_cache?cached(p.x,p.y,p.z,variants[p.rule]):uncached(p.x,p.y,p.z,variants[p.rule]);
    checksum=(checksum^value)*1099511628211ull;
  }
  return {std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count(),checksum};
}
void benchmark() {
  std::vector<Sample> work;work.reserve(32768);
  for(unsigned mode=0;mode<3;++mode) {
    work.clear();
    if(mode==0) {
      for(int z=-1;z<=0;++z) for(int y=WorldMinY;y<WorldMaxYExclusive;++y) for(int x=-16;x<16;++x)
        work.push_back({x,y,z,static_cast<uint32_t>((y-WorldMinY)%3)});
    } else if(mode==1) {
      for(int pass=0;pass<256;++pass) for(int z=-4;z<=4;++z) for(int x=-4;x<=4;++x)
        work.push_back({x,20+pass%16,z,static_cast<uint32_t>(pass%3)});
    } else {
      for(int i=0;i<32768;++i)work.push_back({i*256-4194304,i%512-256,-33,static_cast<uint32_t>(i%3)});
    }
    // Warm code paths, then alternate measurement order in one process. This is
    // CPU scalar sampling cost, not an end-to-end server or FPS benchmark.
    const auto warm=measure(work,true);uint64_t expected=warm.checksum;
    double cached_ms=0,uncached_ms=0;
    for(unsigned pass=0;pass<4;++pass) {
      const bool first_cached=(pass%2)==0;
      const auto first=measure(work,first_cached),second=measure(work,!first_cached);
      require(first.checksum==expected && second.checksum==expected,"benchmark must perform equal observable work");
      cached_ms+=first_cached?first.ms:second.ms;uncached_ms+=first_cached?second.ms:first.ms;
    }
    constexpr const char* names[]={"vertical_scan","fluid_neighborhood","collision_misses"};
    std::printf("terrain_cache_benchmark traversal=%s samples=%zu passes=4 cached_mean_ms=%.6f uncached_mean_ms=%.6f checksum=%llu\n",
        names[mode],work.size(),cached_ms/4,uncached_ms/4,static_cast<unsigned long long>(expected));
  }
}
}
bool validate_column_cache() {
  try {
    const auto samples=full_height();concurrent_callers();benchmark();
    std::printf("terrain_column_cache=passed parity_samples=%llu thread_samples=2048 rules=3 full_y=512 collision=passed\n",
        static_cast<unsigned long long>(samples));return true;
  } catch(const std::exception& error) {std::fprintf(stderr,"terrain_column_cache failed: %s\n",error.what());return false;}
}
