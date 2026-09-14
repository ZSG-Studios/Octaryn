#include "Reference.h"
#include "TerrainDensity.h"

#include <array>
#include <bit>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string_view>

using namespace octaryn::basegame::terrain;
using namespace octaryn::client::world_presentation;
namespace {
std::uint64_t checks{};
void require(bool value,const char* message) {
  ++checks;
  if(!value)throw std::runtime_error(message);
}
bool identical(double a,double b) {return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
void validate_noise() {
  struct Channel {double xz,y;std::uint32_t id;};
  constexpr std::array channels{Channel{.022,.031,307},Channel{.036,.027,353},Channel{.031,.033,401}};
  constexpr std::array coordinates{-32000000,-1025,-1024,-33,-32,-1,0,1,31,32,33,1024,31999999};
  std::mt19937 random(9173);
  for(const auto channel:channels)for(const auto wx:coordinates) {
    const int wz=coordinates[random()%coordinates.size()];
    const double x=wx*channel.xz,z=wz*channel.xz;
    Noise3Column cache(x,z,channel.id);
    const auto compare=[&](double y) {
      const auto expected=terrain_reference::noise(x,y,z,channel.id);
      require(identical(noise3(x,y,z,channel.id),expected),"scalar noise expression changed");
      require(identical(cache.sample(y),expected),"cached noise differs bitwise");
    };
    for(int y=-256;y<256;++y)compare(y*channel.y);
    for(int y=255;y>=-256;--y)compare(y*channel.y);
    for(int i=0;i<512;++i)compare((static_cast<int>(random()%512)-256)*channel.y);
    for(int y=-9;y<=9;++y) {
      compare(std::nextafter(static_cast<double>(y),-std::numeric_limits<double>::infinity()));
      compare(y);
      compare(std::nextafter(static_cast<double>(y),std::numeric_limits<double>::infinity()));
      compare(y);compare(y);
    }
  }
  for(int i=0;i<128;++i) {
    const int x=static_cast<int>(random()%64000000)-32000000;
    const int z=static_cast<int>(random()%64000000)-32000000;
    const auto column=sample_column(x,z);
    CaveColumnSampler cache(column);
    const auto fill=classify_materials(column,terrain_reference::Materials{});
    for(int y=-257;y<=256;++y) {
      require(identical(cache.density(y),terrain_reference::density(column,y)),"cached cave density differs bitwise");
      require(sample_block_cached(cache,y,terrain_reference::Materials{},fill)==terrain_reference::block(column,y,fill),
              "cached block differs at cave/material boundary");
    }
  }
}
constexpr std::array coordinates{std::pair{0,0},std::pair{-1,-1},std::pair{1,0},std::pair{4,-3},
    std::pair{-31,27},std::pair{99,105},std::pair{-1024,1024},std::pair{-1000000,1000000}};
SnapshotColumn fixture(int x,int z) {
  return {x,z,7,{{x*32,-256,z*32,0},{x*32+31,255,z*32+31,65535},{x*32+15,-64,z*32+16,3}}};
}
std::uint64_t hash(const StreamColumn& column) {
  std::uint64_t value=1469598103934665603ull;
  for(const auto block:column.blocks){value^=block;value*=1099511628211ull;}
  return value;
}
void validate_columns() {
  for(const auto& [x,z]:coordinates) {
    const auto source=fixture(x,z);
    const auto expected=terrain_reference::generate(source,42);
    const auto actual=generate_stream_column(source,42);
    require(actual.x==x && actual.z==z && actual.epoch==42 && actual.revision==7,"column identity changed");
    require(actual.blocks==expected.blocks,"full column differs from scalar revision-2 oracle");
    require(actual.blocks.is_compact() && actual.blocks.storage_bytes()==expected.blocks.storage_bytes(),
            "lossless column representation changed");
    std::cout<<"terrain_cache_column x="<<x<<" z="<<z<<" voxels="<<actual.blocks.size()
             <<" hash="<<hash(actual)<<" exact=1\n";
  }
}
void benchmark() {
  using Clock=std::chrono::steady_clock;
  std::uint64_t checksum{};
  double scalar{},cached{};
  constexpr int rounds=5;
  const auto run=[&](bool fast,int x,int z) {
    const auto source=fixture(x,z);
    const auto start=Clock::now();
    const auto column=fast?generate_stream_column(source,42):terrain_reference::generate(source,42);
    const auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    checksum+=hash(column);
    return elapsed;
  };
  for(int round=0;round<rounds;++round)for(const auto& [x,z]:coordinates) {
    // Alternate ordering so warm caches are not assigned only to the optimized path.
    if(round%2) {cached+=run(true,x,z);scalar+=run(false,x,z);}
    else {scalar+=run(false,x,z);cached+=run(true,x,z);}
  }
  constexpr auto count=rounds*coordinates.size();
  std::cout<<"terrain_cache_benchmark columns_per_path="<<count<<" scalar_mean_ms="<<scalar/static_cast<double>(count)
           <<" cached_mean_ms="<<cached/static_cast<double>(count)<<" speedup="<<scalar/cached<<" checksum="<<checksum<<'\n';
}
}
int main(int argc,char** argv) {
  try {
    require(argc==1 || (argc==2 && std::string_view(argv[1])=="--benchmark"),"expected optional --benchmark");
    validate_noise();validate_columns();
    if(argc==2)benchmark();
    std::cout<<"terrain_cache=passed checks="<<checks<<" scalar_oracle=bit_exact full_column_voxels="
             <<coordinates.size()*32*512*32<<" seed=1337 revision=2 gpu_devices=0\n";
    return 0;
  } catch(const std::exception& error) {
    std::cerr<<"terrain_cache=failed reason="<<error.what()<<'\n';return 1;
  }
}
