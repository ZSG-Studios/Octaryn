#include "FrameMetrics.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>

int main() {
  try {
    frame_metrics metrics{};
    frame_metrics_init(&metrics);
    frame_metrics_record(&metrics,100,1);
    frame_metrics_record(&metrics,80,4000000001ull);
    if(metrics.sample_count!=0)throw std::runtime_error("ordinary startup included warmup frames");
    frame_metrics_record(&metrics,10,5000000001ull);
    if(metrics.sample_count!=1)throw std::runtime_error("ordinary startup omitted first warmed frame");
    frame_metrics_begin_measurement(&metrics);
    const auto cleared=frame_metrics_snapshot_value(&metrics,1);
    if(cleared.sample_count || !cleared.warmup_complete)
      throw std::runtime_error("explicit measurement failed to clear history or reapplied warmup");
    frame_metrics_record(&metrics,4,1);
    frame_metrics_record(&metrics,8,1000000001ull);
    const auto measured=frame_metrics_snapshot_value(&metrics,1000000001ull);
    if(measured.sample_count!=2 || std::abs(measured.average.ms-6)>1e-6f || measured.worst.ms!=8)
      throw std::runtime_error("explicit interval omitted initial samples or retained old samples");
    frame_metrics_init(&metrics);
    frame_metrics_record(&metrics,1,1);
    if(metrics.sample_count || frame_metrics_snapshot_value(&metrics,1).warmup_complete)
      throw std::runtime_error("ordinary initialization did not restore warmup");
    std::puts("frame_metrics=passed startup_warmup=1 explicit_interval=complete reset=1");
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"frame_metrics=failed reason=%s\n",error.what());return 1;
  }
}
