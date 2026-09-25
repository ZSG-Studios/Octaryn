#include "BlockTransportSetup.h"
#include "BlockTransportGeometry.h"
#include "BlockTransportDynamicOwner.h"
#include "BlockTransportItemLighting.h"
#include "BlockTransportPlayerBlocker.h"
#include "LocalLight.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/DynamicReceivers.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
constexpr unsigned Capacity=512,Count=4;
using Receiver=DynamicReceiver;
static_assert(sizeof(Receiver)==48);
struct FaceRow {unsigned slot,room;BlockSurfaceKey key;};
struct LightNode {Pixel minimum,maximum;Words links;};
void publish(Fixture& f,bool roof) {
  auto& r=f.renderer;unsigned stone=0;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone>0,"BT dynamic fixture stone material");open_world_renderer_set_center(&r,0,0,0);
  auto source=column();const auto previous=r.sources.find({0,0});
  source.revision=previous==r.sources.end()?1:previous->second.revision+1;
  for(int offset:{0,8})for(int z=8;z<=13;++z)for(int y=0;y<=5;++y)for(int x=8;x<=13;++x)
    if(x==8 || x==13 || y==0 || (roof && y==5) || z==8 || z==13)
      put(source,x+offset,y,z,std::uint16_t(stone));
  source.blocks.compact();require(open_world_renderer_update(&r,source),"BT dynamic world publication");
  for(unsigned attempt=0;attempt<64;++attempt) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT dynamic RT reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT dynamic RT encoder");
    require(world_ray_prepare(r,commands,r.active_frame),"BT dynamic exact world preparation");
    auto command=commands->finish();require(bool(command),"BT dynamic RT finish");
    require(r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BT dynamic bounded RT completion");
    const bool ready=world_ray_coverage_complete(r);block_transport_complete(r,start);if(ready)return;
  }
  require(false,"BT dynamic geometry exceeded bounded publication attempts");
}
class DynamicProbe {
  WorldRenderer& r;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::IBuffer> input,previous,values,surfaces,direct_values,environment_values,indirect_values;
  Slang::ComPtr<rhi::IBuffer> receiver_direct,receiver_state,counters,lights,tree;
  std::array<BlockTransportSurface,Capacity> cache{};
  std::array<Pixel,Capacity> direct{},environment{},indirect{};
  std::vector<FaceRow> face_rows;
  bool poison=false,poison_nonfinite=false,clear_history=false;
  Slang::ComPtr<rhi::IBuffer> storage(const void* bytes,std::size_t size,unsigned stride,bool writable=false) {
    auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
    if(writable)usage|=rhi::BufferUsage::UnorderedAccess;
    return buffer(r,bytes,size,stride,usage);
  }
public:
  unsigned geometry=431,radiance=433,cases=0,checks=0,player_revision=0,samples=16;
  bool enabled=true,coverage=true;
  bool local_light=false,player_enabled=false;
  rhi::IAccelerationStructure* player_scene=nullptr;
  Pixel sky{},sun{0,1,0,1};
  float shadow_range=64;
  std::array<Receiver,Count> receivers{};
  std::array<Pixel,Count> measured_direct{};
  std::array<unsigned,4> measured_counters{};
  explicit DynamicProbe(WorldRenderer& renderer):r(renderer) {
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/DynamicReceivers.slang",
        "main",pipeline),"BT dynamic retained production pipeline");
    for(unsigned room=0;room<2;++room)for(unsigned face=0;face<6;++face)
      for(int v=0;v<4;++v)for(int u=0;u<4;++u) {
        int p[3]={9+int(room)*8,1,9};const unsigned axis=face/2;
        p[axis]=face%2?p[axis]-1:p[axis]+4;p[axis==0?2:0]+=u;p[axis==1?2:1]+=v;
        const BlockSurfaceKey key{p[0],p[1],p[2],face};unsigned slot=Capacity;
        for(unsigned attempt=0;attempt<BlockTransportProbes;++attempt) {
          const unsigned candidate=(block_surface_hash(key)+attempt)&(Capacity-1);
          if(cache[candidate].state[0]==0){slot=candidate;break;}
        }
        require(slot<Capacity,"BT dynamic independent closed-room cache overflow");
        cache[slot]={key,{.5f,.5f,.5f,1},{geometry,1,1,1},{7,0,0,1}};
        face_rows.push_back({slot,room,key});
      }
    for(unsigned i=0;i<Count;++i)receivers[i]={{i==2?19.f:11.f,3,11,0},{0,i==1?-1.f:1.f,0,0},{100+i,17,geometry,radiance}};
    std::array<Receiver,Count> blank{};std::array<Pixel,Count> empty{};
    input=storage(receivers.data(),sizeof(receivers),sizeof(Receiver));
    previous=storage(blank.data(),sizeof(blank),sizeof(Receiver),true);
    values=storage(empty.data(),sizeof(empty),sizeof(Pixel),true);
    receiver_direct=storage(empty.data(),sizeof(empty),sizeof(Pixel),true);
    std::array<std::array<unsigned,2>,Count> empty_state{};
    receiver_state=storage(empty_state.data(),sizeof(empty_state),sizeof(empty_state[0]),true);
    counters=storage(measured_counters.data(),sizeof(measured_counters),sizeof(unsigned),true);
    surfaces=storage(cache.data(),sizeof(cache),sizeof(cache[0]));
    direct_values=storage(direct.data(),sizeof(direct),sizeof(Pixel));
    environment_values=storage(environment.data(),sizeof(environment),sizeof(Pixel));
    indirect_values=storage(indirect.data(),sizeof(indirect),sizeof(Pixel));
    WorldLocalLight light;light.position_range={11,4,11,24};light.color_intensity={.9f,.4f,.15f,20};light.axis_v_type[3]=0;
    const LightNode node{{11,4,11,1},{11,4,11,0},{0,0,0,1}};
    lights=storage(&light,sizeof(light),sizeof(light));tree=storage(&node,sizeof(node),sizeof(node));
  }
  void sources(unsigned mode) {
    for(const auto& face:face_rows) {
      Pixel d{},e{},g{};
      if(mode==0){d={2,0,0,0};e={0,4,0,0};g={0,0,6,0};}
      if(mode==1)d=face.room?Pixel{0,0,4,0}:Pixel{2,0,0,0};
      if(mode==2) {
        if(face.key.direction==2)d={2,0,0,0};
        if(face.key.direction==3)d={0,0,4,0};
      }
      d[3]=e[3]=g[3]=std::bit_cast<float>(radiance);
      direct[face.slot]=d;environment[face.slot]=e;indirect[face.slot]=g;
    }
  }
  void relight() {++radiance;for(auto& receiver:receivers)receiver.identity[3]=radiance;}
  void reset_geometry(bool populate) {
    ++geometry;for(auto& receiver:receivers)receiver.identity[2]=geometry;
    for(auto& surface:cache)if(surface.state[0])surface.state[0]=populate?geometry:geometry-1;
  }
  void poison_values(bool nonfinite=false) {poison=true;poison_nonfinite=nonfinite;}
  void reset_history() {clear_history=true;}
  void player(bool enabled) {if(player_enabled!=enabled)++player_revision;player_enabled=enabled;}
  std::array<Pixel,Count> run(const char* name) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT dynamic receiver reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT dynamic receiver encoder");
    const auto upload=[&](rhi::IBuffer* target,const void* bytes,std::size_t size) {
      checked(commands->uploadBufferData(target,0,size,bytes),"BT dynamic fixture inputs");
      commands->setBufferState(target,rhi::ResourceState::ShaderResource);
    };
    upload(input,receivers.data(),sizeof(receivers));upload(surfaces,cache.data(),sizeof(cache));
    upload(direct_values,direct.data(),sizeof(direct));upload(environment_values,environment.data(),sizeof(environment));
    upload(indirect_values,indirect.data(),sizeof(indirect));
    const std::array<unsigned,4> empty_counters{};upload(counters,empty_counters.data(),sizeof(empty_counters));
    if(clear_history) {
      const std::array<Receiver,Count> blank{};const std::array<Pixel,Count> pixels{};
      const std::array<std::array<unsigned,2>,Count> states{};
      upload(previous,blank.data(),sizeof(blank));upload(values,pixels.data(),sizeof(pixels));
      upload(receiver_state,states.data(),sizeof(states));clear_history=false;
    }
    if(poison) {
      std::array<Pixel,Count> invalid;for(auto& pixel:invalid)
        pixel={poison_nonfinite?std::numeric_limits<float>::quiet_NaN():1000.f,1000,1000,16};
      upload(values,invalid.data(),sizeof(invalid));poison=false;
    }
    commands->setBufferState(previous,rhi::ResourceState::UnorderedAccess);
    commands->setBufferState(values,rhi::ResourceState::UnorderedAccess);
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT dynamic receiver pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT dynamic receiver binding");
    require(world_ray_bind(r,root) && bind_world_atlas(r.atlas,root),"BT dynamic exact scene and atlas");
    const rhi::ShaderCursor cursor(root);
    const auto bind=[&](const char* name,rhi::IBuffer* target) {checked(cursor[name].setBinding(target),name);};
    const auto data=[&](const char* name,const void* bytes,std::size_t size) {
      auto field=cursor[name];if(field.isValid())checked(field.setData(bytes,size),name);
    };
    bind("btReceivers",input);bind("btPreviousReceivers",previous);bind("btReceiverValues",values);
    bind("btReceiverDirect",receiver_direct);bind("btReceiverCounters",counters);
    bind("btReceiverState",receiver_state);
    bind("localLights",lights);bind("giLightTree",tree);
    const unsigned nodes=local_light?1:0;data("giLightNodeCount",&nodes,sizeof(nodes));
    bind("btLookupSurfaces",surfaces);bind("btLookupDirect",direct_values);
    bind("btLookupEnvironment",environment_values);bind("btLookupIndirect",indirect_values);
    const Words lookup{enabled?1u:0u,geometry,radiance,Capacity},work{Count,samples,player_revision,0};
    const Pixel low{0,0,0,coverage?1.f:0.f},high{32,32,32,0},settings{1,64,.001f,0};
    data("btLookupInfo",lookup.data(),sizeof(lookup));data("btReceiverWork",work.data(),sizeof(work));
    data("btCoverageMin",low.data(),sizeof(low));data("btCoverageMax",high.data(),sizeof(high));
    data("btSun",sun.data(),sizeof(sun));data("btSky",sky.data(),sizeof(sky));data("raySettings",settings.data(),sizeof(settings));
    data("btReceiverShadowRange",&shadow_range,sizeof(shadow_range));
    if(player_scene) {
      checked(cursor["playerShadowScene"].setBinding(rhi::Binding(player_scene)),"BT dynamic player triangle scene");
      const unsigned active=player_enabled?1:0;data("playerShadowEnabled",&active,sizeof(active));
    }
    for(const char* forbidden:{"btSurfaceKeys","positions","voxels","colors","btDimensions","projection","eye"})
      require(!cursor[forbidden].isValid(),"BT dynamic receiver transport depends on screen input");
    pass->dispatchCompute(1,1,1);pass->end();auto command=commands->finish();require(bool(command),"BT dynamic receiver finish");
    require(r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BT dynamic receiver bounded completion");
    std::array<Pixel,Count> actual{};
    checked(r.device->readBuffer(values,0,sizeof(actual),actual.data()),"BT dynamic incident readback");
    checked(r.device->readBuffer(receiver_direct,0,sizeof(measured_direct),measured_direct.data()),"BT dynamic direct readback");
    checked(r.device->readBuffer(counters,0,sizeof(measured_counters),measured_counters.data()),"BT dynamic ray budget readback");
    require(measured_counters[0]<=Count*samples && measured_counters[1]<=Count && measured_counters[2]<=Count &&
        measured_counters[3]<=Count*18,"BT dynamic receiver exceeded transport/direct/player ray bounds");
    for(const auto& pixel:actual)for(unsigned c=0;c<3;++c) {
      require(std::isfinite(pixel[c]) && pixel[c]>=0,"BT dynamic invalid incident radiance");++checks;
    }
    ++cases;block_transport_complete(r,start);std::printf("block_transport_dynamic_case=%s status=passed\n",name);
    return actual;
  }
  void equal(const Pixel& actual,Pixel expected) {
    for(unsigned c=0;c<3;++c){require(std::abs(actual[c]-expected[c])<2e-5f,"BT dynamic receiver energy or stale history leak");++checks;}
  }
};
}
void block_transport_dynamic_cases(Fixture& f) {
  block_transport_geometry_cases(f);
  block_transport_item_lighting_cases(f);
  for(unsigned count=1;count<=DynamicReceiverCapacity;++count)
    require(count*std::min(16u,DynamicReceiverRayBudget/count)<=DynamicReceiverRayBudget,
        "BT dynamic owner dispatch exceeded fixed ray budget");
  publish(f,true);BlockTransportPlayerBlocker blocker(f.renderer),moved(f.renderer,.02f,0,.03f),
      distant(f.renderer,0,0,12);DynamicProbe probe(f.renderer);probe.sources(0);
  probe.player_scene=blocker.scene;
  for(const auto& value:probe.run("constant_incident"))probe.equal(value,{1,2,3,0});
  for(const auto& direct:probe.measured_direct) {
    probe.equal(direct,{});require(direct[3]==0,"BT sealed receiver admitted direct sunlight");
  }
  const auto width=f.renderer.width,height=f.renderer.height;
  f.renderer.width=17;f.renderer.height=19;
  world_renderer_prepare_draw(f.renderer,WorldCamera{30,20,30,3.14159265f,0,1});
  for(const auto& value:probe.run("view_independent")) {
    probe.equal(value,{1,2,3,0});require(value[3]==2,"BT unchanged world receiver did not retain its second actual batch");
  }
  f.renderer.width=width;f.renderer.height=height;
  probe.relight();probe.sources(1);auto value=probe.run("separate_rooms");
  probe.equal(value[0],{1,0,0,0});probe.equal(value[2],{0,0,2,0});
  probe.receivers[0].position[0]=19;value=probe.run("moved_across_wall");probe.equal(value[0],{0,0,2,0});
  require(value[0][3]==1,"BT moved receiver retained incompatible room history");
  probe.relight();probe.sources(2);value=probe.run("oriented_sides");
  require(value[0][0]>0 && value[0][2]==0 && value[1][2]>0 && value[1][0]==0,"BT dynamic opposite hemispheres share incident lighting");
  probe.relight();probe.sources(3);for(const auto& pixel:probe.run("source_removed"))probe.equal(pixel,{});
  probe.relight();probe.sources(0);probe.poison_values();for(auto& receiver:probe.receivers)++receiver.identity[0];
  for(const auto& pixel:probe.run("identity_reused")) {
    probe.equal(pixel,{1,2,3,0});require(pixel[3]==1,"BT recycled dynamic identity reused poisoned history");
  }
  --probe.receivers[0].identity[3];value=probe.run("receiver_epoch_mismatch");probe.equal(value[0],{});
  require(value[0][3]==0,"BT source epoch mismatch was accepted as a valid receiver sample");
  ++probe.receivers[0].identity[3];probe.poison_values(true);
  for(const auto& pixel:probe.run("nonfinite_history")) {
    probe.equal(pixel,{1,2,3,0});require(pixel[3]==1,"BT nonfinite history did not force a fresh receiver estimate");
  }
  probe.poison_values();for(auto& receiver:probe.receivers)receiver.normal[3]=std::bit_cast<float>(7u);
  for(const auto& pixel:probe.run("local_receiver_reused")) {
    probe.equal(pixel,{1,2,3,0});require(pixel[3]==1,"BT changed local receiver identity reused another vertex history");
  }
  probe.receivers[0].position={11,3,11,0};probe.local_light=true;probe.relight();probe.sources(3);
  for(const auto& pixel:probe.run("local_direct_separate"))probe.equal(pixel,{});
  const double attenuation=std::pow(1-1./std::pow(24.,4),2)*20/3.141592653589793;
  probe.equal(probe.measured_direct[0],{float(.9*attenuation),float(.4*attenuation),float(.15*attenuation),0});
  probe.equal(probe.measured_direct[1],{});probe.equal(probe.measured_direct[2],{});
  probe.relight();probe.sources(0);
  for(const auto& pixel:probe.run("player_clear_baseline"))probe.equal(pixel,{1,2,3,0});
  probe.player(true);value=probe.run("player_triangle_blocker");
  for(unsigned i:{0u,1u,3u}) {
    probe.equal(value[i],{});probe.equal(probe.measured_direct[i],{});
    require(value[i][3]==1,"BT newly blocked receiver retained old incident history");
  }
  probe.equal(value[2],{1,2,3,0});require(probe.measured_counters[3]>0,"BT dynamic skipped player triangle traversal");
  const float clear_count=value[2][3];
  probe.player_scene=moved.scene;++probe.player_revision;value=probe.run("unrelated_player_motion");
  probe.equal(value[2],{1,2,3,0});
  require(value[2][3]==std::min(clear_count+1,16.f),"BT unrelated moving player discarded clear receiver history");
  require(value[0][3]==1,"BT moved current blocker retained affected receiver history");
  probe.player_scene=distant.scene;++probe.player_revision;value=probe.run("player_moves_clear");
  probe.equal(value[0],{1,2,3,0});require(value[0][3]==1,"BT moved former blocker retained stale dark history");
  probe.player_scene=blocker.scene;++probe.player_revision;value=probe.run("player_moves_blocked");
  probe.equal(value[0],{});require(value[0][3]==1,"BT moved new blocker retained stale lit history");
  probe.player(false);value=probe.run("player_removed");
  for(const auto& pixel:value)probe.equal(pixel,{1,2,3,0});
  require(value[0][3]==1,"BT player removal retained occluded receiver history");
  probe.equal(probe.measured_direct[0],{float(.9*attenuation),float(.4*attenuation),float(.15*attenuation),0});
  probe.local_light=false;
  probe.player(true);probe.receivers[0].position={11.5f,3,11,0};probe.receivers[0].normal={-1,0,0,std::bit_cast<float>(7u)};
  probe.reset_history();const auto reference=probe.run("four_sample_reference");
  require(reference[0][0]>0 && reference[0][0]<1,"BT player partial occluder did not produce mixed visibility");
  probe.samples=4;probe.reset_history();
  for(unsigned batch=1;batch<=4;++batch) {
    const auto name=std::string("four_sample_batch_")+std::to_string(batch);value=probe.run(name.c_str());
    require(value[0][3]==float(batch),"BT stationary partial player discarded valid world-space history");
  }
  for(unsigned i=0;i<Count;++i)probe.equal(value[i],reference[i]);
  probe.samples=16;probe.player(false);
  probe.reset_geometry(false);for(const auto& pixel:probe.run("geometry_reset"))probe.equal(pixel,{});
  probe.reset_geometry(true);probe.receivers[0].normal={0,0,0,0};
  probe.receivers[1].normal[0]=std::numeric_limits<float>::quiet_NaN();probe.receivers[2].position[0]=100;
  value=probe.run("invalid_receivers");for(unsigned i=0;i<3;++i)probe.equal(value[i],{});
  probe.equal(value[3],{1,2,3,0});
  probe.coverage=false;for(const auto& pixel:probe.run("unknown_coverage"))probe.equal(pixel,{});
  probe.coverage=true;probe.receivers[0].normal={0,1,0,0};probe.receivers[0].position={11,3,11,0};
  probe.reset_geometry(false);probe.sky={1,1,0,0};
  for(const auto& pixel:probe.run("closed_sky"))probe.equal(pixel,{});
  publish(f,false);probe.reset_geometry(false);value=probe.run("opened_sky");
  require(value[0][0]>0 && value[0][1]>0 && value[0][2]>0,"BT dynamic roof opening did not expose certified sky");
  require(probe.measured_direct[0][3]==1,"BT open receiver lost unoccluded direct sunlight");
  probe.sky={};probe.sun={std::sqrt(.99f),.1f,0,1};
  probe.receivers[0].position={11,6,11,0};probe.receivers[0].normal={1,0,0,0};
  value=probe.run("sun_side_exit_open");probe.equal(value[0],{});
  require(probe.measured_direct[0][3]==1,"BT finite-scene direct sun incorrectly requires top sky certification");
  probe.receivers[0].position={11,3,11,0};value=probe.run("sun_side_exit_blocked");
  require(probe.measured_direct[0][3]==0,"BT finite-scene direct sun crossed the sealed side wall");
  probe.shadow_range=1;value=probe.run("sun_distance_fade");
  require(probe.measured_direct[0][3]==1,"BT direct sun ignored primary configured distance fade");
  probe.shadow_range=64;probe.sun={0,1,0,1};probe.receivers[0].normal={0,1,0,0};
  probe.receivers[0].position={100,3,11,0};value=probe.run("sun_outside_gi_coverage");probe.equal(value[0],{});
  require(value[0][3]==0 && probe.measured_direct[0][3]==1,"BT absent GI coverage suppressed independently traced direct sun");
  probe.enabled=false;for(const auto& pixel:probe.run("disabled_cache"))probe.equal(pixel,{});
  block_transport_dynamic_owner_case(f);
  require(f.renderer.debug.errors.load()==0,"BT dynamic graphics validation errors");
  require(probe.cases==32,"BT dynamic receiver fixture case count");
  std::printf("block_transport_dynamic=passed hardware=1 production_gather=1 exact_cache=1 source_once=1 true_sides=1 moving_receivers=1 wall_isolation=1 source_removal=1 identity_reuse=1 local_receiver_identity=1 geometry_epoch=1 receiver_epoch=1 finite_history=1 conservative_sky=1 invalid_inputs=1 view_independent=1 no_screen_resources=1 direct_sun_visibility=1 local_direct_separate=1 player_triangle_occlusion=1 player_removal=1 player_disocclusion=1 stationary_partial_player=1 four_sample_stream=1 unaffected_actor_history=1 moved_actor_reset=1 finite_scene_sun=1 independent_direct_coverage=1 sun_range_fade=1 ray_budget=32768 rows=192 cases=%u scalar_checks=%u validation_errors=0\n",probe.cases,probe.checks);
}
}
