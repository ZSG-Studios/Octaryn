#include "BlockTransportSetup.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace mesh_probe {
namespace {
using Words=std::array<unsigned,4>;
using HalfPixel=std::array<std::uint16_t,4>;
template<class T> std::vector<T> read(Fixture& f,unsigned target) {
  Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
  checked(f.renderer.device->readTexture(f.targets[target],0,0,bytes.writeRef(),&layout),"BTGI local UV MRT readback");
  require(bytes && layout.colPitch==sizeof(T) && layout.rowPitch>=Fixture::Size*sizeof(T) &&
      bytes->getBufferSize()>=layout.rowPitch*Fixture::Size,"BTGI local UV MRT layout");
  std::vector<T> values(Fixture::Size*Fixture::Size);
  for(unsigned y=0;y<Fixture::Size;++y)
    std::memcpy(values.data()+y*Fixture::Size,static_cast<const char*>(bytes->getBufferPointer())+y*layout.rowPitch,
        Fixture::Size*sizeof(T));
  return values;
}
float half(std::uint16_t bits) {
  const unsigned exponent=(bits>>10)&31,mantissa=bits&1023;
  require(exponent!=31,"BTGI local UV is nonfinite");
  return ((bits&32768)?-1.f:1.f)*std::ldexp(float(exponent?mantissa+1024:mantissa),exponent?int(exponent)-25:-24);
}
struct Capture {Image image;std::vector<HalfPixel> metadata;std::vector<Words> keys;};
Capture capture(Fixture& f,const WorldColumnGpu& mesh,const WorldCamera& camera) {
  Capture result;result.image=f.render(mesh,camera,false,false);
  result.metadata=read<HalfPixel>(f,4);result.keys=read<Words>(f,5);return result;
}
std::array<float,2> uv(const Capture& image,unsigned pixel) {
  std::uint16_t u{};std::memcpy(&u,image.image.mrt[0].data()+pixel*8+6,sizeof(u));
  return {half(u),half(image.metadata[pixel][3])};
}
}
void block_transport_reconstruction_raster_cases(Fixture& f) {
  auto source=column(-2,3,-32,32);unsigned stone=0;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"BTGI reconstruction raster stone material");
  for(int z=5;z<27;++z)for(int y=3;y<12;++y)for(int x=4;x<28;++x)
    put(source,x,y,z,static_cast<std::uint16_t>(stone));
  const auto merged=f.mesh(source);
  const auto units=f.unit_mesh(f.verify("block_transport_reconstruction",source,merged),-32,32);
  const std::array<float,3> center{-48,-24.5f,112};unsigned checked_pixels=0;
  for(unsigned direction=0;direction<6;++direction) {
    const auto start=std::chrono::steady_clock::now();
    const unsigned normal=direction/2,u=normal==0?2:0,v=normal==1?2:1;
    auto eye=center;eye[normal]+=(direction&1)?45.f:-45.f;
    const float dx=center[0]-eye[0],dy=center[1]-eye[1],dz=center[2]-eye[2];
    const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    const WorldCamera camera{eye[0],eye[1],eye[2],std::atan2(dx,-dz),std::asin(dy/distance),1.03f};
    const auto actual=capture(f,merged.gpu,camera),expected=capture(f,units.gpu,camera);
    require(actual.keys==expected.keys,"BTGI local UV changed greedy/unit face identity");
    unsigned varied=0;
    for(unsigned pixel=0;pixel<actual.keys.size();++pixel) {
      const auto& key=actual.keys[pixel];if(key[3]==0)continue;
      require(key[3]==direction+1,"BTGI local UV raster used wrong face direction");
      const auto a=uv(actual,pixel),b=uv(expected,pixel);
      require(a[0]>=0 && a[0]<=1 && a[1]>=0 && a[1]<=1,"BTGI opaque raster lacks normalized local UV");
      require(std::abs(a[0]-b[0])<=.002f && std::abs(a[1]-b[1])<=.002f,"BTGI greedy and unit local UV differ");
      std::array<float,4> relative{};
      std::memcpy(relative.data(),actual.image.mrt[1].data()+pixel*sizeof(relative),sizeof(relative));
      const float expected_u=std::clamp(relative[u]+eye[u]-float(std::bit_cast<int>(key[u])),0.f,1.f);
      const float expected_v=std::clamp(relative[v]+eye[v]-float(std::bit_cast<int>(key[v])),0.f,1.f);
      require(std::abs(a[0]-expected_u)<=.002f && std::abs(a[1]-expected_v)<=.002f,
          "BTGI local UV metadata does not match the geometric tangent position");
      require(actual.metadata[pixel][0]==0 && actual.metadata[pixel][1]==0 && actual.metadata[pixel][2]==0,
          "BTGI local UV changed the emissive RGB channels");
      if(a[0]>.1f && a[0]<.9f && a[1]>.1f && a[1]<.9f)++varied;
      ++checked_pixels;
    }
    require(varied>20,"BTGI local UV raster fixture lacks interior samples");
    block_transport_complete(f.renderer,start);
  }
  require(f.renderer.debug.errors.load()==0,"BTGI local UV graphics validation errors");
  std::printf("block_transport_reconstruction_raster=passed hardware=1 production_mrt=1 face_directions=6 merged_unit_uv=1 checked_pixels=%u validation_errors=0\n",checked_pixels);
}
}
