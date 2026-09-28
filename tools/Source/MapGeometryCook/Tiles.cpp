#include "Simplify.h"
#include "MapMeshOptimization.h"
#include "MapTextureCache.h"
#include "MapAssetPrepare.h"
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::rendering {
bool cook_map_tile_lods(const std::filesystem::path& directory) {
  unsigned count=0;std::uint64_t prepared_max=0;std::string error;
  for(const auto& file:std::filesystem::directory_iterator(directory/"tiles")) {
    if(file.path().extension()!=".glb")continue;
    MapModel model;
    if(!load_map_model(file.path(),model,error) || !optimize_map_mesh(model,error))throw std::runtime_error(error);
    for(auto& image:model.images)std::vector<std::uint8_t>().swap(image.bytes);
    const auto hash=map_texture_file_digest(file.path(),error);if(hash.empty())throw std::runtime_error(error);
    auto lods=cook_map_lods(model);auto path=file.path();path+=".lods";
    if(!write_map_lods(path,hash,model,lods,error))throw std::runtime_error(error);
    MapLodData checked;if(!read_map_lods(path,hash,model,checked,error))throw std::runtime_error(error);
    model={};lods={};checked={};
    PreparedMapAsset prepared;
    if(!prepare_map_asset(file.path(),prepared,error,nullptr,directory/"textures"))throw std::runtime_error(file.path().string()+": "+error);
    if(prepared.images.decoded)throw std::runtime_error("tile preparation decoded source despite complete shared cache");
    std::uint64_t bytes=prepared.model.vertices.size()*sizeof(MapVertex)+prepared.model.indices.size()*4+prepared.lods.indices.size()*4;
    for(const auto& image:prepared.images.textures)for(const auto& mip:image.texture.levels)bytes+=mip.blocks.size();
    prepared_max=std::max(prepared_max,bytes);++count;
  }
  std::printf("map_tile_lods complete=1 tiles=%u prepared_max_bytes=%llu source_decodes=0\n",count,prepared_max);return true;
}
}
