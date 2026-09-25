#include "BlockTransportPlantProbe.h"
#include <algorithm>
#include <limits>
#include <set>

namespace mesh_probe {
void block_transport_plant_energy_cases(Fixture&,unsigned);
namespace {
using namespace plant_probe;
void key_cases(Fixture& f,unsigned layer,unsigned flags) {
  std::vector<Query> queries;std::vector<BlockSurfaceKey> expected;
  const std::array<std::array<int,3>,4> anchors{{{-48,-24,112},{8,-24,8},
      {16777217,-16777219,2},{INT32_MIN,INT32_MAX,INT32_MIN+1}}};
  for(const auto& anchor:anchors)for(unsigned direction=6;direction<10;++direction)
    for(unsigned side=0;side<2;++side) {
      const unsigned plane=(direction-6)/2;const auto n=normal(plane,side);Query query;
      query.face={std::bit_cast<unsigned>(anchor[0]),std::bit_cast<unsigned>(anchor[1]),
          std::bit_cast<unsigned>(anchor[2]),direction<<16};
      query.toward={n[0],n[1],n[2],0};query.options={(layer<<3)|256u,flags,0,0};
      const BlockSurfaceKey literal{anchor[0],anchor[1],anchor[2],tag(layer,plane,side)};
      require(block_crossed_plant_key(anchor,direction,layer,n)==literal,"BT plant CPU plane-side key oracle");
      expected.push_back(literal);queries.push_back(query);
    }
  const auto actual=dispatch(f,"key_main",queries);
  for(std::size_t i=0;i<actual.size();++i) {
    const auto& value=actual[i];const auto& key=expected[i];
    require(value.key==key && value.packed[3]==key.direction+1,"BT plant GPU exact signed plane-side key");
    require(value.flags[0]==1 && value.flags[2]==1,"BT plant eligibility or encoded normal marker");
    const auto n=normal(((key.direction&15)-6)/2,key.direction&1);
    for(unsigned axis=0;axis<3;++axis)require(std::abs(value.normal[axis]-n[axis])<1e-6f,
        "BT plant encoded true diagonal normal disagrees with side");
  }
  for(unsigned direction=0;direction<6;++direction)
    require(block_surface_key({-48,-24,112},direction,1,1,{.5f,.5f,.5f})==
        BlockSurfaceKey{-48,-24,112,direction},"BT plant extension changed CPU cube key");
  require(!block_transport_plant_tag((17u<<4)|6u) &&
      block_crossed_plant_key({0,0,0},6,layer,{NAN,0,0}).direction==UINT32_MAX,
      "BT plant unsupported key validation");
  std::printf("block_transport_plant_keys=passed cases=%zu duplicate_planes=1 sides=2 signed_int_limits=1\n",actual.size());
}
double width(unsigned i) {return std::min(1.,(double(i)+.5)/31)-std::max(0.,(double(i)-.5)/31);}
float linear(unsigned value) {const float c=float(value)/255;return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
void tables(Fixture& f,SDL_Surface* png,const std::vector<unsigned>& layers) {
  auto& r=f.renderer;std::vector<Words> cells(29*1024);std::array<Pixel,29> actual{};
  checked(r.device->readBuffer(r.atlas->plant_mask_cells,0,cells.size()*sizeof(Words),cells.data()),"BT plant alias table readback");
  checked(r.device->readBuffer(r.atlas->plant_mask_layers,0,sizeof(actual),actual.data()),"BT plant layer table readback");
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> specular(
      load_atlas_rgba("Atlases/basegame-specular.png"),SDL_DestroySurface);
  require(specular && specular->w==png->w && specular->h==32,"BT plant specular PNG oracle");
  for(unsigned layer:layers) {
    std::array<double,1024> weights{},probabilities{};double area=0;std::array<double,3> rho{};
    const auto uploaded=atlas_layer_pixels(png,layer,ATLAS_MIP_ALBEDO);
    require(uploaded.size()>=4096,"BT plant uploaded color oracle");
    unsigned expected_count=0;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x)if(opaque(png,layer,x,y)) {
      const double weight=width(x)*width(y);weights[y*32+x]=weight;area+=weight;++expected_count;
      const auto* color=uploaded.data()+(y*32+x)*4;
      const auto* spec=static_cast<const Uint8*>(specular->pixels)+y*unsigned(specular->pitch)+(layer*32+x)*4;
      if(spec[1]<230)for(unsigned c=0;c<3;++c)rho[c]+=.96*linear(color[c])*weight;
    }
    const auto count=std::bit_cast<unsigned>(actual[layer][3]);
    require(count==expected_count && count>0 && count<1024,"BT plant alias count must match sparse original PNG");
    std::set<unsigned> distinct;
    for(unsigned i=0;i<count;++i) {
      const auto& cell=cells[layer*1024+i];const float threshold=std::bit_cast<float>(cell[2]);
      require(cell[0]<1024 && cell[1]<count && std::isfinite(threshold) && threshold>=0 && threshold<=1,
          "BT plant alias table escaped bounded layer");
      require(distinct.insert(cell[0]).second && weights[cell[0]]>0,"BT plant alias includes duplicate or alpha hole");
      require(cells[layer*1024+cell[1]][0]<1024,"BT plant alias destination texel bounds");
      probabilities[cell[0]]+=double(threshold)/count;
      probabilities[cells[layer*1024+cell[1]][0]]+=(1-double(threshold))/count;
    }
    for(unsigned i=0;i<1024;++i)require(std::abs(probabilities[i]-weights[i]/area)<1e-7,
        "BT plant alias implied PDF differs from independent physical-area oracle");
    for(unsigned c=0;c<3;++c)require(std::abs(actual[layer][c]-rho[c]/area)<2e-6,
        "BT plant conditional reflectance includes holes or wrong color/metal conversion");
  }
  for(const auto& animation:r.atlas->animations)
    require(std::bit_cast<unsigned>(actual[animation.layer][3])==0,"BT plant animated masks must fail closed");
  require(world_atlas_plant_mask_bytes(r.atlas)==476064,"BT opacity mask fixed GPU memory charge");
}
void samples(Fixture& f,SDL_Surface* png,const std::vector<unsigned>& layers) {
  std::vector<Query> queries;
  for(unsigned layer:layers)for(unsigned plane=0;plane<2;++plane)for(unsigned side=0;side<2;++side)
    for(unsigned sample=0;sample<2048;++sample) {
      Query query;query.key={-48,-24,112,tag(layer,plane,side)};
      query.options={0,0,sample%16,19+sample/16};queries.push_back(query);
    }
  const auto actual=dispatch(f,"sample_main",queries);
  for(const auto& value:actual) {
    require(value.flags[0]==1,"BT plant static opaque mask rejected sampling");
    const auto tag_value=value.key.direction;const unsigned plane=((tag_value&15)-6)/2;
    const auto n=normal(plane,tag_value&1);float length=0,cosine=0;Vector local{};
    const int anchor[3]={value.key.x,value.key.y,value.key.z};
    const double x=double(value.origin[0])-anchor[0],z=double(value.origin[2])-anchor[2];
    const double signed_plane=(plane?x+z-1:z-x)*(tag_value%2?-1:1);
    const double ulp=std::max(std::abs(double(std::nextafter(value.origin[0],INFINITY))-value.origin[0]),
        std::abs(double(std::nextafter(value.origin[2],INFINITY))-value.origin[2]));
    require(signed_plane>0 && signed_plane<=4*std::max(ulp,1./1048576),"BT plant outward offset is not ULP bounded");
    const double projected_x=plane?(x-z+1)*.5:(x+z)*.5;
    const double projected_z=plane?(z-x+1)*.5:projected_x;
    for(unsigned axis=0;axis<3;++axis) {
      require(std::isfinite(value.origin[axis]) && std::isfinite(value.direction[axis]),"BT plant finite sample");
      local[axis]=axis==0?float(projected_x):axis==2?float(projected_z):value.origin[axis]-float(anchor[axis]);
      require(local[axis]>=-2e-5f && local[axis]<=1+2e-5f,"BT plant origin escaped unit cell");
      length+=value.direction[axis]*value.direction[axis];cosine+=value.direction[axis]*n[axis];
    }
    require(std::abs(length-1)<2e-5f && cosine>0,"BT plant direction escaped true side hemisphere");
    require(std::abs(plane?local[0]+local[2]-1:local[0]-local[2])<2e-5f,
        "BT plant alias origin is not on actual crossed geometry plane");
    const int texel_x=int(std::floor(.5+31*(1-local[0]))),texel_y=int(std::floor(.5+31*(1-local[1])));
    require(texel_x>=0 && texel_x<32 && texel_y>=0 && texel_y<32 &&
        opaque(png,tag_value>>4,unsigned(texel_x),unsigned(texel_y)),
        "BT plant sampled origin landed in a source PNG alpha hole");
  }
  std::printf("block_transport_plant_masks=passed origins=%zu source_png=1 alias_pdf=1 conditional_reflectance=1 bounded_memory=476064\n",actual.size());
}
void rays(Fixture& f,SDL_Surface* png,unsigned material,unsigned layer) {
  std::vector<Query> queries;std::vector<bool> expected;
  for(unsigned plane=0;plane<2;++plane)for(unsigned side=0;side<2;++side)
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
      Query query;const auto n=normal(plane,side);
      const auto p=point({8,-24,8},plane,(float(x)+.5f)/32,(float(y)+.5f)/32);
      query.origin={p[0]+2*n[0],p[1],p[2]+2*n[2],4};query.direction={-n[0],0,-n[2],0};
      query.key={8,-24,8,tag(layer,plane,side)};queries.push_back(query);expected.push_back(opaque(png,layer,x,y));
    }
  const auto actual=dispatch(f,"ray_main",queries,true);unsigned hits=0,holes=0;
  for(std::size_t i=0;i<actual.size();++i) {
    const auto& value=actual[i];
    require((value.flags[0]!=0)==expected[i],"BT plant RT alpha differs from original PNG oracle");
    if(expected[i]) {
      require(value.flags[1]==1 && value.flags[3]==material && value.key==queries[i].key &&
          std::abs(value.origin[3]-2)<3e-5f && value.direction[3]>=.35f,
          "BT plant actual RT plane-side identity or alpha coverage");++hits;
    } else {require(value.flags[1]==0,"BT plant transparent hole admitted a contributor");++holes;}
  }
  require(hits>0 && holes>0,"BT plant RT fixture must exercise cutout coverage and holes");
  std::printf("block_transport_plants_rt=passed production_query=1 png_oracle=1 planes=2 sides=2 signed_anchor=1 hits=%u holes=%u\n",hits,holes);
}
}
void block_transport_plant_cases(Fixture& f) {
  using namespace plant_probe;auto& r=f.renderer;
  require(prepare_world_atlas_plant_masks(r.atlas),"BT plant mask preparation");
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> png(load_atlas_rgba("Atlases/basegame-color.png"),SDL_DestroySurface);
  require(png && png->w==32*29 && png->h==32,"BT plant source PNG dimensions");
  unsigned material=0;std::vector<unsigned> layers;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].sprite) {
    const auto layer=world_atlas_preview_layer(r.atlas,i);
    if(layer>=11 && layer<=15)layers.push_back(layer);
    if(f.catalog[i].id=="octaryn.basegame.block.gardenia")material=i;
  }
  std::sort(layers.begin(),layers.end());layers.erase(std::unique(layers.begin(),layers.end()),layers.end());
  require(material && layers.size()==5,"BT plant catalog fixture requires all five static crossed masks");
  const unsigned layer=world_atlas_preview_layer(r.atlas,material);
  key_cases(f,layer,r.atlas->material_flags[material]);tables(f,png.get(),layers);samples(f,png.get(),layers);
  raster(f,material,layer);
  auto source=column(0,0,-32,32);
  const auto previous=r.sources.find({0,0});
  source.revision=previous==r.sources.end()?1:previous->second.revision+1;
  put(source,8,8,8,static_cast<std::uint16_t>(material));source.blocks.compact();
  require(open_world_renderer_update(&r,source),"BT plant exact geometry publication");settle(r);
  rays(f,png.get(),material,layer);shadows(f,png.get(),layer);
  block_transport_plant_energy_cases(f,layer);
  require(r.debug.errors.load()==0,"BT plant graphics validation errors");
}
}
