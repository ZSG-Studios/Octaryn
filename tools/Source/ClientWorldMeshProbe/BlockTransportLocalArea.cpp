#include "BlockTransportLocalAreaReference.h"
#include "BlockTransportPlayerBlocker.h"
#include "BlockTransportWork.h"
#include <fstream>

namespace mesh_probe {
namespace {
using namespace local_area;
struct LightNode {Pixel minimum,maximum;Words links;};
static_assert(sizeof(LightNode)==48);
class AreaProbe {
  WorldRenderer& r;
  BlockTransportWork selection;
  Slang::ComPtr<rhi::IBuffer> surfaces,links,counters,direct,environment,output,zero,lights,tree;
  Slang::ComPtr<rhi::IComputePipeline> inject,bounce;
  rhi::IAccelerationStructure* blocker;
  unsigned nodes;
  Slang::ComPtr<rhi::IBuffer> storage(const void* p,std::size_t bytes,unsigned stride) {
    return buffer(r,p,bytes,stride,rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  }
  void dispatch(rhi::ICommandEncoder* commands,rhi::IComputePipeline* pipeline,bool ray) {
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT joint area compute pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT joint area binding");
    require(bind_world_atlas(r.atlas,root),"BT joint area conditional atlas binding");
    if(ray)require(world_ray_bind(r,root),"BT joint area actual RT scene");
    selection.bind(root);const rhi::ShaderCursor cursor(root);
    const auto bind=[&](const char* name,rhi::IBuffer* value) {
      auto field=cursor[name];if(field.isValid())checked(field.setBinding(value),name);
    };
    const auto data=[&](const char* name,const void* value,std::size_t bytes) {
      auto field=cursor[name];if(field.isValid())checked(field.setData(value,bytes),name);
    };
    bind("btSurfaces",surfaces);bind("btLinks",links);bind("btCounters",counters);
    bind("btDirect",direct);bind("btEnvironment",environment);bind("btOutput",output);
    bind("btB0",zero);bind("btB1",zero);bind("btB2",zero);bind("localLights",lights);bind("giLightTree",tree);
    const Words frame{Geometry,Frame,Samples,16},work{0,2048,Radiance,0},solve{Radiance,Samples,0,16};
    const Pixel low{-64,-32,96,1},high{-32,0,128,0},sun{0,1,0,0},sky{},settings{1,64,.001f,0};
    data("btFrameInfo",frame.data(),sizeof(frame));data("btWork",work.data(),sizeof(work));data("btSolve",solve.data(),sizeof(solve));
    data("btCoverageMin",low.data(),sizeof(low));data("btCoverageMax",high.data(),sizeof(high));
    data("btSun",sun.data(),sizeof(sun));data("btSky",sky.data(),sizeof(sky));data("raySettings",settings.data(),sizeof(settings));
    data("giLightNodeCount",&nodes,sizeof(nodes));
    const unsigned player_enabled=blocker?1:0;data("playerShadowEnabled",&player_enabled,sizeof(player_enabled));
    if(blocker && cursor["playerShadowScene"].isValid())checked(cursor["playerShadowScene"].setBinding(blocker),"BT joint area real blocker");
    pass->dispatchCompute((ray?2048:Samples)/64,1,1);pass->end();commands->globalBarrier();
  }
public:
  AreaProbe(WorldRenderer& renderer,const Domain& domain,const std::vector<WorldLocalLight>& sources,
      rhi::IAccelerationStructure* occluder,unsigned reset):r(renderer),selection(r,Samples,2048),blocker(occluder),nodes(sources.size()==1?1u:3u) {
    require(sources.size()>=1 && sources.size()<=2,"BT joint area light fixture bound");
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Direct.slang","main",inject) &&
        block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Bounce.slang","main",bounce),
        "BT joint area retained production direct and bounce pipelines");
    std::vector<BlockTransportSurface> rows(Samples);
    for(unsigned i=0;i<Samples;++i)rows[i]={domain.key,{.5f,.3f,.7f,domain.metadata},
        {Geometry,i,Frame,reset?16u:0u},{1,0,0,reset==2?3u:1u}};
    surfaces=storage(rows.data(),rows.size()*sizeof(rows[0]),sizeof(rows[0]));
    std::vector<Words> empty_links(Samples*16);std::vector<Pixel> values(Samples);std::array<unsigned,12> stats{};
    links=storage(empty_links.data(),empty_links.size()*sizeof(Words),sizeof(Words));
    counters=storage(stats.data(),sizeof(stats),sizeof(unsigned));
    environment=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));
    output=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));zero=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));
    if(reset)for(auto& value:values)value={100,100,100,std::bit_cast<float>(reset==2?Radiance:Radiance-1)};
    direct=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));
    lights=storage(sources.data(),sources.size()*sizeof(sources[0]),sizeof(sources[0]));
    std::vector<LightNode> hierarchy(nodes);
    for(unsigned i=0;i<sources.size();++i) {
      const auto p=sources[i].position_range;
      hierarchy[nodes==1?0:i+1]={{p[0],p[1],p[2],1},{p[0],p[1],p[2],0},{0,0,i,1}};
    }
    if(nodes==3) {
      hierarchy[0].links={1,2,0,0};hierarchy[0].minimum[3]=2;
      for(unsigned c=0;c<3;++c) {
        hierarchy[0].minimum[c]=std::min(sources[0].position_range[c],sources[1].position_range[c]);
        hierarchy[0].maximum[c]=std::max(sources[0].position_range[c],sources[1].position_range[c]);
      }
    }
    tree=storage(hierarchy.data(),hierarchy.size()*sizeof(hierarchy[0]),sizeof(hierarchy[0]));
  }
  Reference run(bool dark) {
    for(unsigned batch=0;batch<Samples/2048;++batch) {
      const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
      require(r.frame_queue.wait(r.active_frame,2000),"BT joint area frame reuse");
      auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT joint area encoder");
      selection.encode(commands,surfaces,direct,environment,output,{Geometry,Frame,Samples,16},Radiance);
      dispatch(commands,inject,true);if(batch+1==Samples/2048)dispatch(commands,bounce,false);
      plant_probe::submit(r,commands);const auto header=selection.header();
      require(header[2]==2048 && header[3]==Samples,"BT joint area production selected-row budget");
      block_transport_complete(r,start);
    }
    std::vector<Pixel> incident(Samples),outgoing(Samples);std::vector<BlockTransportSurface> rows(Samples);
    checked(r.device->readBuffer(direct,0,incident.size()*sizeof(Pixel),incident.data()),"BT joint area linear direct readback");
    checked(r.device->readBuffer(output,0,outgoing.size()*sizeof(Pixel),outgoing.data()),"BT joint area outgoing readback");
    checked(r.device->readBuffer(surfaces,0,rows.size()*sizeof(rows[0]),rows.data()),"BT joint area reset state readback");
    Reference result;double second=0;const double rho[3]={.5,.3,.7};
    for(unsigned i=0;i<Samples;++i) {
      require(std::bit_cast<unsigned>(incident[i][3])==Radiance && std::bit_cast<unsigned>(outgoing[i][3])==Radiance &&
          rows[i].state[1]==i+1 && rows[i].state[3]==1 && (rows[i].extra[3]&2)==0,"BT joint area sample dropped or history reset delayed");
      for(unsigned c=0;c<3;++c) {
        const double value=incident[i][c];
        require(std::isfinite(value) && value>=0 && std::isfinite(outgoing[i][c]) &&
            std::abs(outgoing[i][c]-rho[c]*value)<3e-6*std::max(1.,value),"BT joint area invalid energy or material multiplied twice");
        if(dark)require(value==0 && outgoing[i][c]==0,"BT removed source left cached energy");
        result.mean[c]+=value/Samples;
      }
      second+=double(incident[i][0])*incident[i][0]/Samples;
    }
    result.red_variance=std::max(0.,second-result.mean[0]*result.mean[0]);
    std::array<unsigned,12> stats{};checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT joint area ray budget");
    require(stats[7]==(dark?0u:Samples),"BT joint area exceeded one local visibility call per selected receiver");
    return result;
  }
};
}
void block_transport_local_area_cases(Fixture& f) {
  auto& r=f.renderer;unsigned leaf=0,plant=0,stone=0;
  for(unsigned i=1;i<f.catalog.size();++i) {
    if(f.catalog[i].id=="octaryn.basegame.block.leaves")leaf=i;
    if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
    const unsigned layer=world_atlas_preview_layer(r.atlas,i);
    if(!plant && f.catalog[i].sprite && f.catalog[i].opaque && block_transport_plant_tag((layer<<4)|6))plant=i;
  }
  require(leaf && plant && stone,"BT joint area catalog receiver materials missing");
  require(prepare_world_atlas_plant_masks(r.atlas),"BT joint area conditional mask resources");
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> png(load_atlas_rgba("Atlases/basegame-color.png"),SDL_DestroySurface);
  require(bool(png),"BT joint area original PNG missing");
  open_world_renderer_set_center(&r,-2,3,0);auto source_column=column(-2,3,-32,32);
  const auto previous=r.sources.find({-2,3});source_column.revision=previous==r.sources.end()?1:previous->second.revision+1;
  put(source_column,20,0,20,std::uint16_t(stone));source_column.blocks.compact();
  require(open_world_renderer_update(&r,source_column),"BT joint area exact geometry publication");plant_probe::settle(r);
  BlockTransportPlayerBlocker blocker(r,-58.5f,-25.5f,101.5f);
  Domain cube(png.get()),masked(png.get(),world_atlas_preview_layer(r.atlas,leaf),true);
  unsigned cases=0;double worst=0,variance_ratio=0;
  std::ofstream observations("block-transport-local-area.csv");
  observations<<"case,rows,actual_red,actual_green,actual_blue,reference_red,reference_green,reference_blue,max_relative_error,red_variance\n";
  const auto run=[&](const char* name,const Domain& domain,bool mixed,bool partial,unsigned reset,bool dark) {
    const auto start=std::chrono::steady_clock::now();
    std::vector<WorldLocalLight> lights{domain.light(.13,.79,.8,{.9f,.4f,.15f,dark?0.f:20.f})};
    if(mixed)lights.push_back(domain.light(.85,.22,1.2,{.05f,.1f,.8f,20}));
    if(reset==1 && !dark)lights[0].color_intensity={0,0,1,20};
    const auto coarse=integrate(domain,lights,partial,partial?16:4),reference=integrate(domain,lights,partial,partial?32:8);
    require(relative(coarse.mean,reference.mean)<.002,"BT joint area independent area/slab quadrature did not converge");
    if(partial) {
      const auto clear=integrate(domain,lights,false,8);
      require(reference.mean[0]>clear.mean[0]*.05 && reference.mean[0]<clear.mean[0]*.95,
          "BT joint area visibility fixture must contain both blocked and visible energy");
    }
    block_transport_complete(r,start);
    AreaProbe probe(r,domain,lights,partial?blocker.scene.get():nullptr,reset);const auto actual=probe.run(dark);
    const double error=relative(actual.mean,reference.mean);worst=std::max(worst,error);++cases;
    std::printf("block_transport_local_area_case name=%s rows=%u partial=%u reset=%u relative_error=%.9g\n",name,Samples,partial?1u:0u,reset,error);
    observations<<name<<','<<Samples;
    for(double c:actual.mean)observations<<','<<c;for(double c:reference.mean)observations<<','<<c;
    observations<<','<<error<<','<<actual.red_variance<<'\n';observations.flush();
    require(bool(observations),"BT joint area linear observation write");
    require(error<(mixed || partial || domain.plant?.025:.004),"BT joint area mean differs from independent conditional area and ray integral");
    if(cases==1) {
      require(reference.red_variance>0,"BT joint area variance reference is degenerate");
      variance_ratio=actual.red_variance/reference.red_variance;
      require(variance_ratio<.3,"BT joint receiver candidate sampling did not reduce the one-origin variance");
    }
  };
  run("cube_open",cube,false,false,0,false);run("cube_mixed",cube,true,false,0,false);
  run("cube_partial",cube,true,true,0,false);run("leaf_open",masked,false,false,0,false);run("leaf_mixed",masked,true,false,0,false);
  const char* names[]={"plant_a_front","plant_a_back","plant_b_front","plant_b_back"};
  for(unsigned side=0;side<4;++side) {
    Domain crossed(png.get(),world_atlas_preview_layer(r.atlas,plant),true,true,side);run(names[side],crossed,true,false,0,false);
  }
  run("source_replaced",cube,false,false,1,false);run("visibility_reset",cube,true,true,2,false);run("source_removed",cube,false,false,1,true);
  require(r.debug.errors.load()==0,"BT joint area graphics validation errors");
  std::printf("block_transport_local_area=passed hardware=1 production_direct=1 production_bounce=1 production_selection=1 conditional_png=1 mixed_sources=1 exact_box_visibility=1 one_visibility_call=1 immediate_reset=1 cases=%u rows_per_case=%u worst_relative_error=%.9g one_origin_variance_ratio=%.9g validation_errors=0\n",
      cases,Samples,worst,variance_ratio);
}
}
