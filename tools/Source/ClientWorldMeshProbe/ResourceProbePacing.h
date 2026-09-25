#pragma once
#include "Probe.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <thread>
namespace mesh_probe::resource_probe {
using Clock=std::chrono::steady_clock;
struct Pacing {
  std::ofstream csv;
  Clock::time_point previous{},next_admission{};
  double sleep_ms{};
  std::uint64_t frames{};
  bool started{};
};
inline Pacing& pacing() {static Pacing value;return value;}
inline bool enabled() {
  const auto* value=std::getenv("OCTARYN_CLIENT_RESOURCE_PROBE_PACING");
  return value && std::strcmp(value,"1")==0;
}
inline void start() {
  if(!enabled())return;
  auto& p=pacing();require(!p.started,"resource probe heartbeat restarted");
  p.csv.open("frame-timing.csv",std::ios::trunc);
  p.csv<<"frame,total_ms,work_ms,cap_sleep_ms,phase\n";p.csv.flush();
  require(bool(p.csv),"resource probe heartbeat initialization");
  p.previous=p.next_admission=Clock::now();p.started=true;
  std::puts("resource_probe_pacing=started cap_fps=30 completed_work=1");
}
inline void sleep_until(Clock::time_point deadline) {
  auto& p=pacing();const auto before=Clock::now();
  if(before>=deadline)return;
  std::this_thread::sleep_until(deadline);
  p.sleep_ms+=std::chrono::duration<double,std::milli>(Clock::now()-before).count();
}
inline void admit() {
  auto& p=pacing();if(!p.started)return;
  sleep_until(p.next_admission);
  p.next_admission=Clock::now()+std::chrono::nanoseconds(33333334);
}
// Call only after the associated real work/fence/readback completes.
inline void complete(const char* phase) {
  auto& p=pacing();if(!p.started)return;
  sleep_until(p.previous+std::chrono::nanoseconds(33333334));
  const auto now=Clock::now();
  const auto total=std::chrono::duration<double,std::milli>(now-p.previous).count();
  p.csv<<p.frames++<<','<<total<<','<<std::max(0.0,total-p.sleep_ms)<<','<<p.sleep_ms<<','<<phase<<'\n';
  p.csv.flush();require(bool(p.csv),"resource probe completed-work heartbeat write");
  p.previous=now;p.sleep_ms=0;
}
class Frame {
  const char* phase;
  int exceptions=std::uncaught_exceptions();
public:
  explicit Frame(const char* value):phase(value) {admit();}
  ~Frame() noexcept(false) {if(std::uncaught_exceptions()==exceptions)complete(phase);}
};
}
