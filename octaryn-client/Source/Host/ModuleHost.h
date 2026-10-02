#pragma once

#include <cstdint>

#include "octaryn_host_api.h"

namespace octaryn::client::audio { struct ActionAudio; }
namespace octaryn::client::app { class GameUi; }
namespace octaryn::client::app { class LocalSession; class StartupWork; }

namespace octaryn::client::host {
struct GraphicsHost;

// Presentation backends the module host tables call into. Null members make
// the matching table report unavailable instead of dropping requests.
struct ModuleHostHooks {
  audio::ActionAudio* audio{};
  app::GameUi* ui{};
  GraphicsHost* graphics{};
  bool* running{};
  app::LocalSession* session{};
};

// Starts the managed module host through the client bridge. Returns 0 when
// modules are ticking, 1 when the managed bridge is not part of this build,
// and <0 when the managed host refused initialization.
int module_host_start(const ModuleHostHooks& hooks);
// Bootstrap worker only; host API calls rendezvous with the presentation owner.
int module_host_start_worker(const ModuleHostHooks& hooks,app::StartupWork&);

// Drives one module frame. Input uses the shared ABI snapshot layout.
int module_host_tick(uint64_t frame_index, double delta_seconds,
                     const octaryn_host_input_snapshot& input);

void module_host_stop();
bool module_host_active();
bool module_host_rebind(const ModuleHostHooks& hooks);

} // namespace octaryn::client::host
