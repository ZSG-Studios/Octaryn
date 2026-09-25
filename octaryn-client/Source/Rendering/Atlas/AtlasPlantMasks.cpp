#include "AtlasInternal.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <utility>

namespace octaryn::client::rendering {
namespace {
using Cell=std::array<std::uint32_t,4>;
using Layer=std::array<float,4>;
constexpr unsigned Size=ATLAS_BLOCK_WIDTH,Layers=ATLAS_LAYER_COUNT,Cells=Size*Size;
static_assert(Layers*(Cells*sizeof(Cell)+2*sizeof(Layer))==WorldAtlasPlantMaskBytes);
float linear(std::uint8_t value) {
  const float c=float(value)/255.f;
  return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);
}
double width(unsigned texel) {
  return std::min(1.,(double(texel)+.5)/double(Size-1))-
      std::max(0.,(double(texel)-.5)/double(Size-1));
}
void layer_table(const std::vector<unsigned char>& color,const std::vector<unsigned char>& specular,
    Cell* cells,Layer& layer,Layer& cube) {
  std::array<double,Cells> weights{};
  std::array<double,3> reflectance{},cube_reflectance{};double area=0;unsigned count=0;
  for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x) {
    const unsigned offset=(y*Size+x)*4;
    if(float(color[offset+3])<ATLAS_ALPHA_CUTOFF_U8)continue;
    const double weight=width(x)*width(y);weights[count]=weight;area+=weight;
    cells[count][0]=x|(y<<5);
    if(specular[offset+1]<230)for(unsigned c=0;c<3;++c) {
      const double rho=double(linear(color[offset+c]))*.96;
      reflectance[c]+=rho*weight;cube_reflectance[c]+=rho;
    }
    ++count;
  }
  if(!count)return;
  for(unsigned c=0;c<3;++c) {layer[c]=float(reflectance[c]/area);cube[c]=float(cube_reflectance[c]/count);}
  layer[3]=cube[3]=std::bit_cast<float>(count);
  std::vector<unsigned> small,large;small.reserve(count);large.reserve(count);
  for(unsigned i=0;i<count;++i) {
    weights[i]*=double(count)/area;
    (weights[i]<1?small:large).push_back(i);
  }
  while(!small.empty() && !large.empty()) {
    const unsigned low=small.back(),high=large.back();small.pop_back();large.pop_back();
    cells[low][1]=high;cells[low][2]=std::bit_cast<std::uint32_t>(float(std::clamp(weights[low],0.,1.)));
    weights[high]+=weights[low]-1;
    (weights[high]<1?small:large).push_back(high);
  }
  const auto finish=[&](const std::vector<unsigned>& remaining) {
    for(unsigned index:remaining) {
      cells[index][1]=index;cells[index][2]=std::bit_cast<std::uint32_t>(1.f);
    }
  };
  finish(small);finish(large);
}
bool image_valid(const SDL_Surface* image) {
  return image && image->w==int(Size*Layers) && image->h==int(Size) &&
      image->format==SDL_PIXELFORMAT_RGBA32 && image->pitch>=int(Size*Layers*4);
}
bool create(WorldAtlas& atlas,const void* data,std::uint64_t bytes,const char* label,
    Slang::ComPtr<rhi::IBuffer>& result) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=16;desc.label=label;
  desc.memoryType=rhi::MemoryType::DeviceLocal;desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopySource;
  return SLANG_SUCCEEDED(atlas.device->createBuffer(desc,data,result.writeRef()));
}
}
bool prepare_world_atlas_plant_masks(WorldAtlas* atlas) {
  if(!atlas || !atlas->device)return false;
  if(atlas->plant_mask_cells && atlas->plant_mask_layers && atlas->cube_mask_layers)return true;
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> color(load_atlas_rgba("Atlases/basegame-color.png"),SDL_DestroySurface);
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> specular(load_atlas_rgba("Atlases/basegame-specular.png"),SDL_DestroySurface);
  if(!image_valid(color.get()) || !image_valid(specular.get())) {
    std::fprintf(stderr,"Invalid block transport plant mask atlas dimensions\n");return false;
  }
  std::array<bool,Layers> animated{};
  for(const auto& animation:atlas->animations) {
    if(animation.layer>=Layers)return false;
    animated[animation.layer]=true;
  }
  std::vector<Cell> cells(Layers*Cells);std::array<Layer,Layers> layers{},cube_layers{};
  for(unsigned layer=0;layer<Layers;++layer) {
    if(animated[layer])continue;
    // Mip zero includes the same RGB dilation as the uploaded albedo texture.
    const auto pixels=atlas_layer_pixels(color.get(),layer,ATLAS_MIP_ALBEDO);
    const auto material=atlas_layer_pixels(specular.get(),layer,ATLAS_MIP_LABPBR_SPECULAR);
    if(pixels.size()<Cells*4 || material.size()<Cells*4)return false;
    layer_table(pixels,material,cells.data()+layer*Cells,layers[layer],cube_layers[layer]);
  }
  Slang::ComPtr<rhi::IBuffer> cell_buffer,layer_buffer,cube_buffer;
  if(!create(*atlas,cells.data(),cells.size()*sizeof(Cell),"block_transport_plant_mask_cells",cell_buffer) ||
      !create(*atlas,layers.data(),sizeof(layers),"block_transport_plant_mask_layers",layer_buffer) ||
      !create(*atlas,cube_layers.data(),sizeof(cube_layers),"block_transport_cube_mask_layers",cube_buffer))return false;
  atlas->plant_mask_cells=std::move(cell_buffer);atlas->plant_mask_layers=std::move(layer_buffer);
  atlas->cube_mask_layers=std::move(cube_buffer);return true;
}
std::uint64_t world_atlas_plant_mask_bytes(WorldAtlas* atlas) {
  if(!atlas)return 0;
  std::uint64_t bytes=0;
  if(atlas->plant_mask_cells)bytes+=atlas->plant_mask_cells->getDesc().size;
  if(atlas->plant_mask_layers)bytes+=atlas->plant_mask_layers->getDesc().size;
  if(atlas->cube_mask_layers)bytes+=atlas->cube_mask_layers->getDesc().size;
  return bytes;
}
}
