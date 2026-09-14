#include "PlayerRenderer.h"
#include "PlayerModel.h"
#include "WorldRenderer.h"
#include "SlangShaderPath.h"
#include "AssetPath.h"
#include <slang-com-ptr.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace octaryn::client::rendering;
constexpr unsigned Size=128;
void require(bool value,const char* reason) {if(!value) throw std::runtime_error(reason);}
void checked(SlangResult result,const char* reason) {require(SLANG_SUCCEEDED(result),reason);}
struct Debug final:rhi::IDebugCallback {
  std::atomic<unsigned> errors{},warnings{};
  void SLANG_MCALL handleMessage(rhi::DebugMessageType type,rhi::DebugMessageSource,const char* text) noexcept override {
    if(type==rhi::DebugMessageType::Error) ++errors;
    if(type==rhi::DebugMessageType::Warning) ++warnings;
    std::fprintf(stderr,"player_rhi severity=%s %s\n",type==rhi::DebugMessageType::Error?"error":
        type==rhi::DebugMessageType::Warning?"warning":"info",text?text:"");
  }
};
float half_float(std::uint16_t bits) {
  const int exponent=(bits>>10)&31,sign=(bits&32768)?-1:1;
  const unsigned mantissa=bits&1023;
  require(exponent!=31,"nonfinite HDR pixel");
  return static_cast<float>(sign)*std::ldexp(static_cast<float>(exponent?mantissa+1024:mantissa),
      exponent?exponent-25:-24);
}
struct Image {
  std::vector<std::array<std::uint16_t,4>> color{Size*Size};
  std::vector<float> depth=std::vector<float>(Size*Size);
  unsigned covered{};
};
void save_bmp(const Image& image,const std::filesystem::path& path) {
  // This diagnostic encodes HDR readback with Reinhard + gamma for inspection;
  // all assertions below use the original HDR/depth values, not this preview.
  std::ofstream file(path,std::ios::binary);require(bool(file),"cannot open diagnostic image");
  const auto u16=[&](std::uint16_t value) {char b[2]={char(value),char(value>>8)};file.write(b,2);};
  const auto u32=[&](std::uint32_t value) {char b[4]={char(value),char(value>>8),char(value>>16),char(value>>24)};file.write(b,4);};
  file.write("BM",2);u32(54+Size*Size*4);u32(0);u32(54);u32(40);u32(Size);u32(Size);
  u16(1);u16(32);u32(0);u32(Size*Size*4);u32(0);u32(0);u32(0);u32(0);
  for(unsigned row=0;row<Size;++row) for(unsigned x=0;x<Size;++x) {
    const auto& pixel=image.color[(Size-1-row)*Size+x];
    for(unsigned channel:{2u,1u,0u}) {
      const float linear=std::max(half_float(pixel[channel]),0.f);
      const auto encoded=static_cast<unsigned char>(std::lround(std::pow(linear/(1+linear),1/2.2f)*255));
      file.put(static_cast<char>(encoded));
    }
    file.put(static_cast<char>(255));
  }
  require(bool(file),"diagnostic image write failed");
}
unsigned changed_pixels(const Image& a,const Image& b) {
  unsigned count{};
  for(std::size_t i=0;i<a.color.size();++i) if(a.color[i]!=b.color[i]) ++count;
  return count;
}
unsigned changed_silhouette(const Image& a,const Image& b) {
  unsigned count{};
  for(std::size_t i=0;i<a.depth.size();++i) if((a.depth[i]<1)!=(b.depth[i]<1)) ++count;
  return count;
}
struct Fixture {
  Debug debug;
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ICommandQueue> queue;
  Slang::ComPtr<rhi::ITexture> color,depth;
  Slang::ComPtr<rhi::ITextureView> color_view,depth_view;
  std::unique_ptr<PlayerRenderer,decltype(&destroy_player_renderer)> player{nullptr,destroy_player_renderer};
  PlayerModel model;
  WorldCamera camera;
  std::string asset_path,shader_path;
  explicit Fixture() {
    char path[4096]{};
    require(bundle_path_build(path,sizeof(path),"Client/Assets/Player/octaryn_player_v1.gltf"),"player asset path");
    asset_path=path;shader_path=resolve_slang_shader_path("octaryn-client/Shaders/Player/Player.slang");
    std::string error;
    require(load_player_model(std::filesystem::path(reinterpret_cast<const char8_t*>(path)),model,error),error.c_str());
    require(model.indices.size()==216 && model.first_person_indices.size()==72,"retained full/arms-only index counts");
    require(std::equal(model.first_person_indices.begin(),model.first_person_indices.end(),model.indices.begin()+72),
        "GPU fixture must submit exactly the authored arm triangles");
    for(const auto index:model.first_person_indices)
      require(index>=48 && index<96,"GPU first-person index buffer contains non-arm vertices");
    std::array<float,3> low{},high{};
    std::copy_n(model.vertices.front().position,3,low.begin());high=low;
    for(const auto& vertex:model.vertices) for(unsigned a=0;a<3;++a) {
      low[a]=std::min(low[a],vertex.position[a]);high[a]=std::max(high[a],vertex.position[a]);
    }
    const float dx=high[0]-low[0],dy=high[1]-low[1],dz=high[2]-low[2];
    const float radius=.5f*std::sqrt(dx*dx+dy*dy+dz*dz);
    require(radius>0,"empty model bounds");
    const std::array<float,3> center{(low[0]+high[0])*.5f,(low[1]+high[1])*.5f,(low[2]+high[2])*.5f};
    camera={center[0]+radius*1.4f,center[1],center[2]+radius*3,
      std::atan2(-1.4f,3.f),0,1.04719755f};
    std::printf("player_fixture bounds=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f) camera=(%.3f,%.3f,%.3f) full_indices=216 filtered_indices=72 first_person=arms_only\n",
      low[0],low[1],low[2],high[0],high[1],high[2],camera.x,camera.y,camera.z);
    auto* api=rhi::getRHI();require(api!=nullptr,"standalone RHI unavailable");
    rhi::DebugLayerOptions validation{};validation.coreValidation=true;
    checked(api->setDebugLayerOptions(validation),"core validation setup");
    const rhi::Feature features[]={rhi::Feature::Rasterization};
    rhi::DeviceDesc desc{};desc.deviceType=rhi::DeviceType::Vulkan;
    desc.requiredFeatures=features;desc.requiredFeatureCount=1;
    desc.slang.targetFlags=SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
    desc.slang.targetProfile="spirv_1_3";desc.enableValidation=true;desc.debugCallback=&debug;
    checked(api->createDevice(desc,device.writeRef()),"headless Vulkan device");
    checked(device->getQueue(rhi::QueueType::Graphics,queue.writeRef()),"graphics queue");
    const auto& info=device->getInfo();
    std::printf("player_rendering_device backend=slang_rhi api=%s adapter=%s size=%u surface=none\n",info.apiName,info.adapterName,Size);
    rhi::TextureDesc texture{};texture.size={Size,Size,1};texture.format=rhi::Format::RGBA16Float;
    texture.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource;
    texture.defaultState=rhi::ResourceState::RenderTarget;
    checked(device->createTexture(texture,nullptr,color.writeRef()),"HDR target");
    checked(color->getDefaultView(color_view.writeRef()),"HDR view");
    texture.format=rhi::Format::D32Float;texture.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::CopySource;
    texture.defaultState=rhi::ResourceState::DepthWrite;
    checked(device->createTexture(texture,nullptr,depth.writeRef()),"depth target");
    checked(depth->getDefaultView(depth_view.writeRef()),"depth view");
    reset_player();
  }
  ~Fixture() {if(queue) queue->waitOnHost();}
  void shutdown() {
    checked(queue->waitOnHost(),"final GPU completion");
    player.reset();color_view.setNull();depth_view.setNull();color.setNull();depth.setNull();
    queue.setNull();device.setNull();
    require(debug.errors.load()==0 && debug.warnings.load()==0,"RHI/Vulkan teardown diagnostic");
  }
  void reset_player() {
    player.reset(create_player_renderer(device,rhi::Format::RGBA16Float,rhi::Format::D32Float,asset_path.c_str(),shader_path.c_str()));
    require(player!=nullptr,"production player renderer creation");
  }
  Image render(const PlayerPose& pose,unsigned instances=1) {
    auto commands=queue->createCommandEncoder();require(commands!=nullptr,"command encoder");
    rhi::RenderPassColorAttachment target{};target.view=color_view;
    target.loadOp=rhi::LoadOp::Clear;target.storeOp=rhi::StoreOp::Store;
    rhi::RenderPassDepthStencilAttachment z{};z.view=depth_view;z.depthClearValue=1;
    z.depthLoadOp=rhi::LoadOp::Clear;z.depthStoreOp=rhi::StoreOp::Store;
    z.stencilLoadOp=rhi::LoadOp::DontCare;z.stencilStoreOp=rhi::StoreOp::DontCare;
    rhi::RenderPassDesc pass{};pass.colorAttachments=&target;pass.colorAttachmentCount=1;pass.depthStencilAttachment=&z;
    auto* render=commands->beginRenderPass(pass);require(render!=nullptr,"player render pass");
    rhi::RenderState state{};state.viewportCount=1;state.scissorRectCount=1;
    state.viewports[0]=rhi::Viewport::fromSize(float(Size),float(Size));
    state.scissorRects[0]=rhi::ScissorRect::fromSize(Size,Size);render->setRenderState(state);
    bool drawn=true;
    for(unsigned i=0;i<instances && drawn;++i)
      drawn=render_player(player.get(),render,camera,Size,Size,pose,PlayerLighting{});
    render->end();require(drawn,"production player draw");
    auto submission=commands->finish();require(submission!=nullptr,"command finish");
    checked(queue->submit(submission),"player submission");checked(queue->waitOnHost(),"player completion");
    Image image;Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
    const auto read=[&](rhi::ITexture* texture,void* destination,std::size_t stride) {
      checked(device->readTexture(texture,0,0,pixels.writeRef(),&layout),"texture readback");
      require(pixels && layout.colPitch==stride && layout.rowPitch>=Size*stride &&
          pixels->getBufferSize()>=layout.rowPitch*Size,"texture readback layout");
      for(unsigned y=0;y<Size;++y) std::memcpy(static_cast<unsigned char*>(destination)+y*Size*stride,
          static_cast<const unsigned char*>(pixels->getBufferPointer())+y*layout.rowPitch,Size*stride);
    };
    read(color,image.color.data(),8);read(depth,image.depth.data(),4);
    for(std::size_t i=0;i<image.depth.size();++i) {
      const float value=image.depth[i];require(std::isfinite(value) && value>=0 && value<=1,"nonfinite/out-of-range depth");
      float rgb{};for(unsigned channel=0;channel<4;++channel) {
        const float component=half_float(image.color[i][channel]);require(component>=0,"negative HDR pixel");
        if(channel<3) rgb+=component;
      }
      if(value<1) {++image.covered;require(rgb>0 && half_float(image.color[i][3])>.9f,"geometry lacks shaded HDR output");}
      else require(image.color[i]==std::array<std::uint16_t,4>{},"background color changed without depth");
    }
    require(debug.errors.load()==0 && debug.warnings.load()==0,"RHI/Vulkan validation diagnostic");
    return image;
  }
  void animation(PlayerClip clip,const char* name,const std::filesystem::path& output) {
    const auto found=std::find_if(model.animations.begin(),model.animations.end(),[&](const auto& value){return value.name==name;});
    require(found!=model.animations.end(),"authored clip missing");
    reset_player();PlayerPose pose;pose.first_person=false;pose.clip=clip;pose.source_seconds=10;
    render(pose); // Initialize this production animator's clock, without a clip transition.
    pose.source_seconds=10+found->duration*.25;const auto first=render(pose);
    pose.source_seconds=10+found->duration*.75;const auto last=render(pose);
    const auto color_changes=changed_pixels(first,last),silhouette_changes=changed_silhouette(first,last);
    std::printf("player_animation clip=%s first_time=%.6f last_time=%.6f first_pixels=%u last_pixels=%u hdr_changed=%u silhouette_changed=%u\n",
        name,10+found->duration*.25,pose.source_seconds,first.covered,last.covered,color_changes,silhouette_changes);
    require(first.covered>64 && last.covered>64 && color_changes>8 && silhouette_changes>0,"authored animation must change production GPU image");
    const auto held=render(pose);require(changed_pixels(last,held)==0 && last.depth==held.depth,"held source clock changed rendered pose");
    save_bmp(first,output/(std::string("player-rendering-")+name+"-a.bmp"));
    save_bmp(last,output/(std::string("player-rendering-")+name+"-b.bmp"));
  }
};
}
int main(int argc,char** argv) {
  try {
    require(argc==2,"usage: player_rendering_probe evidence-directory");
    const auto output=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[1]));
    std::filesystem::create_directories(output);
    Fixture fixture;PlayerPose pose;pose.visible=false;pose.first_person=false;
    const auto hidden=fixture.render(pose);require(hidden.covered==0,"hidden-player baseline must stay empty");
    pose.visible=true;const auto full=fixture.render(pose);require(full.covered>128,"full third-person mesh must be visible");
    pose.first_person=true;const auto filtered=fixture.render(pose);
    require(filtered.covered>64 && filtered.covered<full.covered,"first-person subset must retain arms and remove head, torso, and legs");
    for(std::size_t i=0;i<full.depth.size();++i) require(filtered.depth[i]>=full.depth[i],"filtered subset cannot add foreground geometry");
    pose.first_person=false;const auto restored=fixture.render(pose);
    require(changed_pixels(full,restored)==0 && full.depth==restored.depth,"mesh-mode switch changed held full pose");
    std::printf("player_visibility hidden_pixels=%u third_person_pixels=%u first_person_arm_pixels=%u removed_pixels=%u framing=external_fixture\n",
        hidden.covered,full.covered,filtered.covered,full.covered-filtered.covered);
    save_bmp(full,output/"player-rendering-third-person.bmp");save_bmp(filtered,output/"player-rendering-first-person-arms.bmp");
    for(unsigned frame=0;frame<2;++frame) {
      const auto crowded=fixture.render(pose,4096);
      require(changed_pixels(full,crowded)==0 && full.depth==crowded.depth,"descriptor pool growth/reset changed production draw");
    }
    std::puts("player_descriptor_stress=passed instances_per_frame=4096 frames=2 pool_growth_and_reset=passed");
    fixture.animation(PlayerClip::Walk,"walk_loop",output);
    fixture.animation(PlayerClip::Attack,"attack_slash_once",output);
    fixture.shutdown();
    std::printf("client_player_rendering_probe=passed validation_errors=%u validation_warnings=%u surfaces=0 production_renderer=PlayerRenderer\n",
        fixture.debug.errors.load(),fixture.debug.warnings.load());
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"client_player_rendering_probe=failed reason=%s\n",error.what());return 1;
  }
}
