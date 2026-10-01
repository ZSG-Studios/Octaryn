#include "SceneRootPages.h"
#include "SceneSelectionBudget.h"
#include <array>
#include <cstdio>
#include <random>
#include <stdexcept>
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
}
int main() {
  try {
    std::mt19937 random(41327);
    for(unsigned round=0;round<200;++round) {
      std::vector<std::uint32_t> sizes(1+random()%500);
      for(auto& size:sizes)size=1+random()%page_bytes;
      const auto count=SceneRootPages::required_slots(sizes);SceneRootPages pages(count);
      std::vector<SceneRootSpan> spans;std::uint64_t expected{};
      for(auto size:sizes) {
        const auto span=pages.reserve(size);require(bool(span),"packing estimator undercounted slots");
        require(span.offset%16==0 && span.bytes==(size+15u)/16u*16u && span.offset+span.bytes<=page_bytes,"invalid root slice bounds");
        for(const auto other:spans)if(other.slot==span.slot)
          require(span.offset>=other.offset+other.bytes || other.offset>=span.offset+span.bytes,"packed root slices overlap");
        spans.push_back(span);expected+=span.bytes;
      }
      require(pages.bytes()==expected,"packed byte accounting differs");
      for(const auto span:spans) {
        require(!pages.release(span,9,8) && pages.valid(span),"pending consumer released packed root");
        require(pages.release(span,9,9) && !pages.valid(span),"completed root did not release");
        require(!pages.release(span,9,9),"stale root released twice");
      }
      require(pages.bytes()==0,"root retirement leaked bytes");
      for(unsigned i=0;i<count;++i)require(bool(pages.reserve(page_bytes)),"root free ranges did not coalesce");
      require(!pages.reserve(1),"root allocator exceeded physical capacity");
    }
    SceneRootPages generation(1);const auto first=generation.reserve(32);
    require(generation.release(first,0,0),"root generation initial release");
    const auto second=generation.reserve(32);
    require(first.slot==second.slot && first.offset==second.offset && first.generation!=second.generation,"root reuse lost generation");
    require(!generation.release(first,0,0) && generation.valid(second),"stale generation aliases replacement");
    require(SceneRootPages::required_slots(std::array<std::uint32_t,1>{0})==invalid_id,"empty root payload accepted");
    require(SceneRootPages::required_slots(std::array<std::uint32_t,1>{page_bytes+1})==invalid_id,"oversize root payload accepted");
    GeometryAsset asset;asset.pages.resize(2);asset.clusters.resize(3);
    asset.clusters[0].vertex_count=3;asset.clusters[0].triangle_offset=240;asset.clusters[0].triangle_count=1;
    asset.clusters[1].page=1;asset.clusters[1].vertex_count=3;asset.clusters[1].flags=geometry_position_only;
    asset.clusters[1].triangle_offset=36;asset.clusters[1].triangle_count=1;
    asset.clusters[2].page=1;asset.clusters[2].vertex_offset=400;asset.clusters[2].vertex_count=2;
    require(geometry_page_payload_bytes(asset)==std::vector<std::uint32_t>({256,560}),"page extent omitted shared fine clusters");
    asset.clusters[2].vertex_offset=page_bytes;
    bool rejected=false;try {geometry_page_payload_bytes(asset);}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"payload exceeding page accepted");
    SelectionResourcesConfig c;c.groups=c.clusters=c.pages=c.page_references=c.parents=c.instances=c.feedback_capacity=1;
    c.frame_count=2;c.readback_bytes=64;
    require(scene_selection_bytes(c)==2*(48+32+8+28+4+8+16+48+192+64),"shared selection physical bytes mismatch");
    require(scene_selection_feedback_bytes(1,1)==48,"feedback slice alignment mismatch");
    std::printf("scene_root_pages passed=1 checks=%u estimator=best_fit payloads=all_clusters fences=1 generations=1 physical_selection_bytes=1\n",checks);
    return 0;
  }catch(const std::exception& error) {std::fprintf(stderr,"scene_root_pages failed=%s\n",error.what());return 1;}
}
