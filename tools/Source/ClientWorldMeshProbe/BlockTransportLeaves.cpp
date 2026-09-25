#include "BlockTransportLeafProbe.h"
namespace mesh_probe {
namespace {
using namespace leaf_probe;
std::array<Pixel,6> origins(Fixture& f,unsigned material,unsigned layer,const Oracle& oracle) {
  auto& r=f.renderer;std::vector<Query> queries;
  for(unsigned face=0;face<6;++face)for(unsigned i=0;i<Samples;++i)
    queries.push_back({{std::bit_cast<unsigned>(Anchor[0]),std::bit_cast<unsigned>(Anchor[1]),
        std::bit_cast<unsigned>(Anchor[2]),material|(face<<16)},{i%16,i/16,i,0}});
  const auto input=buffer(r,queries.data(),queries.size()*sizeof(Query),sizeof(Query),rhi::BufferUsage::ShaderResource);
  std::vector<Result> actual(queries.size());
  const auto output=buffer(r,actual.data(),actual.size()*sizeof(Result),sizeof(Result),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportLeaves.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BT leaf prepared production-helper pipeline");
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT leaf frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT leaf origin encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT leaf origin pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT leaf origin pipeline bind");
  require(bind_world_atlas(r.atlas,root) && world_ray_bind(r,root),"BT leaf exact geometry and mask bindings");
  const rhi::ShaderCursor cursor(root);const unsigned count=unsigned(queries.size());const Pixel settings{1,8,.001f,0};
  checked(cursor["leafQueries"].setBinding(input),"BT leaf origin input");
  checked(cursor["leafResults"].setBinding(output),"BT leaf origin output");
  checked(cursor["leafCount"].setData(&count,sizeof(count)),"BT leaf origin count");
  checked(cursor["raySettings"].setData(settings.data(),sizeof(settings)),"BT leaf source alpha sampling");
  pass->dispatchCompute((count+63)/64,1,1);pass->end();plant_probe::submit(r,commands);
  checked(r.device->readBuffer(output,0,actual.size()*sizeof(Result),actual.data()),"BT leaf origin readback");
  std::array<Pixel,6> reflectance{};double worst_moment=0;
  for(unsigned face=0;face<6;++face) {
    const auto n=normal(face);double mean_cosine=0;std::array<unsigned,1024> transport_bins{},direct_bins{};
    for(unsigned i=0;i<Samples;++i) {
      const auto& a=actual[face*Samples+i];
      require(a.key==BlockSurfaceKey{Anchor[0],Anchor[1],Anchor[2],face} && a.reflectance[3]==-float(layer+1),
          "BT leaf exact signed key or conditional material metadata");
      require(a.proof[0]==1 && a.proof[1]==1 && a.proof[2]==1 && std::bit_cast<float>(a.proof[3])>=.35f,
          "BT leaf sampled transport origin is not its actual alpha-tested RT owner");
      require(a.origin[3]==1 && a.position[3]==1,"BT leaf mask unexpectedly rejected finite sample");
      double length=0,cosine=0;Pixel on_plane=a.origin;
      for(unsigned c=0;c<3;++c) {
        require(std::isfinite(a.origin[c]) && std::isfinite(a.direction[c]) && std::isfinite(a.position[c]),
            "BT leaf sampled nonfinite geometry");
        require(std::abs(a.reflectance[c]-oracle.rho[c])<2e-6,"BT leaf conditional reflectance differs from source PNG");
        length+=a.direction[c]*a.direction[c];cosine+=a.direction[c]*n[c];
      }
      const auto plane=point(face,.5,.5);const unsigned axis=face/2;
      const float outward=std::nextafter(plane[axis],face%2?INFINITY:-INFINITY);
      require(a.origin[axis]==outward && a.position[axis]==plane[axis],
          "BT leaf sample violates exact outward ULP and owner plane");
      for(unsigned c=0;c<3;++c)if(c!=axis)
        require(a.origin[c]>Anchor[c] && a.origin[c]<Anchor[c]+1 &&
            a.position[c]>Anchor[c] && a.position[c]<Anchor[c]+1,"BT leaf origin escaped unit cell");
      on_plane[axis]=plane[axis];
      require(oracle.owns(on_plane,face) && oracle.owns(a.position,face),"BT leaf sample entered an original PNG hole");
      const auto t=uv(on_plane,face),d=uv(a.position,face);
      ++transport_bins[unsigned(t[1]*32)*32+unsigned(t[0]*32)];
      ++direct_bins[unsigned(d[1]*32)*32+unsigned(d[0]*32)];
      require(std::abs(length-1)<2e-5 && cosine>0,"BT leaf hemisphere is not outward cosine domain");
      mean_cosine+=cosine;reflectance[face]=a.reflectance;
    }
    for(unsigned texel=0;texel<1024;++texel)if(oracle.mask[texel])
      require(transport_bins[texel]>=4 && transport_bins[texel]<=7 && direct_bins[texel]>=4 && direct_bins[texel]<=7,
          "BT leaf conditional texel PDF used sprite edge weights or left sampling holes");
    worst_moment=std::max(worst_moment,std::abs(mean_cosine/Samples-2./3));
  }
  require(worst_moment<.004,"BT leaf conditional area changed cosine direction marginal");
  block_transport_complete(r,start);
  std::printf("block_transport_leaf_origins=passed hardware=1 source_png=1 production_world_admission=1 production_sampling=1 production_rt=1 contributor_metadata=1 equal_texel_area=1 six_faces=1 signed_anchor=1 conditional_reflectance=1 origins=%u opaque_texels=%u holes=%u worst_cosine_error=%.9g validation_errors=0\n",
      count,oracle.count,1024-oracle.count,worst_moment);
  return reflectance;
}
}
void block_transport_leaf_cases(Fixture& f) {
  using namespace leaf_probe;auto& r=f.renderer;unsigned material=0;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.leaves")material=i;
  require(material && f.catalog[material].opaque && !f.catalog[material].occlusion && !f.catalog[material].sprite,
      "BT leaf catalog cutout cube required");
  require(prepare_world_atlas_plant_masks(r.atlas),"BT leaf conditional mask resources");
  const unsigned layer=world_atlas_preview_layer(r.atlas,material);
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> color(load_atlas_rgba("Atlases/basegame-color.png"),SDL_DestroySurface);
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> specular(load_atlas_rgba("Atlases/basegame-specular.png"),SDL_DestroySurface);
  const Oracle oracle(color.get(),specular.get(),layer);
  const double coarse=oracle.irradiance(4),fine=oracle.irradiance(8);
  require(std::abs(coarse-fine)/fine<.00005,"BT independent leaf conditional quadrature did not converge");
  require(std::abs(fine-oracle.irradiance(8,false))/fine>.005,"BT leaf oracle cannot distinguish incorrect full-face domain");
  open_world_renderer_set_center(&r,-2,3,0);auto source=column(-2,3,-32,32);
  const auto previous=r.sources.find({-2,3});source.revision=previous==r.sources.end()?1:previous->second.revision+1;
  put(source,16,8,16,std::uint16_t(material));source.blocks.compact();
  require(open_world_renderer_update(&r,source),"BT leaf signed cube publication");plant_probe::settle(r);
  const auto reflectance=origins(f,material,layer,oracle);leaf_probe::energy(f,oracle,reflectance);
  require(r.debug.errors.load()==0,"BT leaf graphics validation errors");
}
}
