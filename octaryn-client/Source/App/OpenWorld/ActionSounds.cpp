#include "ActionSounds.h"
#include <glaze/glaze.hpp>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

namespace octaryn::client::app {
struct ActionSoundCatalog {
  std::string schema;
  std::map<std::string,audio::SoundDefinition> sounds;
};
audio::SoundDefinitions load_action_sounds(const std::filesystem::path& path) {
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  const auto size=file.tellg();
  if(!file || size<=0 || size>8192) throw std::runtime_error("Action sound catalog missing or exceeds 8192 bytes");
  std::string text(static_cast<std::size_t>(size),'\0');
  file.seekg(0);
  if(!file.read(text.data(),size)) throw std::runtime_error("Cannot read action sound catalog");
  ActionSoundCatalog catalog;
  constexpr glz::opts options{.error_on_unknown_keys=true,.error_on_missing_keys=true};
  if(glz::read<options>(catalog,text) || catalog.schema!="octaryn.basegame.action-sounds.v1" || catalog.sounds.size()!=4)
    throw std::runtime_error("Invalid action sound catalog schema or entries");
  constexpr const char* names[]={"place","break","select","change"};
  audio::SoundDefinitions result;
  for(std::size_t i=0;i<result.size();++i) {
    const auto found=catalog.sounds.find(names[i]);
    if(found==catalog.sounds.end() || !audio::valid_action_sound(found->second))
      throw std::runtime_error("Invalid action sound definition");
    result[i]=found->second;
  }
  return result;
}
}
