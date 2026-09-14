#include "AtlasInternal.h"
#include <filesystem>
#include <fstream>
#include <sstream>
namespace octaryn::client::rendering {
bool load_atlas_animations(WorldAtlas& atlas) {
  atlas.animation.reset(load_atlas_rgba("Atlases/basegame-animation.png"));
  if (!atlas.animation || atlas.animation->h!=32 || atlas.animation->w%32) return false;
  const auto path=atlas_asset_path("Atlases/basegame-animation.txt");
  std::ifstream file(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())));
  std::string line;
  if (!std::getline(file,line)) return false;
  // Asset files can retain CRLF when a Windows checkout is packaged for Linux.
  if (!line.empty() && line.back()=='\r') line.pop_back();
  if (line!="Octaryn generated atlas animations") return false;
  while (std::getline(file,line)) {
    if (!line.empty() && line.back()=='\r') line.pop_back();
    if (!line.starts_with("animation=")) continue;
    std::stringstream fields(line.substr(10));
    std::array<std::string,5> parts;
    for (auto& part:parts) if (!std::getline(fields,part,'|')) return false;
    try {
      AtlasAnimation animation;
      animation.layer=std::stoul(parts[0]); animation.first=std::stoul(parts[2]);
      const auto count=std::stoul(parts[3]);
      std::stringstream times(parts[4]); std::string time;
      while (std::getline(times,time,',')) {
        const auto ticks=std::stoul(time);
        if (!ticks || ticks>10000) return false;
        animation.ticks.push_back(static_cast<unsigned>(ticks)); animation.total+=static_cast<unsigned>(ticks);
      }
      if (count!=animation.ticks.size() || !count || count>256 || animation.layer>=29 ||
          animation.first+count>static_cast<unsigned>(atlas.animation->w/32) || atlas.animations.size()>=64) return false;
      animation.frames.reserve(count);
      for(unsigned frame=0;frame<count;++frame)
        animation.frames.push_back(atlas_layer_pixels(atlas.animation.get(),animation.first+frame,ATLAS_MIP_ALBEDO));
      atlas.animations.push_back(std::move(animation));
    } catch (...) { return false; }
  }
  return !atlas.animations.empty();
}
bool update_world_atlas(WorldAtlas* atlas,rhi::ICommandQueue* queue,double seconds) {
  if (!atlas || !queue || seconds<0) return false;
  std::vector<std::pair<std::size_t,unsigned>> changed;
  const auto tick=static_cast<std::uint64_t>(seconds*20.0);
  for (std::size_t i=0;i<atlas->animations.size();++i) {
    auto& animation=atlas->animations[i];
    auto remaining=tick%animation.total; unsigned frame=0;
    while (frame+1<animation.ticks.size() && remaining>=animation.ticks[frame]) remaining-=animation.ticks[frame++];
    if (frame!=animation.active) changed.emplace_back(i,frame);
  }
  if (changed.empty()) return true;
  Slang::ComPtr<rhi::ICommandEncoder> commands;
  if (SLANG_FAILED(queue->createCommandEncoder(commands.writeRef()))) return false;
  commands->setTextureState(atlas->textures[0],rhi::ResourceState::CopyDestination);
  for (const auto& [index,frame]:changed) {
    const auto& animation=atlas->animations[index];
    const auto& bytes=animation.frames[frame];
    auto data=atlas_subresources(bytes,1);
    rhi::SubresourceRange range{};
    range.layer=animation.layer;
    range.layerCount=1; range.mipCount=6;
    if (SLANG_FAILED(commands->uploadTextureData(atlas->textures[0],range,{},rhi::Extent3D::kWholeTexture,data.data(),6))) return false;
  }
  commands->setTextureState(atlas->textures[0],rhi::ResourceState::ShaderResource);
  Slang::ComPtr<rhi::ICommandBuffer> buffer;
  // RHI owns the staged bytes until completion. Later draws on this same queue
  // consume the upload in order; the frame's completion wait covers its lifetime.
  if (SLANG_FAILED(commands->finish(buffer.writeRef())) || SLANG_FAILED(queue->submit(buffer))) return false;
  for (const auto& [index,frame]:changed) atlas->animations[index].active=frame;
  return true;
}
}
