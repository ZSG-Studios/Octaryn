#include "SelectionGpu.h"
#include "InstanceSelection.h"
#include "GeometryBudget.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
GeometryAsset fixture() {
  GeometryAsset asset;asset.space=GeometrySpace::Object;asset.pages.resize(4);asset.groups.resize(3);
  asset.roots={1,2};asset.group_pages={0,1,2,3};
  asset.groups[0]={0,2,0,0,2,{{0,0,0},1,10}};
  asset.groups[1]={2,1,1,2,1,{{0,0,0},3,std::numeric_limits<float>::max()}};
  asset.groups[2]={3,1,1,3,1,{{0,0,0},3,std::numeric_limits<float>::max()}};
  asset.clusters.resize(4);
  for(unsigned i=0;i<4;++i) {
    auto& cluster=asset.clusters[i];cluster.page=i;cluster.group=i<2?0:i-1;
    cluster.bounds={{i%2?2.f:-2.f,0,0},.5f,i<2?0.f:10.f};
    if(i>=2) {cluster.refined_group=0;cluster.bounds.center[0]=0;cluster.bounds.radius=3;}
  }
  return asset;
}
GeometryTransform transform(float z,float scale=1) {
  std::array<float,16> matrix{-scale,0,0,0, .25f*scale,2*scale,0,0, 0,.125f*scale,.5f*scale,0, 0,0,z,1};
  GeometryTransform value;std::string error;
  require(geometry_transform(matrix,value,error),error.c_str());return value;
}
void submit(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* encoder,
    SelectionGpu& selection,std::span<const SelectionGpuFrame> frames) {
  auto commands=encoder->finish();require(bool(commands),"instance selection finish");
  auto fence=device->createFence({});require(bool(fence),"instance selection fence");
  rhi::ICommandBuffer* command=commands.get();rhi::IFence* signal=fence.get();std::uint64_t value=1;
  rhi::SubmitDesc desc{};desc.commandBuffers=&command;desc.commandBufferCount=1;
  desc.signalFences=&signal;desc.signalFenceValues=&value;desc.signalFenceCount=1;
  require(SLANG_SUCCEEDED(queue->submit(desc)),"instance selection submit");
  for(const auto& frame:frames)require(selection.submitted(frame,fence,value),selection.error().c_str());
  require(SLANG_SUCCEEDED(device->waitForFences(1,&signal,&value,true,30'000'000'000ull)),"instance selection fence timeout");
}
std::vector<unsigned> selected(rhi::IDevice* device,const SelectionGpuFrame& frame,unsigned count) {
  std::vector<std::array<unsigned,4>> records(count);std::vector<unsigned> ids;
  if(count)require(SLANG_SUCCEEDED(device->readBuffer(frame.selected,0,records.size()*16,records.data())),"instance cut readback");
  for(const auto record:records) {require(record[0]==record[1] && record[2]==record[1]+7 && record[3]==17,"instance page identity mismatch");ids.push_back(record[0]);}
  std::sort(ids.begin(),ids.end());return ids;
}
}
bool probe_instance_selection_gpu(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* shader) {
  try {
    static_assert(sizeof(InstanceSelectionView)==192 && geometry_instance_view_reservation(1)==384);
    const auto asset=fixture();SelectionTopology topology;std::string error;
    require(build_selection_topology(asset,topology,error),error.c_str());
    SelectionGpu selection;require(selection.initialize(device,topology,shader,1,2),selection.error().c_str());
    const auto initial_bytes=selection.gpu_bytes();
    std::vector<GpuPage> pages;for(unsigned i=0;i<4;++i)pages.push_back({i+7,17,1,0});
    SelectionView camera{{0,0,20},100,1};
    const auto near=instance_selection_view(camera,transform(0)),far=instance_selection_view(camera,transform(-20000));
    std::vector<InstanceSelectionView> views;unsigned cases{},fine{},coarse{},empty{};
    const auto compare=[&] {
      SelectionResult expected;const bool complete=select_geometry_instances(topology,pages,views,4,1,expected,error);
      auto commands=queue->createCommandEncoder();require(bool(commands),"instance selection encoder");
      SelectionGpuFrame frame;require(selection.record(commands,pages,views,frame),selection.error().c_str());
      submit(device,queue,commands,selection,std::span(&frame,1));
      SelectionFeedback feedback;require(selection.poll_feedback(feedback),selection.error().c_str());
      require(!feedback.selected_overflow && (feedback.missing_roots!=0)==!complete,"instance complete-cut parity");
      require((feedback.feedback_overflow!=0)==(expected.feedback_overflow!=0),"instance bounded feedback parity");
      std::array<unsigned,6> arguments{};
      require(SLANG_SUCCEEDED(device->readBuffer(frame.dispatch,0,sizeof(arguments),arguments.data())),"instance indirect readback");
      if(complete) {
        const auto observed=selected(device,frame,feedback.selected);std::sort(expected.clusters.begin(),expected.clusters.end());
        require(observed==expected.clusters,"affine GPU cut differs from CPU union");
        require(arguments[0]==(observed.size()+31)/32 && arguments[3]==observed.size(),"instance draw count mismatch");
        float bound{};
        for(const auto id:observed)for(const auto& view:views)bound=std::max(bound,instance_selection_error(view,asset.clusters[id].bounds));
        require(feedback.maximum_error_pixels>=bound && feedback.maximum_error_pixels<=std::max(.00001f,bound*1.0001f),
            "GPU achieved error is not the conservative affine bound");
        if(observed.empty())++empty;else if(observed.front()==0)++fine;else ++coarse;
      } else require(arguments[0]==0 && arguments[3]==0,"missing instance roots emitted geometry");
      require(feedback.requests.size()<=1,"instance feedback exceeded capacity");++cases;
    };
    views={far};compare();views={near};compare();views={far,near};compare();std::reverse(views.begin(),views.end());compare();
    views={instance_selection_view(camera,transform(-20000,100))};compare();
    auto hidden=near;hidden.flags[0]=1;hidden.planes[0][0]=1;hidden.planes[0][3]=-10000;
    views={hidden};compare();views={hidden,far};compare();
    views={near,far};pages[0].resident=0;compare();pages[1].resident=0;compare();
    pages[2].resident=0;compare();for(auto& page:pages)page.resident=1;
    pages[0].generation=0;compare();pages[0].generation=17;
    camera.eye[2]=1e7f;views={instance_selection_view(camera,transform(-100))};compare();
    camera.eye[2]=1e30f;views={instance_selection_view(camera,transform(-100))};compare();
    camera.error_pixels=0;views={instance_selection_view(camera,transform(-100))};compare();
    // Two pending frames keep independent view uploads even when capacities grow.
    auto commands=queue->createCommandEncoder();std::array<SelectionGpuFrame,2> frames;
    views={far};require(selection.record(commands,pages,views,frames[0]),selection.error().c_str());
    views={near,far,far};require(selection.record(commands,pages,views,frames[1]),selection.error().c_str());
    SelectionGpuFrame refused;require(!selection.record(commands,pages,views,refused),"instance ring overwrote retained views");
    require(selection.gpu_bytes()>=initial_bytes+2*sizeof(InstanceSelectionView),"instance view bytes omitted from accounting");
    submit(device,queue,commands,selection,frames);SelectionFeedback feedback;
    require(selection.poll_feedback(feedback),selection.error().c_str());
    require(selected(device,frames[0],feedback.selected)==std::vector<unsigned>({2,3}),"pending far views overwritten");
    require(selection.poll_feedback(feedback),selection.error().c_str());
    require(selected(device,frames[1],feedback.selected)==std::vector<unsigned>({0,1}),"pending near views overwritten");
    require(fine && coarse && empty,"instance cases did not exercise both LOD cuts and culling");
    std::printf("geometry_instance_selection_gpu passed=1 parity_cases=%u fine=%u coarse=%u culled=%u mirrored=1 nonuniform=1 shear=1 complete_groups=1 feedback_bounds=1 view_fences=1 view_bytes=384 achieved_error=1\n",cases,fine,coarse,empty);
    return true;
  }catch(const std::exception& failure) {std::fprintf(stderr,"geometry_instance_selection_gpu failed=%s\n",failure.what());return false;}
}
