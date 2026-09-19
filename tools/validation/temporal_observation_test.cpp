#include "TemporalCamera.h"
#include "TemporalObservation.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
using namespace std::chrono_literals;

int main() {
  unsigned checks=0;
  auto expect=[&](bool value,const char* reason) {
    ++checks;if(!value)throw std::runtime_error(reason);
  };
  using Clock=TemporalObservation::Clock;
  const auto submitted=Clock::time_point{}+10s;
  const auto capture_start=submitted+40ms;
  const auto capture_end=capture_start+650ms;
  const auto next_frame=submitted+759ms;
  auto seconds=[&](Clock::time_point last) {
    return std::chrono::duration<double>(next_frame-last).count();
  };
  TemporalCamera history;WorldCamera eye{152.052063f,69.620003f,-233.903442f,0,-.15f,1.570796f};
  history.commit(eye,1280,720);
  expect(history.reset(eye,1280,720,seconds(submitted)),"negative control: untreated capture resets history");
  auto last=submitted;
  {
    TemporalObservation observation(last,true,capture_start);
    expect(observation.elapsed_ms(capture_end)==650,"measured diagnostic duration");
    observation.finish(capture_end);
    expect(last==submitted+650ms,"only diagnostic duration excluded");
    observation.finish(capture_end+1s);
    expect(last==submitted+650ms,"finish is idempotent");
  }
  expect(last==submitted+650ms,"destructor does not double-exclude explicit finish");
  expect(std::abs(seconds(last)-.109)<1e-12,"real 109 ms interval retained");
  expect(!history.reset(eye,1280,720,seconds(last)),"stationary history survives diagnostic observation");
  expect(history.reset(eye,1280,720,.759),"negative control: genuine 759 ms stall still resets");
  auto cut=eye;cut.x+=100;
  expect(history.reset(cut,1280,720,seconds(last)),"camera teleport still resets after observation");
  cut=eye;cut.yaw+=1.5f;
  expect(history.reset(cut,1280,720,seconds(last)),"camera rotation cut still resets");
  cut=eye;cut.vertical_fov+=.1f;
  expect(history.reset(cut,1280,720,seconds(last)),"projection cut still resets");
  expect(history.reset(eye,1920,1080,seconds(last)),"resolution cut still resets");
  last=submitted;
  {
    TemporalObservation observation(last,false,capture_start);observation.finish(capture_end);
  }
  expect(last==submitted,"disabled temporal mode is unchanged");
  last={};
  {
    TemporalObservation observation(last,true,capture_start);observation.finish(capture_end);
  }
  expect(last==Clock::time_point{},"uninitialized history remains uninitialized");
  last=submitted;
  {
    TemporalObservation observation(last,true,capture_start);observation.finish(capture_start-1ms);
  }
  expect(last==submitted,"negative observer duration cannot age history");
  // A real 300 ms fence wait occurs BEFORE observation and must remain in delta.
  last=submitted;
  {
    TemporalObservation observation(last,true,submitted+300ms);observation.finish(submitted+950ms);
  }
  expect(history.reset(eye,1280,720,std::chrono::duration<double>(submitted+970ms-last).count()),
      "negative control: pre-observation GPU fence stall still resets");
  // Bounded 32-capture run: four expensive buffer dumps, then image-only captures.
  last=submitted;
  for(unsigned frame=0;frame<32;++frame) {
    auto start=last+40ms;auto duration=frame<4?650ms:100ms;
    const auto next=start+duration+16ms;
    {
      TemporalObservation observation(last,true,start);observation.finish(start+duration);
    }
    expect(!history.reset(eye,1280,720,std::chrono::duration<double>(next-last).count()),
        "repeated captures retain otherwise valid history");
    last=next;history.commit(eye,1280,720);
  }
  // Exercise production RAII completion itself without waiting on wall time.
  last=submitted;
  {
    TemporalObservation observation(last,true,Clock::now()-650ms);
  }
  expect(last>=submitted+650ms,"RAII completion accounts for observation scope");
  std::printf("temporal_observation=passed checks=%u negative_controls=3 simulated_captures=32 production_camera=1 gpu_devices=0\n",checks);
}
