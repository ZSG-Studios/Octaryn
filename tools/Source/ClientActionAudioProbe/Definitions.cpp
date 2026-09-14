#include "ActionSounds.h"
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
}
void check_action_sound_definitions(const std::filesystem::path& catalog,const std::filesystem::path& scratch) {
  const auto actual=octaryn::client::app::load_action_sounds(catalog);
  constexpr std::array<double,4> frequencies{540,180,760,620},gains{.18,.35,.12,.10};
  for(std::size_t i=0;i<4;++i)
    require(actual[i].frequency==frequencies[i] && actual[i].gain==gains[i],"basegame sound definition differs from original");
  std::filesystem::create_directories(scratch);
  std::ifstream source(catalog);
  const std::string original{std::istreambuf_iterator<char>(source),{}};
  const auto rejected=[&](const std::string& text) {
    const auto path=scratch/"invalid-action-sounds.json";
    {std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<text;require(bool(output),"write invalid fixture");}
    bool failed=false;
    try {octaryn::client::app::load_action_sounds(path);} catch(const std::runtime_error&) {failed=true;}
    require(failed,"invalid action sound catalog accepted");
  };
  const auto replace=[&](const std::string& from,const std::string& to) {
    auto text=original;const auto at=text.find(from);require(at!=std::string::npos,"mutation marker missing");
    text.replace(at,from.size(),to);rejected(text);
  };
  rejected("");rejected("{");rejected(std::string(8193,' '));
  replace("action-sounds.v1","action-sounds.v2");
  replace("\"place\"","\"unknown\"");
  replace("540","24000");replace("540","-1");
  replace("0.18","5");replace("0.18","-0.1");
  replace(", \"gain\": 0.18","");
  require(octaryn::client::app::load_action_sounds(catalog)[0].frequency==540,"invalid fixture altered valid catalog");
}
