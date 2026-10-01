#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>

namespace octaryn::client::app::local_session {
// Owner-thread measurements; timestamps are local monotonic input generation
// to first observed acknowledgement, independent of server wall clock.
class PredictionProfile {
  using Clock=std::chrono::steady_clock;
  struct InputTime {std::uint64_t sequence{};Clock::time_point at{};};
  std::array<InputTime,128> inputs_{};
  std::ofstream output_;
  unsigned rows_{};
  bool attempted_{};
  void open() {
    if(attempted_)return;
    attempted_=true;
    if(const char* path=std::getenv("OCTARYN_CLIENT_PREDICTION_PROFILE");path && *path) {
      output_.open(path);
      output_<<"schema,ack,server_tick,input_ack_ms,pending,replayed,correction_m,authority_x,authority_y,authority_z\n";
    }
  }
public:
  void generated(std::uint64_t sequence) {
    open();
    if(output_) inputs_[sequence%inputs_.size()]={sequence,Clock::now()};
  }
  void acknowledged(std::uint64_t sequence,std::uint64_t tick,std::size_t pending,
      std::uint64_t replays,float correction,float x,float y,float z) {
    if(!output_)return;
    const auto& input=inputs_[sequence%inputs_.size()];
    const double latency=input.sequence==sequence
        ? std::chrono::duration<double,std::milli>(Clock::now()-input.at).count():-1.;
    output_<<1<<','<<sequence<<','<<tick<<','<<latency<<','<<pending<<','<<replays<<','
        <<correction<<','<<x<<','<<y<<','<<z<<'\n';
    if(++rows_%60==0)output_.flush();
  }
};
}
