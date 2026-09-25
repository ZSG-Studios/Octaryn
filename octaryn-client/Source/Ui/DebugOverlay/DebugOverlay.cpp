#include "DebugOverlay.h"
#include "RuntimeControls.h"
#include "UiData.h"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace octaryn::client::app {
namespace {

std::string utf8(const std::filesystem::path& path) {
  const auto value = path.generic_u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}

struct OverlaySystem final : SystemInterface_SDL {
  bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
    if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT || type == Rml::Log::LT_WARNING)
      std::fprintf(stderr, "rmlui severity=%s message=%s\n",
                   type == Rml::Log::LT_WARNING ? "warning" : "error", message.c_str());
    return true;
  }
};

std::string tenths(std::uint32_t value) {
  return std::to_string(value / 10u) + "." + std::to_string(value % 10u);
}

std::string hundredths(std::uint32_t value) {
  return std::to_string(value / 100u) + "." + std::to_string((value / 10u) % 10u) + std::to_string(value % 10u);
}

} // namespace

struct DebugOverlay::State {
  SDL_Window* window{};
  runtime_controls& controls;
  OverlaySystem system;
  Rml::Context* context{};
  Rml::ElementDocument* document{};
  bool initialized{};
  std::unordered_map<std::string, std::string> text_cache;

  State(SDL_Window* window, runtime_controls& controls) : window(window), controls(controls) {
    system.SetWindow(window);
  }

  ~State() {
    if (initialized) Rml::Shutdown();
    Rml::SetSystemInterface(nullptr);
    Rml::SetRenderInterface(nullptr);
  }

  void text(const char* id, const std::string& value) {
    auto& previous = text_cache[id];
    if (previous == value) return;
    if (auto* element = document->GetElementById(id)) element->SetInnerRML(value);
    previous = value;
  }
};

DebugOverlay::DebugOverlay(SDL_Window* window, Rml::RenderInterface* renderer,
                           const std::filesystem::path& assets, runtime_controls& controls)
    : state_(std::make_unique<State>(window, controls)) {
  if (!renderer) throw std::runtime_error("Missing RmlUi render interface");
  auto& s = *state_;
  Rml::SetSystemInterface(&s.system);
  Rml::SetRenderInterface(renderer);
  if (!Rml::Initialise()) throw std::runtime_error("RmlUi initialization failed");
  s.initialized = true;
  if (!Rml::LoadFontFace(utf8(assets / "Fonts" / "Silkscreen-Regular.ttf")))
    throw std::runtime_error("Cannot load bundled Silkscreen UI font");
  int width{}, height{};
  SDL_GetWindowSizeInPixels(window, &width, &height);
  s.context = Rml::CreateContext("debug", {width, height});
  if (!s.context) throw std::runtime_error("RmlUi context initialization failed");
  s.document = s.context->LoadDocument(utf8(assets / "debug.rml"));
  if (!s.document) throw std::runtime_error("Cannot load debug overlay document");
  s.document->Show();
  s.context->Update();
}

DebugOverlay::~DebugOverlay() = default;

Rml::Context* DebugOverlay::context() const { return state_->context; }

void DebugOverlay::set_stat(const char* id, const std::string& value) {
  state_->text(id, value);
}

void DebugOverlay::update(const rendering::UiDrawData& data, int width, int height) {
  auto& s = *state_;
  if (s.context->GetDimensions().x != width || s.context->GetDimensions().y != height)
    s.context->SetDimensions({width, height});
  if (auto* root = s.document->GetElementById("overlay"))
    root->SetClass("hidden", s.controls.debug_overlay_enabled == 0);
  s.text("fps", tenths(data.FPSTenths));
  s.text("frame-ms", hundredths(data.FrameTimeHundredths));
  s.text("fps-avg", tenths(data.FPSAverageTenths));
  s.text("fps-low1", tenths(data.FPSLow1Tenths));
  s.text("sim-ms", hundredths(data.SimTimeHundredths));
  s.text("render-ms", hundredths(data.RenderTimeHundredths));
  s.text("ui-ms", hundredths(data.UiTimeHundredths));
  if (data.GpuVramHundredthsGiB != UINT32_MAX)
    s.text("vram-gib", hundredths(data.GpuVramHundredthsGiB));
  s.context->Update();
}

}
