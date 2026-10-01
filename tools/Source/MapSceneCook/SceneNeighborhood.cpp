#include "ScenePreparation.h"
#include <charconv>
#include <cstdio>
#include <csignal>
#include <stdexcept>
#include <string_view>

namespace {
std::atomic_bool canceled{};
void interrupt(int) {canceled.store(true);}
float number(const char* input) {
  float value{};const std::string_view text(input);const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())throw std::runtime_error("invalid scene position/radius");return value;
}
std::filesystem::path path(const char* value) {return std::filesystem::path(reinterpret_cast<const char8_t*>(value));}
}
int prepare_scene_neighborhood_cli(int argc,char** argv) {
  using namespace octaryn::client::rendering::virtual_geometry;
  ScenePreparationRequest request;request.source=path(argv[2]);request.catalog=path(argv[3]);
  request.camera=request.actor={number(argv[4]),number(argv[5]),number(argv[6])};
  if(argc==9) {request.render_radius=number(argv[7]);request.collision_radius=number(argv[8]);}
  std::signal(SIGINT,interrupt);ScenePreparationResult result;std::string error;
  const auto notify=[](const ScenePreparationProgress& p) {
    if(p.completed%16 && p.completed!=p.requested)return;
    std::printf("scene_prepare_progress stage=%u completed=%llu requested=%llu bounds=%llu cooked=%llu total=%llu\n",
        unsigned(p.stage),static_cast<unsigned long long>(p.completed),static_cast<unsigned long long>(p.requested),
        static_cast<unsigned long long>(p.prepared_bounds),static_cast<unsigned long long>(p.cooked_parts),static_cast<unsigned long long>(p.total_parts));
    std::fflush(stdout);
  };
  const bool ok=prepare_scene_neighborhood(request,result,error,&canceled,notify);
  std::printf("scene_prepare_result neighborhood_ready=%u full_scene_ready=%u render_parts=%llu collision_pairs=%llu render_bytes=%llu "
      "collision_bytes=%llu pending_bounds=%zu pending_cooks=%zu reason=%s\n",unsigned(result.neighborhood_ready),unsigned(result.full_scene_ready),
      static_cast<unsigned long long>(result.render_parts),static_cast<unsigned long long>(result.collision_pairs),
      static_cast<unsigned long long>(result.render_bytes),static_cast<unsigned long long>(result.collision_bytes),
      result.pending_bounds.size(),result.pending_cooks.size(),error.c_str());
  return ok?0:1;
}
int qualify_scene_spawn_cli(char** argv) {
  using namespace octaryn::client::rendering::virtual_geometry;
  std::array<float,3> output{};std::string error;
  const bool ok=qualify_scene_spawn(path(argv[3]),path(argv[2]),
      {number(argv[4]),number(argv[5]),number(argv[6])},output,error,&canceled);
  std::printf("scene_spawn_qualified passed=%u x=%.9g y=%.9g z=%.9g reason=%s\n",unsigned(ok),output[0],output[1],output[2],error.c_str());
  return ok?0:1;
}
