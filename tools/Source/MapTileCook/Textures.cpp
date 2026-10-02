#include "TileCook.h"
#include "MapTextureCache.h"
#include <fstream>

namespace octaryn::tools::tiles {
bool prepare_textures(const MapModel& model,const std::filesystem::path& cache,const std::filesystem::path& output,
    TextureFiles& files,std::string& error) {
  try {
    std::filesystem::create_directories(output/"images");std::filesystem::create_directories(output/"textures");
    files.images.resize(model.images.size());files.materials.resize(model.primitives.size());
    std::map<std::pair<unsigned,MapMipOptions>,std::string> requests;
    for(size_t p=0;p<model.primitives.size();++p) {
      const auto& material=model.primitives[p].material;
      for(unsigned role=0;role<21;++role) {
        const auto image=material.textures[role].image;if(image<0)continue;
        const auto& source=model.images.at(image);
        if(files.images[image].uri.empty()) {
          const auto hash=map_texture_digest(source.bytes);
          const auto file=hash+(source.mime_type=="image/jpeg"?".jpg":".png");
          files.images[image].uri="../images/"+file;
          const auto path=output/"images"/file;
          if(!std::filesystem::exists(path)) {
            std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(source.bytes.data()),source.bytes.size());
            if(!stream)throw std::runtime_error("cannot write shared image");
          }
        }
        const auto options=map_mip_options(material,role);const auto request=std::make_pair(unsigned(image),options);
        auto found=requests.find(request);
        if(found==requests.end())found=requests.emplace(request,map_texture_cache_key(source,options)).first;
        const auto& key=found->second;files.materials[p].insert(key);
        if(files.bytes.contains(key))continue;
        MapCachedTexture cooked;
        if(read_map_texture_cache(cache/(key+".dds"),0,0,role==0 || role==4 || (role>=5 && (role-5)%2==0),cooked,error)!=MapCacheResult::Ready)
          throw std::runtime_error("required source texture cache "+key+": "+error);
        std::uint64_t bytes=0;for(const auto& level:cooked.levels)bytes+=level.blocks.size();files.bytes[key]=bytes;
        for(const auto* suffix:{".dds",".dds.sha256"})
          std::filesystem::copy_file(cache/(key+suffix),output/"textures"/(key+suffix),std::filesystem::copy_options::overwrite_existing);
      }
    }
    return true;
  } catch(const std::exception& exception) {error=exception.what();return false;}
}
}
