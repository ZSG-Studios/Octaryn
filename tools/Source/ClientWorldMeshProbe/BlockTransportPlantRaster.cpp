#include "BlockTransportPlantProbe.h"

namespace mesh_probe::plant_probe {
namespace {
struct Capture {std::vector<Words> keys;Image image;};
Capture capture(Fixture& f,const WorldColumnGpu& mesh,const WorldCamera& camera) {
  const auto start=std::chrono::steady_clock::now();Capture result;
  result.image=f.render(mesh,camera,false,false);
  Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
  checked(f.renderer.device->readTexture(f.targets[5],0,0,bytes.writeRef(),&layout),"BT plant exact raster key readback");
  require(bytes && layout.colPitch==sizeof(Words) && layout.rowPitch>=Fixture::Size*sizeof(Words),
      "BT plant exact integer MRT layout");
  result.keys.resize(Fixture::Size*Fixture::Size);
  for(unsigned y=0;y<Fixture::Size;++y)
    std::memcpy(result.keys.data()+y*Fixture::Size,static_cast<const char*>(bytes->getBufferPointer())+y*layout.rowPitch,
        Fixture::Size*sizeof(Words));
  cap(f.renderer,start);return result;
}
}
void raster(Fixture& f,unsigned material,unsigned layer) {
  std::vector<Face> four,two;
  for(unsigned direction=6;direction<10;++direction) {
    Face face{std::bit_cast<unsigned>(-48),std::bit_cast<unsigned>(-24),112,material|(direction<<16)};
    four.push_back(face);if((direction&1)==0)two.push_back(face);
  }
  const auto duplicates=f.unit_mesh(four,-32,32),canonical=f.unit_mesh(two,-32,32);unsigned pixels=0;
  for(unsigned plane=0;plane<2;++plane)for(unsigned side=0;side<2;++side) {
    const auto n=normal(plane,side);const Vector center{-47.5f,-23.5f,112.5f};
    const Vector eye{center[0]+3*n[0],center[1],center[2]+3*n[2]};
    const WorldCamera camera{eye[0],eye[1],eye[2],std::atan2(-n[0],n[2]),0,1.03f};
    const auto a=capture(f,duplicates.gpu,camera),b=capture(f,canonical.gpu,camera);
    require(a.keys==b.keys,"BT plant duplicated submitted faces changed canonical raster keys");
    unsigned count=0;
    for(std::size_t i=0;i<a.keys.size();++i) {
      const auto& key=a.keys[i];if(key[3]==0)continue;
      require(std::bit_cast<int>(key[0])==-48 && std::bit_cast<int>(key[1])==-24 && key[2]==112 &&
          key[3]==tag(layer,plane,side)+1,"BT plant production raster plane-side exact key");
      unsigned voxel=0;std::memcpy(&voxel,a.image.mrt[2].data()+i*4,4);
      require((voxel&768u)==768u && ((voxel>>10)&1)==plane && ((voxel>>11)&1)==side &&
          ((voxel>>3)&31)==layer,"BT plant raster true-normal encoding damaged material or side");
      std::uint16_t local_u=0;std::memcpy(&local_u,a.image.mrt[0].data()+i*8+6,2);
      require(local_u==0xbc00u,"BT cutout plants must stay outside solid-plane reconstruction");
      ++count;
    }
    require(count>32,"BT plant raster missed one plane side");pixels+=count;
  }
  std::printf("block_transport_plants_raster=passed production_mrt=1 duplicate_submissions=1 planes=2 sides=2 signed_key=1 true_normals=1 pixels=%u\n",pixels);
}
}
