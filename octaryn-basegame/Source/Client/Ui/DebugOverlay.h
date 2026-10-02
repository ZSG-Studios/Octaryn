#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

struct SDL_Window;
union SDL_Event;
struct runtime_controls;
namespace Rml { class Context; class RenderInterface; }
namespace octaryn::client::rendering { struct UiDrawData; }

namespace octaryn::client::app {

// Minimal RmlUi debug overlay: fps, frame timing, render resolution and
// renderer status, toggled with the debug overlay hotkey. The only UI surface
// in the engine; product UI belongs to games built on it.
class DebugOverlay {
public:
  DebugOverlay(SDL_Window* window, Rml::RenderInterface* renderer,
               const std::filesystem::path& assets, runtime_controls& controls);
  ~DebugOverlay();
  DebugOverlay(const DebugOverlay&) = delete;
  DebugOverlay& operator=(const DebugOverlay&) = delete;

  void update(const rendering::UiDrawData& data, int width, int height);
  void set_stat(const char* id, const std::string& value);
  Rml::Context* context() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};

}
