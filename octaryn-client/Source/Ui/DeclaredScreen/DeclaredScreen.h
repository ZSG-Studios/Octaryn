#pragma once
#include <filesystem>
namespace Rml { class Context; class ElementDocument; }
namespace octaryn::client::ui {
// Loads the one validated startup surface declared by the bundled game.
Rml::ElementDocument* load_world_library_screen(Rml::Context& context,
    const std::filesystem::path& package);
}
