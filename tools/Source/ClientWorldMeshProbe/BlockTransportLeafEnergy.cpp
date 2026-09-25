#include "BlockTransportLeafProbe.h"
#include "LocalLight.h"
namespace mesh_probe::leaf_probe {
namespace {
struct LightNode {Pixel minimum,maximum;Words links;};
class Energy {
  WorldRenderer& r;
  BlockTransportWork selection;
  Slang::ComPtr<rhi::IBuffer> surfaces,links,counters,direct,environment,output,zero,lights,tree;
  Slang::ComPtr<rhi::IComputePipeline> inject,bounce;
  Slang::ComPtr<rhi::IBuffer> storage(const void* p,std::size_t bytes,unsigned stride) {
    return buffer(r,p,bytes,stride,rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  }
  void dispatch(rhi::ICommandEncoder* commands,rhi::IComputePipeline* pipeline,bool ray) {
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT leaf energy compute pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT leaf energy binding");
    require(bind_world_atlas(r.atlas,root),"BT leaf conditional atlas binding");
    if(ray)require(world_ray_bind(r,root),"BT leaf actual RT scene");
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
    const Pixel low{-64,-32,96,1},high{-32,0,128,0},sun{0,1,0,0},sky{},settings{1,8,.001f,0};
    const unsigned nodes=1;
    data("btFrameInfo",frame.data(),sizeof(frame));data("btWork",work.data(),sizeof(work));data("btSolve",solve.data(),sizeof(solve));
    data("btCoverageMin",low.data(),sizeof(low));data("btCoverageMax",high.data(),sizeof(high));
    data("btSun",sun.data(),sizeof(sun));data("btSky",sky.data(),sizeof(sky));data("raySettings",settings.data(),sizeof(settings));
    data("giLightNodeCount",&nodes,sizeof(nodes));
    pass->dispatchCompute((ray?2048:Samples)/64,1,1);pass->end();commands->globalBarrier();
  }
public:
  Energy(WorldRenderer& renderer,unsigned face,Pixel rho):r(renderer),selection(r,Samples,2048) {
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Direct.slang","main",inject) &&
        block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Bounce.slang","main",bounce),
        "BT leaf retained production direct and bounce pipelines");
    std::vector<BlockTransportSurface> rows(Samples);
    // Independent first-sample rows integrate the production area estimator without history filtering.
    for(unsigned i=0;i<Samples;++i)rows[i]={{Anchor[0],Anchor[1],Anchor[2],face},rho,{Geometry,i,Frame,0},{1,0,0,1}};
    surfaces=storage(rows.data(),rows.size()*sizeof(rows[0]),sizeof(rows[0]));
    std::vector<Words> empty_links(Samples*16);std::vector<Pixel> values(Samples);std::array<unsigned,12> stats{};
    links=storage(empty_links.data(),empty_links.size()*sizeof(Words),sizeof(Words));
    counters=storage(stats.data(),sizeof(stats),sizeof(unsigned));
    direct=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));
    environment=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));
    output=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));zero=storage(values.data(),values.size()*sizeof(Pixel),sizeof(Pixel));
    auto position=point(face,.13,.79);const auto n=normal(face);
    for(unsigned c=0;c<3;++c)position[c]+=n[c]*.8f;
    WorldLocalLight light;light.position_range={position[0],position[1],position[2],24};
    light.color_intensity={.9f,.4f,.15f,20};light.axis_v_type[3]=0;
    const LightNode node{{position[0],position[1],position[2],1},{position[0],position[1],position[2],0},{0,0,0,1}};
    lights=storage(&light,sizeof(light),sizeof(light));tree=storage(&node,sizeof(node),sizeof(node));
  }
  std::pair<Color,Color> run() {
    for(unsigned batch=0;batch<2;++batch) {
      const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
      require(r.frame_queue.wait(r.active_frame,2000),"BT leaf energy frame reuse");
      auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT leaf energy encoder");
      selection.encode(commands,surfaces,direct,environment,output,{Geometry,Frame,Samples,16},Radiance);
      dispatch(commands,inject,true);if(batch==1)dispatch(commands,bounce,false);
      plant_probe::submit(r,commands);const auto header=selection.header();
      require(header[2]==2048 && header[3]==Samples,"BT leaf energy production row budget");
      block_transport_complete(r,start);
    }
    std::vector<Pixel> incident(Samples),outgoing(Samples);
    checked(r.device->readBuffer(direct,0,incident.size()*sizeof(Pixel),incident.data()),"BT leaf linear direct readback");
    checked(r.device->readBuffer(output,0,outgoing.size()*sizeof(Pixel),outgoing.data()),"BT leaf outgoing radiance readback");
    Color direct_mean{},bounce_mean{};
    for(unsigned i=0;i<Samples;++i) {
      require(std::bit_cast<unsigned>(incident[i][3])==Radiance && std::bit_cast<unsigned>(outgoing[i][3])==Radiance,
          "BT leaf energy dropped selected sample");
      for(unsigned c=0;c<3;++c) {
        require(std::isfinite(incident[i][c]) && std::isfinite(outgoing[i][c]) && incident[i][c]>=0 && outgoing[i][c]>=0,
            "BT leaf nonfinite or negative energy");
        direct_mean[c]+=incident[i][c]/Samples;bounce_mean[c]+=outgoing[i][c]/Samples;
      }
    }
    std::array<unsigned,12> stats{};checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT leaf query count");
    require(stats[7]==Samples,"BT leaf production direct must execute one visible local query per sample");
    return {direct_mean,bounce_mean};
  }
};
}
void energy(Fixture& f,const Oracle& oracle,const std::array<Pixel,6>& reflectance) {
  const double reference=oracle.irradiance(8);const Color source{18,8,3};double worst_direct=0,worst_bounce=0;
  for(unsigned face=0;face<6;++face) {
    Energy probe(f.renderer,face,reflectance[face]);const auto actual=probe.run();
    for(unsigned c=0;c<3;++c) {
      const double direct=reference*source[c],outgoing=direct*oracle.rho[c];
      require(direct>0 && outgoing>0,"BT leaf energy oracle must illuminate each color channel");
      worst_direct=std::max(worst_direct,std::abs(actual.first[c]-direct)/direct);
      worst_bounce=std::max(worst_bounce,std::abs(actual.second[c]-outgoing)/outgoing);
    }
  }
  require(worst_direct<.004 && worst_bounce<.004,"BT leaf production lighting differs from independent conditional PNG area integral");
  std::printf("block_transport_leaf_energy=passed hardware=1 production_direct=1 production_bounce=1 production_selection=1 source_png=1 conditional_area_integral=1 no_double_count=1 faces=6 samples_per_face=%u direct_relative_error=%.9g outgoing_relative_error=%.9g validation_errors=0\n",
      Samples,worst_direct,worst_bounce);
}
}
