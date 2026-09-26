#include "MenuStack.h"
#include "RmlRuntime.h"
#include "RuntimeControls.h"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace octaryn::client::app {
namespace {

std::string utf8(const std::filesystem::path& path) {
    const auto value = path.generic_u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

struct MenuSystem final : SystemInterface_SDL {
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT || type == Rml::Log::LT_WARNING)
            std::fprintf(stderr, "menu rmlui severity=%s message=%s\n",
                         type == Rml::Log::LT_WARNING ? "warning" : "error", message.c_str());
        return true;
    }
};

} // namespace

// Dispatches button clicks and input changes by element attribute so one
// listener serves the whole document.
class MenuListener final : public Rml::EventListener {
public:
    struct StateRef {
        virtual ~StateRef() = default;
        virtual void apply_setting(Rml::Element* input) = 0;
        virtual void handle_action(const std::string& action, Rml::Element* element) = 0;
    };

    explicit MenuListener(StateRef& state) : state_(state) {}

    void ProcessEvent(Rml::Event& event) override {
        auto* element = event.GetTargetElement();
        while (element && element->GetTagName() != "button" && element->GetTagName() != "input")
            element = element->GetParentNode();
        if (!element) return;
        const std::string tag = element->GetTagName().c_str();
        if (tag == "input" && event == "change") {
            state_.apply_setting(element);
            return;
        }
        if (tag != "button" || !(event == "click")) return;
        const std::string action = element->GetAttribute<Rml::String>("action", "").c_str();
        if (action.empty()) return;
        state_.handle_action(action, element);
    }

private:
    StateRef& state_;
};

struct MenuStack::State final : public MenuListener::StateRef {
    RmlRuntimeHandle rml;
    SDL_Window* window{};
    runtime_controls& controls;
    MenuActions actions;
    MenuSystem system;
    Rml::Context* context{};
    Rml::ElementDocument* document{};
    std::vector<MenuWorldEntry> worlds;
    std::string active_screen{"screen-main"};
    bool pause_visible_{};
    bool text_input_active_{};
    std::unique_ptr<MenuListener> listener;

    State(SDL_Window* window, runtime_controls& controls, MenuActions actions)
        : window(window), controls(controls), actions(std::move(actions)) {
        system.SetWindow(window);
    }

    void show_screen(const char* id) {
        for (const char* screen : {"screen-main", "screen-worlds", "screen-connect",
                                   "screen-settings", "screen-pause"}) {
            if (auto* element = document->GetElementById(screen))
                element->SetClass("active", std::string(screen) == id);
        }
        active_screen = id;
        sync_settings();
    }

    void set_text_input(bool enabled) {
        if (enabled == text_input_active_) return;
        text_input_active_ = enabled;
        if (enabled) SDL_StartTextInput(window);
        else SDL_StopTextInput(window);
    }

    std::string input_value(const char* id) const {
        if (auto* element = document->GetElementById(id))
            return element->GetAttribute<Rml::String>("value", "").c_str();
        return {};
    }

    void set_status(const char* id, const std::string& text) {
        if (auto* element = document->GetElementById(id)) element->SetInnerRML(text);
    }

    void refresh_worlds() {
        auto* host = document->GetElementById("worlds");
        if (!host) return;
        std::string markup;
        for (size_t index = 0; index < worlds.size(); ++index) {
            const auto& world = worlds[index];
            markup += "<button action=\"world\" data-index=\"" + std::to_string(index) + "\">"
                "<span class=\"world-name\">" + world.name + "</span>"
                "<span class=\"world-path\">" + utf8(world.glb) + "</span></button>";
        }
        if (worlds.empty())
            markup = "<div class=\"status\">No worlds found. Place a GLB and map.json in Client/Assets/Maps/&lt;name&gt;/.</div>";
        host->SetInnerRML(markup);
    }

