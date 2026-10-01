#include "MapSource.h"
#include "MapSourceReader.h"
#include "GltfTriangleReader.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
bool test_map_import(const std::filesystem::path&);
int main(int argc,char** argv) {
  try {
    if(argc==8 && std::string_view(argv[1])=="--collision-range") {
      octaryn::assets::GltfTriangleReader reader;octaryn::assets::GltfTriangleWindow window;std::string error;
      const auto mesh=std::stoull(argv[3]),primitive=std::stoull(argv[4]),count=std::stoull(argv[6]);
      if(mesh>UINT32_MAX || primitive>UINT32_MAX || count>65536)throw std::runtime_error("collision range arguments exceed limits");
      if(!reader.open(std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2])),argv[7],error) ||
          !reader.read(std::uint32_t(mesh),std::uint32_t(primitive),std::stoull(argv[5]),std::uint32_t(count),window,error))
        throw std::runtime_error(error);
      std::printf("collision_source_range=passed mesh=%llu primitive=%llu triangles=%zu vertices=%zu object_space=1 full_scene_loaded=0 "
          "bounds=%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",mesh,primitive,window.indices.size()/3,window.positions.size()/3,
          window.bounds[0],window.bounds[1],window.bounds[2],window.bounds[3],window.bounds[4],window.bounds[5]);return 0;
    }
    if(argc==8 && std::string_view(argv[1])=="--range") {
      MapSourceReader reader;MapModel model;std::string error;
      const std::filesystem::path path(reinterpret_cast<const char8_t*>(argv[2]));
      if(!reader.open(path,argv[7],error) || !reader.load(std::stoull(argv[3]),std::stoull(argv[4]),
          std::stoull(argv[5]),std::stoull(argv[6]),model,error))throw std::runtime_error(error);
      std::printf("map_source_range=passed mesh=%s primitive=%s first_triangle=%s triangles=%zu vertices=%zu "
          "full_scene_loaded=0 object_space=1\n",argv[3],argv[4],argv[5],model.indices.size()/3,model.vertices.size());return 0;
    }
    if(argc==3 && std::string_view(argv[1])=="--self-test")return test_map_import(argv[2])?0:1;
    if(argc!=4)throw std::runtime_error("usage: map_import_probe source.gltf mesh primitive | --self-test directory");
    const std::filesystem::path path(reinterpret_cast<const char8_t*>(argv[1]));
    MapSourceInfo info;std::string error;
    if(!inspect_map_source(path,info,error))throw std::runtime_error(error);
    std::printf("map_source_inspection=passed meshes=%zu primitives=%zu instances=%zu materials=%zu compressed_views=%zu "
        "external_bytes=%llu logical_bytes=%llu unique_triangles=%llu instanced_triangles=%llu\n",
        info.mesh_count,info.primitives.size(),info.instances.size(),info.material_count,info.compressed_views,
        static_cast<unsigned long long>(info.external_bytes),static_cast<unsigned long long>(info.logical_bytes),
        static_cast<unsigned long long>(info.unique_triangles),static_cast<unsigned long long>(info.instanced_triangles));
    MapModel whole;
    const bool accepted=load_map_model(path,whole,error);
    if(info.instanced_triangles>MapLoadLimits{}.triangles) {
      if(accepted || error.find("streamed instance cook")==std::string::npos)throw std::runtime_error("large-scene preparation did not reject before decode");
      std::printf("map_source_residency bounded_rejection=1 reason=%s\n",error.c_str());
    }
    const auto mesh=std::stoull(argv[2]),primitive=std::stoull(argv[3]);MapModel model;
    if(!load_map_source_primitive(path,mesh,primitive,model,error))throw std::runtime_error(error);
    std::size_t checked{};
    for(const auto& vertex:model.vertices) {
      for(const auto value:vertex.position)if(!std::isfinite(value))throw std::runtime_error("nonfinite decoded position");
      const auto& n=vertex.normal;const float length=n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
      if(!std::isfinite(length) || std::abs(length-1.f)>.001f)throw std::runtime_error("invalid decoded normal");
      ++checked;
    }
    if(model.primitives.size()!=1 || model.indices.empty())throw std::runtime_error("selective preparation lost primitive");
    std::printf("map_source_primitive=passed mesh=%llu primitive=%llu triangles=%zu vertices=%zu normals=%zu "
        "object_space=1 full_scene_loaded=0\n",static_cast<unsigned long long>(mesh),static_cast<unsigned long long>(primitive),
        model.indices.size()/3,model.vertices.size(),checked);return 0;
  }catch(const std::exception& exception) {std::fprintf(stderr,"map_import_probe_failed: %s\n",exception.what());return 1;}
}
