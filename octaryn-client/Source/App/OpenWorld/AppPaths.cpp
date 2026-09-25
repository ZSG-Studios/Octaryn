#include "AppPaths.h"

#include <SDL3/SDL.h>
#include <stdexcept>

namespace octaryn::client::app {

std::filesystem::path bundle_path(const char* base)
{
  if (!base) throw std::runtime_error("Missing application path");
  return std::filesystem::path(reinterpret_cast<const char8_t*>(base));
}

std::filesystem::path repo_root(const std::filesystem::path& bundle)
{
  auto path = bundle;
  if (path.filename().empty()) path = path.parent_path();
  const auto root = path.parent_path().parent_path().parent_path().parent_path();
  if (std::filesystem::exists(root / "CMakeLists.txt")) return root;
  char* pref = SDL_GetPrefPath("ZSGStudios", "Octaryn");
  if (!pref) throw std::runtime_error(SDL_GetError());
  std::filesystem::path fallback = bundle_path(pref);
  SDL_free(pref);
  return fallback;
}

}
