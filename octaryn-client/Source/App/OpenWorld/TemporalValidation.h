#pragma once
#include <cstdint>
struct SDL_Window;
struct runtime_controls;
namespace Rml {class Context;}
namespace octaryn::client::rendering {struct WorldCamera;struct WorldRendererStats;}
namespace octaryn::client::app {
// Explicit CLI qualification of production mode, resize, history, and native UI paths.
class TemporalValidation {
public:
  void begin_frame(SDL_Window*,runtime_controls&,const rendering::WorldRendererStats&,double seconds);
  void camera(rendering::WorldCamera&) const;
  void frame_rendered(const rendering::WorldRendererStats&,Rml::Context*,SDL_Window*,
      bool resident,bool rendered,bool captured,double seconds);
  bool capture_ready() const;
  bool complete() const {return complete_;}
private:
  unsigned phase_{},resident_frames_{},total_frames_{};
  std::uint64_t previous_frame_{},previous_resets_{},phase_resets_{};
  double started_{-1},phase_started_{},resident_started_{-1};
  bool requested_{},complete_{};
};
}
