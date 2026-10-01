#include "SceneResourceHash.h"
#include "ResourceDigest.h"

namespace octaryn::client::rendering::virtual_geometry {
std::string scene_resource_hash(const std::filesystem::path& path,std::string& error,const std::atomic_bool* cancel) {
  return content::resource_tree_digest(path,error,1ull<<40,cancel);
}
}
