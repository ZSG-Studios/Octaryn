#include "SceneHierarchy.h"
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <string_view>

int test_scene_hierarchy(const std::filesystem::path&);
int audit_scene_hierarchy(const std::filesystem::path&,std::uint32_t);
int prepare_scene_hierarchy_cli(int argc,char** argv) {
  using namespace octaryn::client::rendering::virtual_geometry;
  const auto path=[](const char* p){return std::filesystem::path(reinterpret_cast<const char8_t*>(p));};
  const auto number=[](const char* p) {
    std::uint64_t value{};const std::string_view text(p);const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size())throw std::runtime_error("invalid hierarchy primitive/count argument");return value;
  };
  if(argc==3 && std::string_view(argv[1])=="--hierarchy-test")return test_scene_hierarchy(path(argv[2]));
  if(argc==4 && std::string_view(argv[1])=="--hierarchy-audit") {
    const auto primitive=number(argv[3]);if(primitive>UINT32_MAX)throw std::runtime_error("hierarchy audit primitive exceeds uint32");
    return audit_scene_hierarchy(path(argv[2]),std::uint32_t(primitive));
  }
  const bool window=std::string_view(argv[1])=="--hierarchy-window";
  if(!window && std::string_view(argv[1])!="--hierarchy")throw std::runtime_error("invalid hierarchy command or argument count");
  if((!window && argc!=4 && argc!=6 && argc!=7) || (window && argc!=5 && argc!=6))
    throw std::runtime_error("usage: --hierarchy catalog.json hierarchy.json [first_primitive primitive_count [target_triangles]] | --hierarchy-window catalog.json hierarchy.json primitive [target_triangles]");
  SceneHierarchyRequest request;request.catalog=path(argv[2]);request.output=path(argv[3]);
  if(!window && argc>=6) {request.first_primitive=number(argv[4]);request.primitive_count=number(argv[5]);}
  if(window) {request.first_primitive=number(argv[4]);request.primitive_count=1;request.maximum_new_leaves=1;}
  if((window && argc==6) || (!window && argc==7)) {
    const auto target=number(argv[window?5:6]);
    if(target==0 || target>65536)throw std::runtime_error("hierarchy target must be1..65536 triangles");
    request.target_triangles=std::uint32_t(target);
  }
  SceneHierarchyProgress result;std::string error;
  const auto progress=[](const SceneHierarchyProgress& p) {
    std::printf("scene_hierarchy_progress primitives=%llu/%llu leaves=%llu/%llu roots=%llu root_pages=%llu root_triangles=%llu metadata_bytes=%llu encoded_bytes=%llu root_payload_bytes=%llu complete=%u\n",
        static_cast<unsigned long long>(p.completed_primitives),static_cast<unsigned long long>(p.total_primitives),
        static_cast<unsigned long long>(p.completed_leaves),static_cast<unsigned long long>(p.total_leaves),
        static_cast<unsigned long long>(p.forest_roots),static_cast<unsigned long long>(p.root_pages),
        static_cast<unsigned long long>(p.root_triangles),static_cast<unsigned long long>(p.root_metadata_bytes),
        static_cast<unsigned long long>(p.root_encoded_bytes),static_cast<unsigned long long>(p.root_payload_bytes),unsigned(p.complete));std::fflush(stdout);
  };
  if(!prepare_scene_hierarchy(request,result,error,nullptr,progress))throw std::runtime_error(error);
  std::printf("scene_hierarchy_prepared complete=%u original_source_unchanged=1 leaf_full_dags_required=0\n",unsigned(result.complete));return 0;
}