    void sync_settings() {
        const std::pair<const char*, int> values[] = {
            {"val-shadow-quality", controls.shadow_quality},
            {"val-reflection-quality", controls.reflection_quality},
            {"val-ray-tracing", controls.ray_tracing_enabled ? 1 : 0},
            {"val-shadow-distance", controls.shadow_distance},
            {"val-reflection-distance", controls.reflection_distance},
            {"val-frame-cap", controls.frame_cap_fps},
        };
        for (const auto& entry : values) {
            auto* label = document->GetElementById(entry.first);
            if (!label) continue;
            const std::string id = entry.first;
            std::string text;
            if (id == "val-ray-tracing") text = entry.second ? ": on" : ": off";
            else if (id.find("quality") != std::string::npos)
                text = ": " + quality_name(entry.second);
            else if (id == "val-frame-cap")
                text = entry.second > 0 ? ": " + std::to_string(entry.second) + " fps" : ": uncapped";
            else text = ": " + std::to_string(entry.second) + " m";
            label->SetInnerRML(text);
        }
    }

    static std::string quality_name(int value) {
        switch (value) {
            case 0: return "low";
            case 1: return "medium";
            case 2: return "high";
            default: return "ultra";
        }
    }

    void apply_setting(Rml::Element* input) override {
        const std::string setting = input->GetAttribute<Rml::String>("setting", "").c_str();
        const std::string value = input->GetAttribute<Rml::String>("value", "0").c_str();
        const int number = std::atoi(value.c_str());
        if (setting == "shadow_quality") controls.shadow_quality = static_cast<uint8_t>(number);
        else if (setting == "reflection_quality") controls.reflection_quality = static_cast<uint8_t>(number);
        else if (setting == "ray_tracing_enabled") controls.ray_tracing_enabled = static_cast<uint8_t>(number != 0);
        else if (setting == "shadow_distance") controls.shadow_distance = static_cast<uint16_t>(number);
        else if (setting == "reflection_distance") controls.reflection_distance = static_cast<uint16_t>(number);
        else if (setting == "frame_cap_fps") controls.frame_cap_fps = static_cast<uint16_t>(number);
        sync_settings();
        if (actions.settings_changed) actions.settings_changed();
    }

    void tweak_setting(const std::string& key, int delta) {
        auto clamp_u8 = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 3 ? 3 : v)); };
        if (key == "shadow_quality") controls.shadow_quality = clamp_u8(controls.shadow_quality + delta);
        else if (key == "reflection_quality") controls.reflection_quality = clamp_u8(controls.reflection_quality + delta);
        else if (key == "ray_tracing_enabled") controls.ray_tracing_enabled = controls.ray_tracing_enabled ? 0 : 1;
        else if (key == "shadow_distance")
            controls.shadow_distance = static_cast<uint16_t>(std::clamp(controls.shadow_distance + delta, 128, 8192));
        else if (key == "reflection_distance")
            controls.reflection_distance = static_cast<uint16_t>(std::clamp(controls.reflection_distance + delta, 128, 8192));
        else if (key == "frame_cap_fps")
            controls.frame_cap_fps = static_cast<uint16_t>(std::clamp(controls.frame_cap_fps + delta, 0, 480));
    }

    void handle_action(const std::string& action, Rml::Element* element) override {
        if (action == "leave") leave_requested_ = true;
        if (action == "quit") {
            if (actions.quit) actions.quit();
        } else if (action == "worlds") {
            show_screen("screen-worlds");
        } else if (action == "connect") {
            show_screen("screen-connect");
            set_text_input(true);
        } else if (action == "world") {
            set_text_input(false);
            const int index = std::atoi(element->GetAttribute<Rml::String>("data-index", "0").c_str());
            if (index >= 0 && index < static_cast<int>(worlds.size()) && actions.play_world)
                actions.play_world(worlds[index]);
        } else if (action == "connect-go") {
            set_text_input(false);
            if (actions.connect_server) actions.connect_server(input_value("connect-endpoint"));
        } else if (action == "back-main") {
            set_text_input(false);
            show_screen("screen-main");
        } else if (action == "back") {
            set_text_input(false);
            show_screen(pause_visible_ ? "screen-pause" : "screen-main");
            if (actions.settings_changed) actions.settings_changed();
        } else if (action == "settings-main" || action == "pause-settings") {
            show_screen("screen-settings");
        } else if (action == "tweak") {
            tweak_setting(element->GetAttribute<Rml::String>("data-key", ""),
                          std::atoi(element->GetAttribute<Rml::String>("data-delta", "0").c_str()));
            sync_settings();
            if (actions.settings_changed) actions.settings_changed();
        } else if (action == "resume") {
            if (actions.resume) actions.resume();
        }
    }
    bool leave_requested_{};
};

