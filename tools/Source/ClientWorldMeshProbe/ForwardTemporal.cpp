#include "Probe.h"
#include "CloudRenderer.h"
#include "SlangShaderPath.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace mesh_probe {
namespace {
constexpr unsigned Width=384,Height=216,Warmup=64,Stationary=16,Moving=16;
struct Target {
  Slang::ComPtr<rhi::ITexture> texture;
  Slang::ComPtr<rhi::ITextureView> view;
};
Target target(WorldRenderer& r,unsigned width,unsigned height,rhi::Format format,rhi::TextureUsage usage) {
  Target output;rhi::TextureDesc desc{};desc.size={width,height,1};desc.format=format;
  desc.usage=usage|rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  checked(r.device->createTexture(desc,nullptr,output.texture.writeRef()),"forward texture creation");
  checked(output.texture->getDefaultView(output.view.writeRef()),"forward texture view");return output;
}
std::vector<unsigned char> read(WorldRenderer& r,const Target& target,unsigned width,unsigned height) {
  Slang::ComPtr<ISlangBlob> blob;rhi::SubresourceLayout layout{};
  checked(r.device->readTexture(target.texture,0,0,blob.writeRef(),&layout),"forward image readback");
  require(layout.colPitch==4 && layout.rowPitch>=width*4 && blob->getBufferSize()>=height*layout.rowPitch,"forward RGBA readback extent");
  std::vector<unsigned char> image(width*height*4);
  for(unsigned y=0;y<height;++y)std::memcpy(image.data()+y*width*4,
      static_cast<const char*>(blob->getBufferPointer())+y*layout.rowPitch,width*4);
  return image;
}
void save(const std::filesystem::path& path,const std::vector<unsigned char>& image,unsigned width,unsigned height) {
  auto* surface=SDL_CreateSurfaceFrom(int(width),int(height),SDL_PIXELFORMAT_RGBA32,
      const_cast<unsigned char*>(image.data()),int(width*4));
  require(surface!=nullptr,"forward image surface");const bool saved=SDL_SaveBMP(surface,path.string().c_str());
  SDL_DestroySurface(surface);require(saved,"forward sequence image save");
}
struct Difference {double mean{},maximum{},p99{};unsigned changed{};};
Difference difference(const std::vector<unsigned char>& image,const std::vector<unsigned char>& previous) {
  if(previous.empty())return {};
  require(image.size()==previous.size(),"forward sequence image size changed");
  Difference result;std::vector<double> errors;errors.reserve(image.size()/4);
  for(std::size_t i=0;i<image.size();i+=4) {
    double error{};
    for(unsigned c=0;c<3;++c)error+=std::abs(int(image[i+c])-int(previous[i+c]))/(3.*255);
    result.mean+=error;result.maximum=std::max(result.maximum,error);result.changed+=error>2./255;errors.push_back(error);
  }
  result.mean/=double(errors.size());std::sort(errors.begin(),errors.end());result.p99=errors[std::size_t(std::ceil(double(errors.size())*.99))-1];return result;
}
unsigned material(const Fixture& f,const char* kind) {
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].fluidKind==kind && f.catalog[i].fluidLevel==0)return i;
  throw std::runtime_error("forward fixture source fluid missing");
}
void water(Fixture& f) {
  auto& r=f.renderer;r.columns.clear();r.sources.clear();
  const auto block=static_cast<std::uint16_t>(material(f,"water"));
  for(int z=-8;z<=-5;++z)for(int x=-2;x<=1;++x) {
    auto source=column(x,z,0,1);source.blocks.fill(block);source.blocks.compact();
    r.sources.emplace(std::make_pair(x,z),std::move(source));
  }
  for(const auto& [coordinate,source]:r.sources) {
    WorldColumnGpu gpu;require(world_renderer_mesh(r,source,gpu),"forward production water mesh");
    require(gpu.pass_counts[3]>0,"forward fixture lacks production water faces");
    r.columns.emplace(coordinate,std::move(gpu));
  }
}
void sequence(Fixture& f,const std::filesystem::path& directory,bool clouds,unsigned mode) {
  auto& r=f.renderer;auto& temporal=r.temporal;
  const char* layer=clouds?"clouds":"water";const char* modes[]={"off","native","quality"};
  const auto folder=directory/(std::string(layer)+"-"+modes[mode]);std::filesystem::create_directories(folder);
  r.width=Width;r.height=Height;r.culling_enabled=false;r.radius=32;r.pbr=true;r.pom=true;
  r.sky=make_sky_uniforms(.4,0,{true,false,true,false});r.sky.twilight_celestial_time[3]=0;
  WorldSceneSettings scene_settings;scene_settings.upscaler_mode=mode;scene_settings.fsr_sharpness=.3f;
  configure_temporal(temporal,scene_settings,true);temporal.mode=mode;
  require(resize_temporal(temporal,r.device,Width,Height,1),"forward temporal allocation");
  const auto width=temporal.width,height=temporal.height;
  WorldHdr hdr;require(create_world_hdr(r.device,hdr) && resize_world_hdr(r.device,hdr,width,height),"forward HDR allocation");
  auto depth=target(r,width,height,rhi::Format::D32Float,rhi::TextureUsage::DepthStencil);
  auto material_texture=target(r,width,height,rhi::Format::RGBA8Unorm,rhi::TextureUsage::RenderTarget);
  auto output=target(r,Width,Height,rhi::Format::RGBA8Unorm,rhi::TextureUsage::UnorderedAccess);
  auto raw=target(r,width,height,rhi::Format::RGBA8Unorm,rhi::TextureUsage::UnorderedAccess);
  Slang::ComPtr<rhi::IRenderPipeline> cloud_pipeline;
  const auto cloud_shader=resolve_slang_shader_path("octaryn-client/Shaders/Sky/Clouds.slang");
  require(create_cloud_pipeline(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,cloud_shader.c_str(),cloud_pipeline),"production cloud pipeline");
  std::ofstream csv(folder/"frames.csv");require(bool(csv),"forward sequence report open");
  csv<<"frame,phase,camera_x,camera_y,camera_z,jitter_x,jitter_y,reset,raw_mean_delta,resolved_mean_delta,resolved_p99_delta,resolved_max_delta,changed_pixels,foreground_pixels\n";
  std::vector<unsigned char> previous,previous_raw;
  unsigned covered_peak{},resets{};double static_total{},motion_total{};
  for(unsigned frame=0;frame<Warmup+Stationary+Moving;++frame) {
    WorldCamera source=clouds?WorldCamera{0,64,0,0,.13f,.55f}:WorldCamera{0,12,0,0,-.058f,.55f};
    if(frame>=Warmup+Stationary)source.x=float(frame-Warmup-Stationary+1)*.1f;
    // Keep simulation cadence deterministic even while diagnostic readbacks run.
    if(frame)temporal.last=WorldTemporal::Clock::now()-std::chrono::microseconds(16667);
    const auto camera=begin_temporal(temporal,source,frame);temporal.delta_ms=16.6667f;
    require(!mode || temporal.reset==(frame==0),"forward sequence unexpectedly reset temporal history");
    resets+=mode && temporal.reset;
    world_renderer_prepare_draw(r,camera);
    auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"forward sequence encoder");
    float zero[4]{};
    commands->clearTextureFloat(material_texture.texture,{0,1,0,1},zero);
    if(mode) {
      commands->clearTextureFloat(temporal.targets[0].object_motion,{0,1,0,1},zero);
      commands->setTextureState(temporal.targets[0].object_motion,rhi::ResourceState::ShaderResource);
    }
    float background[4]={.12f,.22f,.34f,1};
    commands->clearTextureFloat(hdr.scene,{0,1,0,1},background);
    if(mode)commands->copyTexture(temporal.targets[0].opaque,{0,1,0,1},{},hdr.scene,{0,1,0,1},{},{width,height,1});
    rhi::RenderPassColorAttachment color{};color.view=hdr.scene_view;color.loadOp=rhi::LoadOp::Load;color.storeOp=rhi::StoreOp::Store;
    rhi::RenderPassDepthStencilAttachment ds{};ds.view=depth.view;ds.depthClearValue=1;ds.depthLoadOp=rhi::LoadOp::Clear;ds.depthStoreOp=rhi::StoreOp::Store;
    ds.stencilLoadOp=rhi::LoadOp::DontCare;ds.stencilStoreOp=rhi::StoreOp::DontCare;
    rhi::RenderPassDesc pass{};pass.colorAttachments=&color;pass.colorAttachmentCount=1;pass.depthStencilAttachment=&ds;
    auto* render=commands->beginRenderPass(pass);require(render!=nullptr,"forward layer render pass");
    rhi::RenderState state{};state.viewportCount=1;state.scissorRectCount=1;
    state.viewports[0]=rhi::Viewport::fromSize(float(width),float(height));state.scissorRects[0]=rhi::ScissorRect::fromSize(width,height);render->setRenderState(state);
    if(clouds) {
      const float position[]={camera.x,camera.y,camera.z};
      require(render_clouds(render,cloud_pipeline,r.sky,position,camera.yaw,camera.pitch,camera.vertical_fov,int(width),int(height),2048,.1f,8192,camera.jitter_x,camera.jitter_y),"production clouds draw");
    } else require(world_renderer_draw(r,render,true),"production water forward draw");
    render->end();
    require(present_world_hdr(commands,hdr,raw.view,width,height),"forward raw presentation");
    require(prepare_temporal(temporal,commands,0,depth.texture,hdr.scene_view,material_texture.view) &&
        resolve_temporal(temporal,commands,0,depth.texture,hdr.scene),"forward production FSR dispatch");
    require(present_world_hdr(commands,hdr,output.view,Width,Height,mode?temporal.targets[0].output_view.get():nullptr),"forward resolved presentation");
    auto submission=commands->finish();require(submission!=nullptr,"forward sequence command finish");
    checked(r.queue->submit(submission),"forward sequence submit");checked(r.queue->waitOnHost(),"forward sequence completion");
    commit_temporal(temporal);
    const auto image=read(r,output,Width,Height),raw_image=read(r,raw,width,height);
    const auto delta=difference(image,previous),raw_delta=difference(raw_image,previous_raw);
    unsigned covered{};for(std::size_t i=0;i<raw_image.size();i+=4)
      covered+=std::abs(int(raw_image[i])-int(raw_image[0]))+std::abs(int(raw_image[i+1])-int(raw_image[1]))+std::abs(int(raw_image[i+2])-int(raw_image[2]))>12;
    covered_peak=std::max(covered_peak,covered);
    if(frame>=Warmup) {
      const bool moving=frame>=Warmup+Stationary;
      csv<<frame<<','<<(moving?"moving":"stationary")<<','<<source.x<<','<<source.y<<','<<source.z<<','<<(mode?temporal.jitter.x:0)<<','<<(mode?temporal.jitter.y:0)<<','<<int(mode && temporal.reset)<<','<<raw_delta.mean<<','<<delta.mean<<','<<delta.p99<<','<<delta.maximum<<','<<delta.changed<<','<<covered<<'\n';
      char name[64];std::snprintf(name,sizeof(name),"frame-%02u.bmp",frame-Warmup);save(folder/name,image,Width,Height);
      std::snprintf(name,sizeof(name),"raw-%02u.bmp",frame-Warmup);save(folder/name,raw_image,width,height);
      (moving?motion_total:static_total)+=delta.mean;
    }
    previous=image;previous_raw=raw_image;
    require(r.debug.errors.load()==0,"forward sequence graphics validation errors");
  }
  require(covered_peak>=256,"forward fixture lacks meaningful distant layer coverage");
  require(!mode || resets==1,"forward fixture reset accumulated history");
  require(bool(csv),"forward sequence report write");
  std::printf("forward_temporal layer=%s mode=%s render=%ux%u output=%ux%u stationary_frames=%u moving_frames=%u stationary_delta=%.9g moving_delta=%.9g coverage=%u history_resets=%u camera_motion=0.1m_per_frame time=frozen\n",
      layer,modes[mode],width,height,Width,Height,Stationary,Moving,static_total/Stationary,motion_total/Moving,covered_peak,resets);
}
}
void forward_temporal_cases(Fixture& fixture) {
  const char* requested=SDL_getenv("OCTARYN_FORWARD_EVIDENCE_PATH");
  require(requested && *requested,"forward sequence requires explicit OCTARYN_FORWARD_EVIDENCE_PATH");
  const auto directory=std::filesystem::path(reinterpret_cast<const char8_t*>(requested));std::filesystem::create_directories(directory);
  require(fixture.renderer.columns.empty(),"forward sequence requires fresh fixture");
  for(unsigned mode:{0u,1u,2u})sequence(fixture,directory,true,mode);
  water(fixture);for(unsigned mode:{0u,1u,2u})sequence(fixture,directory,false,mode);
  std::puts("forward_temporal_sequence=passed layers=clouds,water modes=off,native,quality measured_frames=192 warmup_frames=384 production_raster=1 production_fsr=1 windows=0");
}
}
