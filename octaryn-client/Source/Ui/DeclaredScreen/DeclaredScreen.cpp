#include "DeclaredScreen.h"
#include <RmlUi/Core.h>
#include <glaze/glaze.hpp>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace octaryn::client::ui {
struct ScreenDeclaration {
  unsigned version{};
  std::string screen_id, model, document;
  std::vector<std::string> actions;
};
namespace {
std::string utf8(const std::filesystem::path& path) {
  const auto value=path.generic_u8string();
  return {reinterpret_cast<const char*>(value.data()),value.size()};
}
}
Rml::ElementDocument* load_world_library_screen(Rml::Context& context,
    const std::filesystem::path& package) {
  std::ifstream file(package/"screen.json",std::ios::binary|std::ios::ate);
  const auto size=file.tellg();
  if(!file || size<=0 || size>4096)throw std::runtime_error("World library declaration is missing or invalid");
  std::string text(static_cast<std::size_t>(size),'\0');file.seekg(0);
  if(!file.read(text.data(),size))throw std::runtime_error("Cannot read world library declaration");
  ScreenDeclaration declaration;
  constexpr glz::opts options{.error_on_missing_keys=true};
  const std::vector<std::string> actions={"select","search","browse","find","open","new_save","locate","prepare","cancel","settings","multiplayer","exit"};
  if(glz::read<options>(declaration,text) || declaration.version!=1 ||
      declaration.screen_id!="octaryn.basegame.world_library" || declaration.model!="world_library" ||
      declaration.document!="world-library.rml" || declaration.actions!=actions)
    throw std::runtime_error("World library declaration does not match the supported screen contract");
  for(const char* font:{"LatoLatin-Regular.ttf","LatoLatin-Bold.ttf"})
    if(!Rml::LoadFontFace(utf8(package/"Fonts"/font)))throw std::runtime_error("World library font is missing");
  auto* document=context.LoadDocument(utf8(package/declaration.document));
  if(!document)throw std::runtime_error("Cannot load declared world library screen");
  for(const char* id:{"world-library","library-search","library-list","library-add","library-find",
      "library-open","library-new-save","library-locate","library-prepare","library-cancel","library-feedback",
      "library-status","library-empty","library-details"}) {
    if(!document->GetElementById(id)) {
      document->Close();
      throw std::runtime_error("Declared world library screen is missing a required control");
    }
  }
  return document;
}
}
