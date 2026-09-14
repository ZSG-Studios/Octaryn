#include "WorldRendererInternal.h"
#include "TemporalCapture.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
namespace octaryn::client::rendering {
namespace {
std::uint16_t source_block(const WorldRenderer& r,int x,int y,int z) {
  const int cx=x/32-(x%32<0),cz=z/32-(z%32<0);
  const auto found=r.sources.find({cx,cz});if(found==r.sources.end()) return 0;
  const auto& column=found->second;const int local_y=y-column.min_y;
  if(local_y<0 || local_y>=column.height) return 0;
  return column.blocks[static_cast<std::size_t>(x-cx*32)+32u*(static_cast<unsigned>(local_y)+
      static_cast<unsigned>(column.height)*static_cast<unsigned>(z-cz*32))];
}
bool capture_fluid_column(WorldRenderer& r,const std::pair<std::int32_t,std::int32_t>& coordinate,
                          const WorldColumnGpu& column,const void* face_data,std::ofstream& file) {
  const auto count=column.pass_counts[3]+column.pass_counts[4];
  const std::uint32_t header[]={static_cast<std::uint32_t>(coordinate.first),static_cast<std::uint32_t>(coordinate.second),count};
  file.write(reinterpret_cast<const char*>(header),sizeof(header));if(!count) return true;
  Slang::ComPtr<ISlangBlob> data;
  if(SLANG_FAILED(r.device->readBuffer(column.fluids,0,count*32ull,data.writeRef())) || !data || data->getBufferSize()!=count*32ull) return false;
  const auto* faces=static_cast<const std::int32_t*>(face_data)+(column.face_count-count)*4ull;
  const auto* fluids=static_cast<const char*>(data->getBufferPointer());
  for(std::uint32_t i=0;i<count;++i) {
    const auto* face=faces+i*4ull;std::uint16_t neighbors[27]{};
    for(int z=-1;z<=1;++z) for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
      neighbors[x+1+3*(y+1+3*(z+1))]=source_block(r,face[0]+x,face[1]+y,face[2]+z);
    file.write(reinterpret_cast<const char*>(face),16);file.write(fluids+i*32ull,32);
    file.write(reinterpret_cast<const char*>(neighbors),sizeof(neighbors));
  }
  return static_cast<bool>(file);
}
bool capture_mesh(WorldRenderer& r,const char* path) {
  auto mesh_path=std::filesystem::path(reinterpret_cast<const char8_t*>(path));
  mesh_path += ".quads.bin";
  std::ofstream file(mesh_path,std::ios::binary);
  auto fluid_path=std::filesystem::path(reinterpret_cast<const char8_t*>(path));fluid_path+=".fluids.bin";
  std::ofstream fluid_file(fluid_path,std::ios::binary);
  if (!file || !fluid_file) return false;
  fluid_file.write("OCFLUID1",8);const auto columns=static_cast<std::uint32_t>(r.columns.size());
  fluid_file.write(reinterpret_cast<const char*>(&columns),sizeof(columns));
  for (const auto& [coordinate,column]:r.columns) {
    // Each record is signed column X/Z, uint32 count, then count uint4 GPU faces.
    const std::uint32_t header[]={static_cast<std::uint32_t>(coordinate.first),
        static_cast<std::uint32_t>(coordinate.second),column.face_count};
    file.write(reinterpret_cast<const char*>(header),sizeof(header));
    if (!column.face_count) {
      if(!capture_fluid_column(r,coordinate,column,nullptr,fluid_file)) return false;
      continue;
    }
    Slang::ComPtr<ISlangBlob> faces;
    const auto bytes=column.face_count*16ull;
    if (SLANG_FAILED(r.device->readBuffer(column.faces,0,bytes,faces.writeRef())) ||
        !faces || faces->getBufferSize()!=bytes) return false;
    file.write(static_cast<const char*>(faces->getBufferPointer()),static_cast<std::streamsize>(bytes));
    if(!capture_fluid_column(r,coordinate,column,faces->getBufferPointer(),fluid_file)) return false;
  }
  return static_cast<bool>(file);
}
}
bool world_renderer_capture(WorldRenderer& r,const WorldCamera& camera) {
  const char* path=SDL_GetEnvironmentVariable(SDL_GetEnvironment(),"OCTARYN_CLIENT_CAPTURE_PATH");
  const auto expected_columns=static_cast<std::size_t>((2*r.radius+1)*(2*r.radius+1));
  if (!path || !*path || !r.capture_enabled || r.captured || r.frames<120 || r.columns.size()<expected_columns || world_mesh_has_pending(r)) return true;
  if(!r.frame_queue.wait(r.active_frame))return false;
  if(const auto* temporal=SDL_getenv("OCTARYN_CLIENT_CAPTURE_TEMPORAL");temporal && std::string_view(temporal)=="1")
    if(!capture_temporal(r.temporal,r.device,r.target().hdr.scene,r.target().depth,r.active_frame,path))return false;
  Slang::ComPtr<ISlangBlob> pixels;
  rhi::SubresourceLayout layout{};
  if (SLANG_FAILED(r.device->readTexture(r.target().color,0,0,pixels.writeRef(),&layout)) || !pixels || layout.colPitch!=4 ||
      pixels->getBufferSize()<layout.rowPitch*static_cast<rhi::Size>(r.height)) return false;
  const auto format=r.target().color->getDesc().format;
  const bool bgra=format==rhi::Format::BGRA8Unorm || format==rhi::Format::BGRA8UnormSrgb;
  SDL_Surface* surface=SDL_CreateSurfaceFrom(r.width,r.height,
      bgra?SDL_PIXELFORMAT_BGRA32:SDL_PIXELFORMAT_RGBA32,
      const_cast<void*>(pixels->getBufferPointer()),static_cast<int>(layout.rowPitch));
  if (!surface) return false;
  const bool saved=SDL_SaveBMP(surface,path);
  SDL_DestroySurface(surface);
  if (!saved) return false;
  if (!capture_mesh(r,path)) return false;
  const auto* data=static_cast<const unsigned char*>(pixels->getBufferPointer());
  std::uint64_t nonclear{};
  for (int y=0;y<r.height;++y) for (int x=0;x<r.width;++x) {
    const auto* pixel=data+static_cast<rhi::Size>(y)*layout.rowPitch+x*4;
    if (std::abs(int(pixel[bgra?2:0])-102)>1 ||
        std::abs(int(pixel[1])-163)>1 || std::abs(int(pixel[bgra?0:2])-219)>1) ++nonclear;
  }
  r.captured=true;
  std::fprintf(stdout,"world_capture frame=%llu columns=%zu nonclear_pixels=%llu eye=%.6f,%.6f,%.6f yaw=%.6f pitch=%.6f fov=%.6f path=%s\n",
      static_cast<unsigned long long>(r.frames),r.columns.size(),
      static_cast<unsigned long long>(nonclear),camera.x,camera.y,camera.z,
      camera.yaw,camera.pitch,camera.vertical_fov,path);
  std::fflush(stdout);
  return true;
}
}
