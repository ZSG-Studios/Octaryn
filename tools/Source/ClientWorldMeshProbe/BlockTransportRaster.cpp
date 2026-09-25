#include "BlockTransportSetup.h"
#include "BlockTransportRasterTrace.h"
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <set>
#include <thread>

namespace mesh_probe {
namespace {
using Key=std::array<std::uint32_t,4>;
std::vector<Key> keys(Fixture& f,const WorldColumnGpu& mesh,const WorldCamera& camera,const char* name) {
  BlockTransportRasterTrace trace(f.renderer,name);trace.mark("render_begin");
  {
    const auto rendered=f.render(mesh,camera,false,false);trace.mark("render_returned");
    unsigned covered=0;
    for(float depth:rendered.depth) {
      require(std::isfinite(depth) && depth>=0 && depth<=1,"BTGI raster depth must remain finite and normalized");
      if(depth<1)++covered;
    }
    require(covered>32,"BTGI owned depth attachment has no meaningful covered samples");
  }
  trace.mark("render_image_destroyed");
  require(f.targets.size()>5 && f.targets[5],"BTGI exact Gbuffer attachment missing");
  Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
  trace.mark("key_readback_begin");
  checked(f.renderer.device->readTexture(f.targets[5],0,0,bytes.writeRef(),&layout),"BTGI raster key readback");
  require(bytes && layout.colPitch==sizeof(Key) && layout.rowPitch>=Fixture::Size*sizeof(Key),
      "BTGI raster key integer format mismatch");
  trace.mark("key_readback_complete");
  std::vector<Key> result(Fixture::Size*Fixture::Size);
  for(unsigned y=0;y<Fixture::Size;++y)
    std::memcpy(result.data()+y*Fixture::Size,static_cast<const char*>(bytes->getBufferPointer())+y*layout.rowPitch,
        Fixture::Size*sizeof(Key));
  trace.mark("key_copy_complete");bytes.setNull();trace.mark("key_blob_destroyed");
  return result;
}
}
void block_transport_raster_cases(Fixture& f) {
  BlockTransportRasterTrace trace(f.renderer,"identity_cases");
  auto column_data=column(-2,3,-32,32);
  unsigned stone=0;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"BTGI raster fixture stone material");
  for(int z=5;z<27;++z)for(int y=3;y<12;++y)for(int x=4;x<28;++x)
    put(column_data,x,y,z,static_cast<std::uint16_t>(stone));
  auto merged=f.mesh(column_data);
  trace.mark("merged_mesh_ready");
  auto unit_faces=f.verify("block_transport_raster",column_data,merged);trace.mark("unit_mesh_begin",unsigned(unit_faces.size()));
  auto unit=f.unit_mesh(unit_faces,-32,32);trace.mark("unit_mesh_complete");
  std::vector<Face>().swap(unit_faces);trace.mark("unit_cpu_faces_destroyed");
  const float center[3]={-48,-24.5f,112};
  unsigned checked_pixels=0;
  for(unsigned direction=0;direction<6;++direction) {
    const auto start=std::chrono::steady_clock::now();
    const unsigned axis=direction/2;
    std::array<float,3> eye{center[0],center[1],center[2]};
    eye[axis]+=(direction&1)?45.f:-45.f;
    const float dx=center[0]-eye[0],dy=center[1]-eye[1],dz=center[2]-eye[2];
    const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    const WorldCamera camera{eye[0],eye[1],eye[2],std::atan2(dx,-dz),std::asin(dy/distance),1.03f};
    if(direction==0) {
      auto& r=f.renderer;r.active_frame=1;
      require(r.frame_queue.wait(r.active_frame,2000),"BTGI deliberate rotated raster slot fence");
      require(r.targets[0].depth && r.targets[0].depth_view && !r.targets[1].depth && !r.targets[1].depth_view,
          "BTGI fixture must expose slot-zero-only depth ownership");
    }
    trace.mark("direction_begin",direction);
    {
      const auto actual=keys(f,merged.gpu,camera,"merged_keys");
      const auto expected=keys(f,unit.gpu,camera,"unit_keys");trace.mark("comparison_begin",direction);
      require(actual==expected,"BTGI greedy and unit-face rasterizations disagree on exact surface identity");
      std::set<Key> distinct;
      for(const auto& key:actual) {
        if(key[3]==0)continue;
        require(key[3]==direction+1,"BTGI raster key used opposite or interpolated face direction");
        const int x=std::bit_cast<std::int32_t>(key[0]),y=std::bit_cast<std::int32_t>(key[1]),
            z=std::bit_cast<std::int32_t>(key[2]);
        require(x>=-60 && x<-36 && y>=-29 && y<-20 && z>=101 && z<123,
            "BTGI raster key escaped signed block bounds");
        const int coordinates[3]={x,y,z};
        const int lower[3]={-60,-29,101},upper[3]={-37,-21,122};
        require(coordinates[axis]==((direction&1)?upper[axis]:lower[axis]),
            "BTGI positive face plane selected adjacent empty cell");
        distinct.insert(key);++checked_pixels;
      }
      require(distinct.size()>20,"BTGI raster fixture missed merged-face cell boundaries");
      require(f.renderer.active_frame==1,"BTGI raster changed the caller-selected runtime frame slot");
      trace.mark("comparison_complete",direction);
    }
    trace.mark("key_vectors_destroyed",direction);
    block_transport_complete(f.renderer,start);
  }

  const auto leaf_start=std::chrono::steady_clock::now();
  unsigned leaves=0;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.leaves")leaves=i;
  require(leaves && f.catalog[leaves].opaque && !f.catalog[leaves].occlusion && !f.catalog[leaves].sprite,
      "BTGI raster leaf fixture must be an opaque non-occluding cube");
  auto leaf_column=column(-2,3,-32,32);put(leaf_column,16,8,16,static_cast<std::uint16_t>(leaves));
  const auto leaf_mesh=f.mesh(leaf_column);
  const WorldCamera leaf_camera{-47.5f,-23.5f,116,0,0,1.03f};
  const auto leaf_keys=keys(f,leaf_mesh.gpu,leaf_camera,"leaf_keys");
  unsigned leaf_pixels=0;
  for(const auto& key:leaf_keys)if(key[3]!=0) {
    require(std::bit_cast<std::int32_t>(key[0])==-48 && std::bit_cast<std::int32_t>(key[1])==-24 &&
        std::bit_cast<std::int32_t>(key[2])==112 && key[3]>=1 && key[3]<=6,
        "BTGI leaf raster surface key escaped its signed cube");
    ++leaf_pixels;
  }
  require(leaf_pixels>32,"BTGI non-occluding opaque leaf pixels have no surface key");
  std::printf("block_transport_leaves_raster=passed production_mrt=1 opaque_non_occluding=1 signed_key=1 pixels=%u\n",leaf_pixels);
  block_transport_complete(f.renderer,leaf_start);
  require(f.renderer.debug.errors.load()==0,"BTGI raster identity graphics validation errors");
  std::printf("block_transport_raster=passed hardware=1 production_mrt=1 signed_coordinates=1 face_directions=6 greedy_unit_parity=1 owned_depth=1 rotated_slot=1 preserved_slot=1 checked_pixels=%u validation_errors=0\n",checked_pixels);
}
}