MenuStack::MenuStack(SDL_Window* window, Rml::RenderInterface* renderer,
                     const std::filesystem::path& assets, runtime_controls& controls,
                     MenuActions actions)
    : state_(std::make_unique<State>(window, controls, std::move(actions))) {
    if (!renderer) throw std::runtime_error("Missing RmlUi render interface for menus");
    auto& s = *state_;
    Rml::SetSystemInterface(&s.system);
    Rml::SetRenderInterface(renderer);
    if (!Rml::LoadFontFace(utf8(assets / "Fonts" / "Silkscreen-Regular.ttf")))
        throw std::runtime_error("Cannot load bundled Silkscreen UI font");
    int width{}, height{};
    SDL_GetWindowSizeInPixels(window, &width, &height);
    s.context = Rml::CreateContext("menu", {width, height});
    if (!s.context) throw std::runtime_error("Menu context initialization failed");
    s.document = s.context->LoadDocument(utf8(assets / "menu.rml"));
    if (!s.document) throw std::runtime_error("Cannot load menu document");
    s.document->Show();
    s.listener = std::make_unique<MenuListener>(s);
    s.document->AddEventListener("click", s.listener.get());
    s.document->AddEventListener("change", s.listener.get());
    s.show_screen("screen-main");
    s.refresh_worlds();
    s.context->Update();
}

MenuStack::~MenuStack() = default;

void MenuStack::install_actions(MenuActions actions) {
    state_->actions = std::move(actions);
}

void MenuStack::set_worlds(std::vector<MenuWorldEntry> worlds) {
    state_->worlds = std::move(worlds);
    state_->refresh_worlds();
}

void MenuStack::set_connect_status(const std::string& text) {
    state_->set_status("connect-status", text);
}

void MenuStack::show_main() {
    state_->pause_visible_ = false;
    state_->show_screen("screen-main");
}

void MenuStack::show_pause() {
    state_->pause_visible_ = true;
    state_->show_screen("screen-pause");
}

void MenuStack::hide_pause() {
    state_->pause_visible_ = false;
    state_->set_text_input(false);
    state_->active_screen = "none";
    if (auto* root = state_->document->GetElementById("menu-root"))
        root->SetClass("hidden", true);
}

bool MenuStack::pause_visible() const { return state_->pause_visible_; }
bool MenuStack::leave_requested() const {
    if (!state_->leave_requested_) return false;
    state_->leave_requested_ = false;
    return true;
}

void MenuStack::process_event(const SDL_Event& event) {
    RmlSDL::InputEventHandler(state_->context, state_->window, const_cast<SDL_Event&>(event));
}

void MenuStack::update(int width, int height) {
    auto& s = *state_;
    if (s.context->GetDimensions().x != width || s.context->GetDimensions().y != height)
        s.context->SetDimensions({width, height});
    if (auto* root = s.document->GetElementById("menu-root"))
        root->SetClass("hidden", s.active_screen == "none");
    s.context->Update();
}

Rml::Context* MenuStack::context() const { return state_->context; }

}
