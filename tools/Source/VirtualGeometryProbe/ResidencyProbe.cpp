#include "PageResidency.h"
#include "Selection.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace octaryn::client::rendering::virtual_geometry;
namespace {
unsigned checks{};
void check(bool condition,const char* message) {
  ++checks;if(!condition)throw std::runtime_error(message);
}
void retirement() {
  PageResidency pool(8,2,128,3);
  auto root=pool.reserve(0,64,true),page=pool.reserve(1,64);
  check(bool(root) && bool(page),"reserve initial pages");
  check(!pool.reserve(2,1),"budget includes reserved bytes");
  check(!pool.resident(0),"reserved root cannot render");
  check(pool.begin_upload(root,2) && pool.begin_upload(page,3),"submit uploads");
  pool.complete({2,0,0,0});check(pool.resident(0) && !pool.resident(1),"fence gates upload publication");
  check(!pool.evict(0),"pinned root survives pressure");
  pool.complete({3,0,0,0});check(pool.reference(page,{3,5,7,9}),"record all consumers");
  check(pool.evict(1) && !pool.resident(1),"retiring page unmaps immediately");
  check(pool.stats().bytes==128 && pool.stats().retiring==1,"retirement retains byte ownership");
  check(!pool.reserve(1,64) && !pool.reserve(2,64),"inflight slots cannot be reused");
  pool.complete({3,5,7,8});check(pool.valid(page),"pose snapshot retains page");
  pool.complete({3,5,7,9});check(!pool.valid(page),"all fences release page");
  auto next=pool.reserve(2,64);
  check(next.slot==page.slot && next.generation!=page.generation,"slot generation changes on reuse");
  check(!pool.reference(page,{3,10,10,10}) && !pool.begin_upload(page,10),"stale handles rejected");
  check(pool.evict(2) && pool.stats().bytes==64,"cancel unsubmitted reservation");
  auto cancelled=pool.reserve(3,64);check(pool.begin_upload(cancelled,11),"upload cancellation setup");
  check(pool.evict(3),"cancel in-flight upload");
  pool.complete({10,20,20,20});check(pool.valid(cancelled),"cancelled upload retained until fence");
  pool.complete({11,20,20,20});check(!pool.valid(cancelled),"cancelled upload retirement");
  check(pool.pin(0,false) && pool.evict_oldest(),"explicit root unpin permits release");
  check(pool.stats().bytes==0,"all bytes reclaimed");
}
void feedback() {
  PageResidency pool(10,3,192,2);
  const PageRequest values[]={{1,1},{1,8},{2,2},{3,10},{99,1},{4,-1},{4,std::numeric_limits<float>::infinity()}};
  pool.feedback(values);auto requests=pool.take_requests(1);
  check(requests.size()==1 && requests[0].page==3,"feedback retains strongest priority");
  requests=pool.take_requests(9);check(requests.size()==1 && requests[0].page==1 && requests[0].priority==8,"feedback deduplicates priority");
  check(pool.stats().feedback_overflow==1 && pool.stats().invalid_feedback==3,"feedback diagnostics");
  const PageRequest again[]={{0,1},{1,2}};pool.feedback(again);pool.reserve(1,64);
  requests=pool.take_requests(9);check(requests.size()==1 && requests[0].page==0,"admitted feedback is removed");
}
void timeline_handoff() {
  PageResidency pool(4,3,192,4);const auto root=pool.reserve(0,64,true);
  check(!pool.release_upload_timeline(),"handoff must reject pending CPU reservations");
  check(pool.begin_upload(root,100),"startup upload submission");
  check(!pool.release_upload_timeline(),"handoff must reject in-flight uploads");
  pool.complete({100,100,0,0});check(pool.reference(root,{0,101,5,7}),"startup consumer reference");
  pool.complete({100,101,5,6});check(!pool.release_upload_timeline(),"handoff must retain unfinished external consumers");
  pool.complete({100,101,5,7});check(pool.release_upload_timeline(),"idle handoff must succeed");
  check(pool.resident(0) && pool.handle(0)==root && pool.stats().bytes==64,"handoff must preserve residency and generations");
  const auto detail=pool.reserve(1,64);check(pool.begin_upload(detail,1),"new upload timeline may restart below startup signal");
  check(!pool.resident(1),"old completed values must not publish new timeline uploads");
  pool.complete({1,1,5,7});check(pool.resident(1),"new upload fence publishes detail");
  check(pool.reference(detail,{0,2,5,7}) && pool.evict(1) && pool.valid(detail),"new raster timeline protects eviction");
  check(!pool.release_upload_timeline(),"handoff must reject retiring consumers");
  pool.complete({1,2,5,7});check(!pool.valid(detail) && pool.release_upload_timeline(),"completed retirement permits handoff");
}
GeometryAsset fixture() {
  GeometryAsset asset;asset.pages.resize(4);asset.groups.resize(3);
  asset.roots={1,2};asset.group_pages={0,1,2,3};
  asset.groups[0]={0,2,0,0,2,{{0,0,0},1,10}};
  asset.groups[1]={2,1,1,2,1,{{0,0,0},1,std::numeric_limits<float>::max()}};
  asset.groups[2]={3,1,1,3,1,{{0,0,0},1,std::numeric_limits<float>::max()}};
  asset.clusters.resize(4);
  asset.clusters[0].group=asset.clusters[1].group=0;
  asset.clusters[0].page=0;asset.clusters[1].page=1;
  asset.clusters[2].group=1;asset.clusters[2].page=2;asset.clusters[2].refined_group=0;
  asset.clusters[3].group=2;asset.clusters[3].page=3;asset.clusters[3].refined_group=0;
  return asset;
}
void selection() {
  auto asset=fixture();SelectionTopology topology;std::string error;SelectionResult result;
  check(build_selection_topology(asset,topology,error),"DAG topology builds");
  check(topology.groups[0].parent_count==2,"DAG remembers both replacement parents");
  SelectionView view{{0,0,10},100,1};std::vector<GpuPage> pages(4);
  const auto select=[&](unsigned count=4,unsigned requests=4) {
    return select_geometry(topology,pages,view,count,requests,result,error);
  };
  check(!select() && result.missing_roots==2 && result.clusters.empty(),"missing roots refuse incomplete cut");
  pages[2]={2,1,1,0};pages[3]={3,1,1,0};
  check(select() && result.clusters==std::vector<std::uint32_t>({2,3}),"coarse root coverage before detail");
  check(result.requests.size()==2,"missing complete group pages requested");
  pages[0]={0,1,1,0};
  check(select() && result.clusters==std::vector<std::uint32_t>({2,3}),"partial child pages retain every parent");
  pages[1]={1,1,1,0};
  check(select() && result.clusters==std::vector<std::uint32_t>({0,1}),"complete group replaces both parents atomically");
  view.error_pixels=200;
  check(select() && result.clusters==std::vector<std::uint32_t>({2,3}),"screen error selects coarse cut");
  check(!select(1) && result.clusters.empty(),"output overflow never emits partial cut");
  view.error_pixels=1;pages[0]={};pages[1]={};
  check(select(4,1) && result.requests.size()==1 && result.feedback_overflow==1,"feedback overflow keeps complete coarse cut");
  view.frustum=true;view.planes[0][0]=1;view.planes[0][3]=-20;
  check(select() && result.requests.empty() && result.clusters.empty(),"offscreen groups do not request detail");
  view.frustum=false;
  pages[0]={0,0,1,0};check(select() && result.clusters[0]==2,"zero generation cannot satisfy residency");
  asset.groups[0].depth=1;check(!build_selection_topology(asset,topology,error),"hierarchy cycles/nondecreasing depth rejected");
}
void churn() {
  PageResidency pool(128,8,512,8);std::uint64_t frame=1;
  for(unsigned round=0;round<10000;++round) {
    auto h=pool.reserve(round%128,64);
    if(h) {check(pool.begin_upload(h,frame),"churn upload");pool.complete({frame,frame,frame,frame});pool.reference(h,{frame,frame+1,frame+2,frame+3});}
    if(round%2==0)pool.evict_oldest();
    ++frame;pool.complete({frame-1,frame-1,frame-1,frame-1});
    auto stats=pool.stats();check(stats.bytes<=512 && stats.resident+stats.pending+stats.retiring<=8,"bounded churn");
    for(auto entry:pool.page_table())check(!entry.resident || (entry.generation && entry.slot<8),"valid published table");
  }
}
}
int main() {
  try {retirement();feedback();timeline_handoff();selection();churn();std::printf("virtual_geometry_residency checks=%u passed=1 timeline_handoff=1\n",checks);return 0;}
  catch(const std::exception& error) {std::fprintf(stderr,"virtual_geometry_residency failed=%s checks=%u\n",error.what(),checks);return 1;}
}
