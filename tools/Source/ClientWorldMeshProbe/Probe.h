#pragma once
#include "WorldRendererInternal.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mesh_probe {
using namespace octaryn::client::rendering;
using octaryn::client::world_presentation::StreamColumn;
using Face=std::array<std::uint32_t,4>;
struct Block {
  std::string id,fluidKind;
  bool opaque{},sprite{},solid{},occlusion{},requiresSolidBase{};
  int fluidLevel{};
};
struct Catalog { std::string schema;std::vector<Block> blocks; };
struct Mesh {
  WorldColumnGpu gpu;
  std::vector<Face> faces;
  std::vector<std::uint32_t> patches;
  std::vector<std::array<float,8>> fluids;
};
struct Image {
  std::array<std::vector<unsigned char>,4> mrt;
  std::vector<float> depth;
};
void require(bool,const char*);
void checked(SlangResult,const char*);
unsigned pass(const Block&);
StreamColumn column(int x=0,int z=0,int min_y=0,int height=32);
void put(StreamColumn&,int x,int y,int z,std::uint16_t block);
struct Fixture {
  static constexpr unsigned Size=128;
  WorldRenderer renderer;
  std::vector<Block> catalog;
  std::array<Slang::ComPtr<rhi::ITexture>,4> targets;
  std::array<Slang::ComPtr<rhi::ITextureView>,4> views;
  explicit Fixture(bool batch_capacity=false,bool ray_tracing=false);
  Mesh mesh(const StreamColumn&);
  Mesh read_mesh(const WorldColumnGpu&) const;
  Mesh unit_mesh(const std::vector<Face>&,int min_y,int height);
  std::uint16_t sample(const StreamColumn&,int x,int y,int z) const;
  std::vector<Face> expected(const StreamColumn&) const;
  std::vector<Face> verify(const char*,const StreamColumn&,const Mesh&) const;
  void verify_fluids(const StreamColumn&,const Mesh&) const;
  Image render(const WorldColumnGpu&,const WorldCamera&,bool pbr,bool pom,bool retained_columns=false);
  void compare_raster(const char*,const Mesh&,const Mesh&,const WorldCamera&,bool pbr,bool pom);
};
Slang::ComPtr<rhi::IBuffer> buffer(WorldRenderer&,const void*,std::size_t,unsigned,rhi::BufferUsage);
void surface_cases(Fixture&);
void greedy_output_cases(Fixture&);
void greedy_output_timing(Fixture&);
void raster_cases(Fixture&);
void binding_cases(Fixture&);
void batch_cases(Fixture&);
void frames_cases(Fixture&);
void forward_temporal_cases(Fixture&);
void ray_tracing_cases(Fixture&);
void lighting_temporal_cases(Fixture&);
void direct_lighting_cases(Fixture&);
void ddgi_volume_cases(Fixture&);
void ddgi_response_cases(Fixture&);
void ddgi_transition_cases(Fixture&);
void ddgi_leak_cases(Fixture&);
void ddgi_dark_room_cases(Fixture&);
void culling_cases(Fixture&);
void halo_lifecycle_cases(Fixture&);
void delivery_lifecycle_cases(Fixture&);
void dual_delivery_cases(Fixture&);
void relative_precision_cases(Fixture&);
void seam_cases(Fixture&);
void patch_coordinate_cases(Fixture&);
void atlas_filtering_cases(Fixture&);
void atlas_mip_cases();
void atlas_animation_cases(Fixture&);
struct SamplingBoundaries {std::vector<bool> albedo,material;};
SamplingBoundaries sampling_boundaries(Fixture&,const Mesh&,const Mesh&,const WorldCamera&,bool,bool,
                                      const std::vector<bool>&,const Image&,const Image&);
std::vector<std::array<float,4>> sampling_reference(Fixture&,const std::vector<std::array<float,8>>&);
float projected_edge_distance(const Mesh&,const WorldCamera&,unsigned,unsigned);
}
