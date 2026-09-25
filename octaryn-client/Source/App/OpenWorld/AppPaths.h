#pragma once
#include <filesystem>

struct SDL_Window;

namespace octaryn::client::app {

std::filesystem::path bundle_path(const char* base);
std::filesystem::path repo_root(const std::filesystem::path& bundle);

}
