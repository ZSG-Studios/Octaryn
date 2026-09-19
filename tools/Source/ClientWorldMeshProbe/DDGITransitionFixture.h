#pragma once
#include "Probe.h"
#include <chrono>
#include <filesystem>
#include <fstream>

namespace mesh_probe::transition {
using Clock=std::chrono::steady_clock;
using RGB=std::array<double,3>;
constexpr unsigned Receivers=12,CohortSize=48;
inline double seconds(Clock::time_point start) {return std::chrono::duration<double>(Clock::now()-start).count();}
inline double energy(const RGB& rgb) {return (rgb[0]+rgb[1]+rgb[2])/3;}
struct Cohort {
  RGB raw{},published{};
  unsigned valid{},refreshed{},control_mismatches{},pending_removal{},minimum_trace{};
  double oldest_trace_seconds{};
};
struct Sample {
  double elapsed{},day{},total_seconds{};
  bool source{};
  std::array<RGB,3> receiver{}; // Hierarchy, fine, coarse.
  std::array<double,3> minimum_coverage{1,1,1};
  std::array<std::array<RGB,Receivers>,3> pixels{};
  std::array<Cohort,2> cohort{}; // Fine, coarse.
};
struct Phase {
  std::string name,kind="steady",from,target;
  bool strict{};
  std::vector<Sample> samples;
};
class Fixture {
  mesh_probe::Fixture& f;
  WorldRenderer& r;
  std::uint64_t serial{};
  Clock::time_point epoch=Clock::now();
  std::array<std::vector<double>,2> completion{{{0},{0}}};
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::IBuffer> positions,normals,results;
  std::ofstream csv,receiver_csv;
  Cohort read_cohort(DDGISystem&,unsigned,std::uint64_t);
public:
  std::filesystem::path directory;
  std::string backend;
  explicit Fixture(mesh_probe::Fixture&);
  void scene(double day);
  void publish(StreamColumn&);
  void frame();
  Sample read(const std::string&,double,double,bool,const std::array<std::uint64_t,2>&);
  std::array<std::uint64_t,2> frames() const;
  void check() const;
};
unsigned material(const mesh_probe::Fixture&,const char*);
RGB tail(const Phase&,unsigned channel=0);
bool report(const std::filesystem::path&,const std::string&,const std::vector<Phase>&);
}
