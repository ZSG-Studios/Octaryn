#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;
union SDL_Event;
struct runtime_controls;
namespace Rml { class Context; class RenderInterface; }

namespace octaryn::client::app {

struct MenuWorldEntry {
    std::string name;
    std::filesystem::path glb;
    std::filesystem::path manifest;
};

struct MenuActions {
    std::function<void()> quit;
    std::function<void(const MenuWorldEntry&)> play_world;
    std::function<void(const std::string& endpoint)> connect_server;
    std::function<void()> leave_world;
    std::function<void()> resume;
    std::function<void()> settings_changed;
};

// Engine menu system on RmlUi: main menu, world selector, server connection,
// settings and the in-game pause screen. Screens live in one document; the
// caller pumps SDL events and drives update/render each frame.
class MenuStack {
public:
    MenuStack(SDL_Window* window, Rml::RenderInterface* renderer,
              const std::filesystem::path& assets, runtime_controls& controls,
              MenuActions actions);
    ~MenuStack();
    MenuStack(const MenuStack&) = delete;
    MenuStack& operator=(const MenuStack&) = delete;

    void set_worlds(std::vector<MenuWorldEntry> worlds);
    // Replaces the action handlers bound at construction (menu phase wires
    // selection callbacks after the stack exists).
    void install_actions(MenuActions actions);
    void set_connect_status(const std::string& text);

    void show_main();
    // In-game overlay: only the pause/settings screens are interactive; the
    // world keeps rendering underneath.
    void show_pause();
    void hide_pause();
    bool pause_visible() const;
    // True once after the pause menu's leave action fires.
    bool leave_requested() const;

    // Forwards window input to the UI.
    void process_event(const SDL_Event& event);
    void update(int width, int height);
    Rml::Context* context() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

}
