#include "BlockTransportRasterTrace.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace mesh_probe {
Image Fixture::render(const WorldColumnGpu& mesh,const WorldCamera& camera,bool pbr,bool pom,bool retained_columns) {
  auto& r=renderer;
  BlockTransportRasterTrace trace(r,"render");
  // This synchronous fixture owns one attachment set, independent of runtime frame slots.
  const auto& fixture_target=r.targets[0];
  require(fixture_target.depth && fixture_target.depth_view,"fixture-owned depth attachment is missing");
  if(!retained_columns) {
    trace.mark("columns_clear_begin");r.columns.clear();trace.mark("columns_cleared");
    r.columns.emplace(std::make_pair(0,0),mesh);trace.mark("column_inserted");
  }
  r.pbr=pbr;r.pom=pom;trace.mark("prepare_draw_begin");world_renderer_prepare_draw(r,camera);trace.mark("draw_prepared");
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"MRT command encoder");
  require(world_batch_prepare(r,commands),"batch preparation");trace.mark("batch_prepared");
  std::array<rhi::RenderPassColorAttachment,world_gbuffer_formats.size()> colors{};
  const auto target_count=world_gbuffer_target_count(r.gi_mode==GiMode::BlockTransport);
  for(unsigned i=0;i<target_count;++i) {require(bool(views[i]),"fixture-owned color attachment is missing");colors[i].view=views[i];colors[i].loadOp=rhi::LoadOp::Clear;colors[i].storeOp=rhi::StoreOp::Store;}
  rhi::RenderPassDepthStencilAttachment depth{};depth.view=fixture_target.depth_view;depth.depthClearValue=1;
  depth.depthLoadOp=rhi::LoadOp::Clear;depth.depthStoreOp=rhi::StoreOp::Store;
  depth.stencilLoadOp=rhi::LoadOp::DontCare;depth.stencilStoreOp=rhi::StoreOp::DontCare;
  rhi::RenderPassDesc pass{};pass.colorAttachments=colors.data();pass.colorAttachmentCount=target_count;pass.depthStencilAttachment=&depth;
  auto* encoder=commands->beginRenderPass(pass);require(encoder!=nullptr,"MRT render pass");
  rhi::RenderState state{};state.viewportCount=1;state.scissorRectCount=1;
  require(r.width>0 && r.height>0 && r.width<=int(Size) && r.height<=int(Size),"fixture viewport exceeds owned attachments");
  state.viewports[0]=rhi::Viewport::fromSize(float(r.width),float(r.height));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(static_cast<unsigned>(r.width),static_cast<unsigned>(r.height));
  encoder->setRenderState(state);require(world_renderer_draw(r,encoder,false),"production MRT draw");encoder->end();trace.mark("draw_encoded");
  auto submission=commands->finish();require(submission!=nullptr,"MRT command finish");
  trace.mark("command_finished");
  require(r.frame_queue.submit(r.queue,submission,r.active_frame),"MRT submit");trace.mark("submitted");
  require(r.frame_queue.wait(r.active_frame,2000),"MRT bounded completion");trace.mark("draw_completed");
  const auto read=[&](rhi::ITexture* texture,void* destination,std::size_t stride,unsigned target) {
    require(texture!=nullptr,"fixture readback texture is missing");trace.mark("texture_read_begin",target);
    Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
    checked(r.device->readTexture(texture,0,0,pixels.writeRef(),&layout),"MRT readback");
    require(pixels && layout.colPitch==stride && layout.rowPitch>=Size*stride && pixels->getBufferSize()>=layout.rowPitch*Size,"MRT readback layout");
    for(unsigned y=0;y<Size;++y) std::memcpy(static_cast<unsigned char*>(destination)+y*Size*stride,
        static_cast<const unsigned char*>(pixels->getBufferPointer())+y*layout.rowPitch,Size*stride);
    trace.mark("texture_read_complete",target);
  };
  Image result;result.depth.resize(Size*Size);read(fixture_target.depth,result.depth.data(),4,6);
  constexpr std::array<unsigned,4> strides{8,16,4,4};
  for(unsigned i=0;i<strides.size();++i) {
    result.mrt[i].resize(Size*Size*strides[i]);read(targets[i],result.mrt[i].data(),strides[i],i);
  }
  trace.mark("readback_completed");
  require(r.debug.errors.load()==0,"Vulkan/RHI validation errors");return result;
}
namespace {
float half(std::uint16_t bits) {
  const int exponent=(bits>>10)&31,sign=(bits&32768)?-1:1;const unsigned mantissa=bits&1023;
  require(exponent!=31,"nonfinite half-float G-buffer");
  return static_cast<float>(sign)*std::ldexp(static_cast<float>(exponent?mantissa+1024:mantissa),exponent?exponent-25:-24);
}
void save_color(const Image& image,const char* path) {
  std::vector<unsigned char> rgba(Fixture::Size*Fixture::Size*4);
  for(std::size_t i=0;i<rgba.size();++i) {
    std::uint16_t bits{};std::memcpy(&bits,image.mrt[0].data()+i*2,2);
    const float value=std::clamp(half(bits),0.f,1.f);
    rgba[i]=i%4==3?255:static_cast<unsigned char>(std::lround(std::pow(value,1.f/2.2f)*255));
  }
  auto* surface=SDL_CreateSurfaceFrom(Fixture::Size,Fixture::Size,SDL_PIXELFORMAT_RGBA32,rgba.data(),Fixture::Size*4);
  require(surface!=nullptr,"diagnostic surface");const bool saved=SDL_SaveBMP(surface,path);SDL_DestroySurface(surface);
  require(saved,"diagnostic BMP save");
}
}
void Fixture::compare_raster(const char* name,const Mesh& merged,const Mesh& units,const WorldCamera& camera,bool pbr,bool pom) {
  const auto expected_image=render(units.gpu,camera,pbr,pom),actual=render(merged.gpu,camera,pbr,pom);
  unsigned voxel_changes=0;
  for(std::size_t i=0;i<actual.depth.size();++i)if(actual.depth[i]<1 && expected_image.depth[i]<1) {
    std::uint32_t av{},bv{};
    std::memcpy(&av,actual.mrt[2].data()+i*4,4);std::memcpy(&bv,expected_image.mrt[2].data()+i*4,4);
    if(av==bv)continue;
    if(voxel_changes++<8)std::printf("world_mesh_voxel_difference case=%s pixel=%zu,%zu actual=%u expected=%u depth_actual=%.9g depth_expected=%.9g\n",
        name,i%Size,i/Size,av,bv,actual.depth[i],expected_image.depth[i]);
  }
  if(voxel_changes)std::printf("world_mesh_voxel_changes case=%s count=%u\n",name,voxel_changes);
  unsigned covered=0;float max_depth=0,max_color=0,max_relative=0;unsigned max_material=0;
  unsigned color_channels_changed=0,color_channels_outside=0;std::size_t worst=0;
  unsigned coverage_changed=0,unclassified_coverage=0;
  bool relative_valid=true;
  std::vector<bool> outside_color(actual.depth.size()),outside_material(actual.depth.size());
  for(std::size_t i=0;i<actual.depth.size();++i) {
    const float a=actual.depth[i],b=expected_image.depth[i];
    require(std::isfinite(a) && std::isfinite(b) && a>=0 && a<=1 && b>=0 && b<=1,"invalid depth output");
    if((a<1)!=(b<1)) {
      const auto distance=projected_edge_distance(merged,camera,static_cast<unsigned>(i%Size),static_cast<unsigned>(i/Size));
      bool silhouette=false;
      for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        const int x=int(i%Size)+dx,y=int(i/Size)+dy;
        if(x>=0 && y>=0 && x<int(Size) && y<int(Size)) {
          const auto neighbor=static_cast<std::size_t>(y)*Size+static_cast<unsigned>(x);
          silhouette|=actual.depth[neighbor]>=1 && expected_image.depth[neighbor]>=1;
        }
      }
      if(distance>1.f/256 || !silhouette)++unclassified_coverage;
      if(++coverage_changed<=8)std::printf("world_mesh_coverage case=%s pixel=%zu,%zu actual=%g expected=%g edge_distance=%g\n",name,i%Size,i/Size,a,b,distance);
      continue;
    }
    if(a<1) ++covered;max_depth=std::max(max_depth,std::abs(a-b));
    for(unsigned target=0;target<4;++target) for(unsigned channel=0;channel<4;++channel) {
      if(target==0) {
        std::uint16_t av{},bv{};const auto offset=(i*4+channel)*2;
        std::memcpy(&av,actual.mrt[target].data()+offset,2);std::memcpy(&bv,expected_image.mrt[target].data()+offset,2);
        const float error=std::abs(half(av)-half(bv));
        if(error>0)++color_channels_changed;if(error>.008f){++color_channels_outside;outside_color[i]=true;}
        if(error>max_color){max_color=error;worst=i;}
      } else if(target==1) {
        float av{},bv{};const auto offset=(i*4+channel)*sizeof(float);
        std::memcpy(&av,actual.mrt[target].data()+offset,sizeof(float));
        std::memcpy(&bv,expected_image.mrt[target].data()+offset,sizeof(float));
        require(std::isfinite(av) && std::isfinite(bv),"nonfinite float32 G-buffer position");
        const float error=std::abs(av-bv),magnitude=std::max(std::abs(av),std::abs(bv));
        const float ulp=magnitude>0?std::ldexp(1.f,std::ilogb(magnitude)-23):0;
        max_relative=std::max(max_relative,error);
        relative_valid&=error<=std::max(.0001f,4*ulp);
      } else {
        const auto av=actual.mrt[target][i*4+channel],bv=expected_image.mrt[target][i*4+channel];
        if(target==2) require(av==bv,"atlas layer/direction G-buffer changed");
        else {
          const auto error=static_cast<unsigned>(std::abs(int(av)-int(bv)));
          max_material=std::max(max_material,error);if(error>2)outside_material[i]=true;
        }
      }
    }
  }
  std::printf("world_mesh_raster case=%s pbr=%u pom=%u covered=%u depth_error=%g color_error=%g relative_error=%g material_error=%u\n",
      name,pbr,pom,covered,max_depth,max_color,max_relative,max_material);
  require(covered>32,"raster fixture did not render meaningful terrain");
  if(coverage_changed) {
    std::printf("world_mesh_coverage case=%s changed=%u\n",name,coverage_changed);
    if(unclassified_coverage || coverage_changed>covered/100+1) {
      save_color(actual,"logs/client/world-mesh-actual.bmp");save_color(expected_image,"logs/client/world-mesh-units.bmp");
    }
    require(unclassified_coverage==0 && coverage_changed<=covered/100+1,"greedy raster coverage changed away from subpixel face edges");
  }
  // Allow float32 position interpolation rounding; color remains half float.
  // Silhouette exceptions are bounded above.
  require(max_depth<=.000002f && relative_valid,"merged geometry changed depth/relative position");
  if(max_color>.008f || max_material>2) {
    const auto boundaries=sampling_boundaries(*this,merged,units,camera,pbr,pom,outside_color,actual,expected_image);
    unsigned classified=0,unclassified=0;
    for(std::size_t i=0;i<outside_color.size();++i)if(outside_color[i] || outside_material[i]) {
      if((!outside_color[i] || boundaries.albedo[i]) && (!outside_material[i] || boundaries.material[i]))++classified;
      else {++unclassified;if(unclassified<=4)std::printf("world_mesh_unclassified pixel=%zu,%zu\n",i%Size,i/Size);}
    }
    std::printf("world_mesh_precision classified_pixels=%u unclassified_pixels=%u covered=%u\n",classified,unclassified,covered);
    if(unclassified || classified>covered/100+1) {
      std::printf("world_mesh_pixel case=%s changed_channels=%u outside_channels=%u worst_xy=%zu,%zu\n",
          name,color_channels_changed,color_channels_outside,worst%Size,worst/Size);
      save_color(actual,"logs/client/world-mesh-actual.bmp");save_color(expected_image,"logs/client/world-mesh-units.bmp");
    }
    require(unclassified==0 && classified<=covered/100+1,"merged sampling changed outside sparse proven nearest-boundary rounding");
  }
}
}
