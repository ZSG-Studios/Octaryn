#pragma once
#include "GpuCounterProfile.h"
#ifdef _WIN32
#include <gpu_performance_api/gpu_perf_api.h>
#include <slang-com-ptr.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

namespace octaryn::client::rendering {
enum class CounterPhase {Setup,Armed,Recording,Encoded,Submitted,Complete,Failed};
struct GpuCounterProfile::State {
  struct Counter {GpaUInt32 index{};std::string name;GpaDataType type{};};
  std::atomic<unsigned> references{1};
  void* library{};
  GpaFunctionTable api;
  GpaContextId context{};
  GpaSessionId session{};
  GpaCommandListId command_list{};
  CounterPhase phase{CounterPhase::Setup};
  bool initialized{},owns_global{},session_open{},list_open{},sample_open{},queued{},closed{},explicit_names{};
  std::ofstream report;
  std::uint64_t target{240},frame{},signal{};
  std::uint64_t scene_revision{},ready_since{};
  bool was_ready{};
  unsigned polls{};
  std::chrono::steady_clock::time_point submitted_at;
  Slang::ComPtr<rhi::IFence> fence;
  std::vector<std::string> requested;
  std::vector<Counter> enabled;
  GpuCounterObservation observation;
  void retain() {references.fetch_add(1,std::memory_order_relaxed);}
  void release() {if(references.fetch_sub(1,std::memory_order_acq_rel)==1)delete this;}
  bool check(GpaStatus status,const char* operation);
  bool flush_report();
  void fail(const char* operation,GpaStatus status=kGpaStatusErrorFailed);
  void initialize(bool dx12);
  void attach(rhi::IDevice* device);
  void enumerate();
  void close();
};
std::string counter_json_string(const char* value);
}
#endif
