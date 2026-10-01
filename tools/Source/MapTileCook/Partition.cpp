#include "TileCook.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace octaryn::tools::tiles {
bool partition(const MapModel& model,const TextureFiles& textures,const Settings& settings,const std::filesystem::path& output,
    std::vector<TileResult>& results,std::string& error) {
  if(!settings.triangles || settings.triangles>131072 || !std::isfinite(settings.cell) || settings.cell<=0) {error="invalid tile limits";return false;}
  std::map<std::array<int,3>,std::vector<Triangle>> cells;
  for(unsigned p=0;p<model.primitives.size();++p) {
    const auto& primitive=model.primitives[p];
    for(unsigned i=0;i<primitive.index_count;i+=3) {
      std::array<int,3> key{};
      for(unsigned axis=0;axis<3;++axis) {
        double center=0;for(unsigned corner=0;corner<3;++corner)center+=model.vertices.at(model.indices.at(primitive.first_index+i+corner)).position[axis]/3.;
        const auto cell=std::floor(center/settings.cell);
        if(!std::isfinite(cell) || cell<std::numeric_limits<int>::min() || cell>std::numeric_limits<int>::max()) {error="nonfinite or excessive map bounds";return false;}
        key[axis]=static_cast<int>(cell);
      }
      cells[key].push_back({p,primitive.first_index+i});
    }
  }
  std::filesystem::create_directories(output/"tiles");results.clear();
  for(auto& [key,triangles]:cells) {
    if(settings.order==PartitionOrder::Morton)order_triangles(model,triangles,key,settings);
    size_t first=0;
    while(first<triangles.size()) {
      std::set<std::string> resident;std::uint64_t bytes=0;size_t end=first;
      while(end<triangles.size() && end-first<settings.triangles) {
        std::uint64_t added=0;
        for(const auto& texture:textures.materials.at(triangles[end].primitive))if(!resident.contains(texture))added+=textures.bytes.at(texture);
        if(bytes+added>settings.texture_budget)break;
        resident.insert(textures.materials[triangles[end].primitive].begin(),textures.materials[triangles[end].primitive].end());bytes+=added;++end;
      }
      if(end==first) {error="one material exceeds tile texture budget; increase budget or recook its source textures";return false;}
      if(results.size()>=settings.max_tiles) {error="tile count exceeds configured limit";return false;}
      TileResult result;result.file="tiles/"+std::to_string(results.size())+".glb";
      if(!write_tile(model,std::span(triangles).subspan(first,end-first),textures,output/result.file,result,error))return false;
      results.push_back(std::move(result));first=end;
    }
  }
  if(results.empty()) {error="map has no triangles";return false;}return true;
}
}
