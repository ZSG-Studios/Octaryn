#include "GameUiState.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::app {
namespace {
std::string utf8(const std::filesystem::path& path) {
  const auto value=path.generic_u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}
}
GameUi::State::State(SDL_Window* window, runtime_controls& controls, LightingPanel& lighting)
    : window(window), controls(controls), lighting(lighting) { system.SetWindow(window); }
GameUi::State::~State() {
  save_inventory();
  update_profile.report();
  if (document) {
    document->RemoveEventListener("click", this);
    document->RemoveEventListener("change", this);
    document->RemoveEventListener("mousedown", this);
    document->RemoveEventListener("mouseover", this);
    for(const char* type:{"dragstart","drag","dragdrop","dragend","mousemove"})document->RemoveEventListener(type,this);
  }
  if (initialized) Rml::Shutdown();
  Rml::SetSystemInterface(nullptr);
  Rml::SetRenderInterface(nullptr);
}
GameUi::GameUi(SDL_Window* window, Rml::RenderInterface* renderer,
               const std::filesystem::path& assets, runtime_controls& controls, LightingPanel& lighting,
               const std::filesystem::path& palette)
    : state_(std::make_unique<State>(window, controls, lighting)) {
  if (!renderer) throw std::runtime_error("Missing RmlUi render interface");
  auto& s=*state_;
  const auto data=assets.parent_path().parent_path()/"Data";
  if (!s.inventory.load_catalog(data/"Blocks"/"octaryn.basegame.blocks.json",
                                data/"Items"/"octaryn.basegame.item.hand.json"))
    throw std::runtime_error("Cannot load inventory block catalog");
  s.palette_path=palette;
  if (!palette.empty() && std::filesystem::exists(palette) && !s.inventory.load(palette)) {
    s.palette_path.clear(); // Failed construction must not save defaults over the damaged file.
    throw std::runtime_error("Existing inventory could not be loaded; repair or restore it before continuing. No inventory was reset.");
  }
  if(s.inventory.reserved_drop())s.drop_request={s.inventory.cursor().block,s.inventory.reserved_drop()};
  Rml::SetSystemInterface(&s.system);
  Rml::SetRenderInterface(renderer);
  if (!Rml::Initialise()) throw std::runtime_error("RmlUi initialization failed");
  s.initialized=true;
  if (!Rml::LoadFontFace(utf8(assets/"Fonts"/"Silkscreen-Regular.ttf")))
    throw std::runtime_error("Cannot load bundled Silkscreen UI font");
  int width{},height{};
  SDL_GetWindowSizeInPixels(window,&width,&height);
  s.context=Rml::CreateContext("octaryn",{width,height});
  if (!s.context) throw std::runtime_error("RmlUi context initialization failed");
  s.document=s.context->LoadDocument(utf8(assets/"game.rml"));
  if (!s.document) throw std::runtime_error("Cannot load basegame RmlUi document");
  s.document->AddEventListener("click",&s);
  s.document->AddEventListener("change",&s);
  s.document->AddEventListener("mousedown",&s);
  s.document->AddEventListener("mouseover",&s);
  for(const char* type:{"dragstart","drag","dragdrop","dragend","mousemove"})s.document->AddEventListener(type,&s);
  s.document->Show();
  s.sync_inventory();
  s.sync_menu();
  s.sync_lighting();
  s.sync_capture();
  s.context->Update();
}
GameUi::~GameUi()=default;
Rml::Context* GameUi::context() const { return state_->context; }
bool GameUi::consume_ui_capture_request() {
  const bool requested=state_->ui_capture_requested;
  state_->ui_capture_requested=false;
  return requested;
}
void GameUi::State::text(const char* id,const std::string& value) {
  auto& previous=text_cache[id];
  if (previous==value) return;
  if (auto* element=document->GetElementById(id)) element->SetInnerRML(value);
  previous=value;
}
void GameUi::State::visible(const char* id,bool show) {
  if (auto* element=document->GetElementById(id)) element->SetClass("hidden",!show);
}
void GameUi::State::sync_capture() {
  const bool open=modal_open();
  const unsigned overlay=inventory_open?(creative_open?2u:1u):controls_open?3u:0u;
  const bool changed=lighting_was_visible!=lighting.visible || previous_screen!=controls.display_menu.screen || previous_overlay!=overlay;
  if (modal_was_open && (!open || changed))
    release_input_pending=true;
  if (open && !modal_was_open) {
    mouse_was_relative=SDL_GetWindowRelativeMouseMode(window);
    SDL_SetWindowRelativeMouseMode(window,false);
  } else if (!open && modal_was_open && (mouse_was_relative || controls.restore_relative_mouse_after_ui)) {
    SDL_SetWindowRelativeMouseMode(window,true);
    controls.restore_relative_mouse_after_ui=0;
  }
  if (open && SDL_GetWindowRelativeMouseMode(window)) {
    controls.restore_relative_mouse_after_ui=1;
    SDL_SetWindowRelativeMouseMode(window,false);
  }
  if (open && (!modal_was_open || changed)) {
    context->Update();
    const char* first_ids[]={"main-screen","world-name","server-address","display","pause-screen"};
    const char* first_id=inventory_open?(creative_open?"creative-search":"inventory-slot-0"):
        controls_open?"back-pause":lighting.visible?"ambient":first_ids[std::min(controls.display_menu.screen,4u)];
    if (auto* first=document->GetElementById(first_id)) first->Focus();
  }
  modal_was_open=open;
  lighting_was_visible=lighting.visible;
  previous_screen=controls.display_menu.screen;
  previous_overlay=overlay;
}
void GameUi::State::release_input() {
  if (!release_input_pending) return;
  release_input_pending=false;
  context->ProcessMouseLeave();
  for (int button=0;button<3;++button) context->ProcessMouseButtonUp(button,0);
  SDL_CaptureMouse(false);
}
}
