#pragma once
#include "MapTextureCache.h"
#include <algorithm>
namespace octaryn::client::rendering {
inline bool map_texture_content_valid(const std::string& key,const MapCachedTexture& texture) {
 if(key.size()!=64 || !std::all_of(key.begin(),key.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}) ||
     texture.levels.empty() || texture.levels.size()>13)return false;
 auto width=texture.levels.front().width,height=texture.levels.front().height;
 if(!width || !height || width>4096 || height>4096)return false;
 for(const auto& level:texture.levels) {
   const auto bytes=texture.compressed?std::uint64_t((width+3)/4)*((height+3)/4)*16:std::uint64_t(width)*height*4;
   if(level.width!=width || level.height!=height || level.blocks.size()!=bytes)return false;
   width=std::max(1u,width/2);height=std::max(1u,height/2);
 }
 return true;
}
}
