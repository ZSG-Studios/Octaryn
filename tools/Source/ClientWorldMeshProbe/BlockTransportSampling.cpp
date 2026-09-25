#include "BlockTransportSampling.h"
#include "BlockTransportSetup.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>

namespace mesh_probe {
namespace {
constexpr unsigned Samples=4096;
constexpr double Pi=3.14159265358979323846;
using Pixel=std::array<float,4>;
using Color=std::array<double,3>;
static_assert(sizeof(BlockTransportSampleQuery)==32 && sizeof(BlockTransportSampleResult)==80);
Slang::ComPtr<rhi::IBuffer> sample_buffer(WorldRenderer& r,const void* data,std::size_t bytes,
    unsigned stride,rhi::BufferUsage usage) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;desc.usage=usage|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  Slang::ComPtr<rhi::IBuffer> result;
  checked(r.device->createBuffer(desc,data,result.writeRef()),"BT sampling fixture buffer");return result;
}
// Independent area-integral reference: a finite-range point source above a unit square.
double source(double u,double v) {
  const double squared=(u-.13)*(u-.13)+(v-.79)*(v-.79)+.64;
  const double fade=std::pow(1-squared*squared/std::pow(24.,4),2);
  return .8*fade/(Pi*squared*std::sqrt(squared));
}
double reference(unsigned side) {
  double value=0;
  for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x)
    value+=source((x+.5)/side,(y+.5)/side);
  return value/(double(side)*side);
}
Color local(Pixel value,unsigned direction) {
  const unsigned axis=direction/2,u=axis==0?2:0,v=axis==1?2:1;
  const double sign=direction%2?1:-1;
  // cross(normal,tangent) is positive only for -X, -Y and +Z.
  const double handed=direction==0 || direction==2 || direction==5?1:-1;
  return {value[u],value[v]*handed,value[axis]*sign};
}
unsigned cell(double value) {return std::min(15u,unsigned(value*16));}
void audit_mapping(double mapped,double sample,float world,unsigned face,unsigned index,
    const char* domain,unsigned component,unsigned& rounded,double& worst) {
  require(std::isfinite(sample) && sample>0 && sample<1,"BT sequence sample is outside its open domain");
  const double ulp=std::max(double(std::nextafter(world,INFINITY))-world,
      double(world)-std::nextafter(world,-INFINITY));
  const double error=std::abs(mapped-sample);worst=std::max(worst,error);
  require(error<=ulp+1./16777216,"BT world sample differs from its sequence by more than float rounding");
  if(cell(mapped)==cell(sample))return;
  const double boundary=double(std::max(cell(mapped),cell(sample)))/16;
  const bool rounding=cell(mapped)==cell(sample)+1 && mapped==boundary && std::abs(sample-boundary)<=ulp*.5;
  if(!rounding) {
    std::printf("block_transport_sampling_bin_failure face=%u sample=%u domain=%s component=%u "
        "raw=%.17g mapped=%.17g world=%.9g raw_bin=%u mapped_bin=%u boundary=%.17g ulp=%.17g\n",
        face,index,domain,component,sample,mapped,world,cell(sample),cell(mapped),boundary,ulp);
    std::fflush(stdout);
  }
  require(rounding,"BT mapped dyadic cell differs without a half-ULP boundary rounding");++rounded;
}
}
std::vector<BlockTransportSampleResult> block_transport_samples(Fixture& f,
    const std::vector<BlockTransportSampleQuery>& queries) {
  require(!queries.empty(),"BT sample queries are empty");auto& r=f.renderer;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportSampling.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BT production sampling pipeline");
  auto input=sample_buffer(r,queries.data(),queries.size()*sizeof(queries[0]),sizeof(queries[0]),rhi::BufferUsage::ShaderResource);
  auto output=sample_buffer(r,nullptr,queries.size()*sizeof(BlockTransportSampleResult),sizeof(BlockTransportSampleResult),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT sampling frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT sampling command encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT sampling compute pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT sampling pipeline binding");
  require(bind_world_atlas(r.atlas,root),"BT sampling conditional plant resources");
  rhi::ShaderCursor cursor(root);const unsigned count=unsigned(queries.size());
  checked(cursor["probeSampleQueries"].setBinding(input),"BT sample queries");
  checked(cursor["probeSampleResults"].setBinding(output),"BT sample results");
  checked(cursor["probeSampleCount"].setData(&count,sizeof(count)),"BT sample count");
  pass->dispatchCompute((count+63)/64,1,1);pass->end();
  auto submission=commands->finish();require(bool(submission),"BT sampling command finish");
  require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
      "BT sampling bounded completion");
  std::vector<BlockTransportSampleResult> result(count);
  checked(r.device->readBuffer(output,0,result.size()*sizeof(result[0]),result.data()),"BT production sample readback");
  block_transport_complete(r,start);return result;
}
void block_transport_sampling_cases(Fixture& f) {
  std::vector<BlockTransportSampleQuery> queries;
  for(unsigned face=0;face<6;++face)for(unsigned sample=0;sample<Samples;++sample)
    queries.push_back({{-3,7,-11,face},{sample/16,sample%16,sample,0}});
  const unsigned ordinary=unsigned(queries.size());
  for(unsigned batch:{0u,1u,0x0fffffffu,0x10000000u,0xfffffffeu,0xffffffffu})
    queries.push_back({{-3,7,-11,3},{batch,15,batch,0}});
  const auto actual=block_transport_samples(f,queries);
  const double integral=reference(512),refined=reference(1024);
  require(std::abs(integral-refined)<1e-6,"BT independent point source quadrature did not converge");
  double worst_moment=0,worst_integral=0,worst_mapping=0;unsigned rounded_boundaries=0;
  for(unsigned face=0;face<6;++face) {
    Color first{},second{};double area_light=0,joint=0;
    std::array<unsigned,256> area_bins{},angle_bins{},direct_bins{},sequence_angle_bins{};
    const unsigned axis=face/2,u=axis==0?2:0,v=axis==1?2:1;
    const double handed=face==0 || face==2 || face==5?1:-1;
    for(unsigned sample=0;sample<Samples;++sample) {
      const auto& value=actual[face*Samples+sample];
      require(value.origin[3]==1 && value.direct[3]==1,"BT valid cube surface sampling failed");
      const double base[3]={-2.5,7.5,-10.5};
      const double sign=face%2?1:-1,plane=base[axis]+sign*.5;
      const double step=std::abs(double(std::nextafter(float(plane),sign>0?INFINITY:-INFINITY))-plane);
      const double displacement=(value.origin[axis]-plane)*sign;
      require(displacement>0 && displacement<=std::max(step,1./1048576)+step &&
          std::abs(value.direct[axis]-plane)<1e-6,"BT sample violates outward representable owner plane");
      const double pu=value.origin[u]-base[u]+.5,pv=(value.origin[v]-base[v])*handed+.5;
      const double du=value.direct[u]-base[u]+.5,dv=(value.direct[v]-base[v])*handed+.5;
      const bool in_face=pu>=0 && pu<1 && pv>=0 && pv<1 && du>=0 && du<1 && dv>=0 && dv<1;
      if(!in_face) {
        const auto& query=queries[face*Samples+sample];
        std::printf("block_transport_sampling_failure face=%u sample=%u key=(%d,%d,%d,%u) batch=%u slot=%u direct_batch=%u "
            "origin=(%.9g,%.9g,%.9g,%.9g) direction=(%.9g,%.9g,%.9g) direct=(%.9g,%.9g,%.9g,%.9g) "
            "area_uv=(%.17g,%.17g) direct_uv=(%.17g,%.17g) axis=%u u=%u v=%u handed=%.1f\n",
            face,sample,query.key.x,query.key.y,query.key.z,query.key.direction,query.info[0],query.info[1],query.info[2],
            value.origin[0],value.origin[1],value.origin[2],value.origin[3],value.direction[0],value.direction[1],value.direction[2],
            value.direct[0],value.direct[1],value.direct[2],value.direct[3],pu,pv,du,dv,axis,u,v,handed);
        std::fflush(stdout);
      }
      require(in_face,"BT area sample left exact owner face");
      audit_mapping(pu,value.sequence[0],value.origin[u],face,sample,"transport",0,rounded_boundaries,worst_mapping);
      audit_mapping(pv,value.sequence[1],value.origin[v],face,sample,"transport",1,rounded_boundaries,worst_mapping);
      audit_mapping(du,value.direct_sequence[0],value.direct[u],face,sample,"direct",0,rounded_boundaries,worst_mapping);
      audit_mapping(dv,value.direct_sequence[1],value.direct[v],face,sample,"direct",1,rounded_boundaries,worst_mapping);
      const auto direction=local(value.direction,face);
      double squared=0;for(unsigned c=0;c<3;++c) {
        require(std::isfinite(direction[c]),"BT nonfinite hemisphere sample");
        first[c]+=direction[c];second[c]+=direction[c]*direction[c];squared+=direction[c]*direction[c];
      }
      require(direction[2]>0 && std::abs(squared-1)<2e-6,"BT sample violates unit outward hemisphere");
      const double radius=std::clamp(1-direction[2]*direction[2],0.,std::nextafter(1.,0.));
      double angle=std::atan2(direction[1],direction[0])/(2*Pi);if(angle<0)angle+=1;
      require(value.sequence[2]>0 && value.sequence[2]<1 && value.sequence[3]>0 && value.sequence[3]<1,
          "BT angular sequence sample is outside its open domain");
      const double sequence_angle=double(value.sequence[3])*2*Pi,sequence_radius=std::sqrt(value.sequence[2]);
      const Color expected_direction={sequence_radius*std::cos(sequence_angle),sequence_radius*std::sin(sequence_angle),
          std::sqrt(1-double(value.sequence[2]))};
      for(unsigned c=0;c<3;++c)require(std::abs(direction[c]-expected_direction[c])<2e-6,
          "BT hemisphere mapping differs from the actual production sequence");
      ++area_bins[cell(value.sequence[0])+16*cell(value.sequence[1])];
      ++angle_bins[cell(radius)+16*cell(angle)];
      ++direct_bins[cell(value.direct_sequence[0])+16*cell(value.direct_sequence[1])];
      ++sequence_angle_bins[cell(value.sequence[2])+16*cell(value.sequence[3])];
      area_light+=source(du,dv);joint+=pu*direction[2];
    }
    for(const auto& bins:{area_bins,angle_bins,direct_bins,sequence_angle_bins})for(unsigned count:bins)
      require(count==Samples/256,"BT progressive sample set has an empty or duplicated dyadic cell");
    const double expected_first[3]={0,0,2./3},expected_second[3]={.25,.25,.5};
    for(unsigned c=0;c<3;++c) {
      worst_moment=std::max(worst_moment,std::abs(first[c]/Samples-expected_first[c]));
      worst_moment=std::max(worst_moment,std::abs(second[c]/Samples-expected_second[c]));
    }
    worst_moment=std::max(worst_moment,std::abs(joint/Samples-1./3));
    worst_integral=std::max(worst_integral,std::abs(area_light/Samples-refined)/refined);
  }
  require(worst_moment<.004,"BT area/cosine distribution or cross-dimension correlation is wrong");
  require(worst_integral<.004,"BT area source estimate does not match independent quadrature");
  for(unsigned i=ordinary;i<actual.size();++i)for(const auto& vector:{actual[i].origin,actual[i].direction,actual[i].direct})
    for(float value:vector)require(std::isfinite(value),"BT batch index carry produced invalid sample");
  require(actual[ordinary].origin!=actual[ordinary+3].origin,"BT transport batch carry repeated first sequence");
  require(f.renderer.debug.errors.load()==0,"BT sampling graphics validation errors");
  std::printf("block_transport_sampling=passed hardware=1 production_sampling=1 area_integral=1 cosine_moments=1 dyadic_coverage=1 joint_dimensions=1 batch_carry=1 sequence_readback=1 mapped_rounding_audit=1 samples=%zu worst_moment=%.9g relative_area_error=%.9g rounded_boundaries=%u worst_mapping_error=%.9g validation_errors=0\n",
      actual.size(),worst_moment,worst_integral,rounded_boundaries,worst_mapping);
}
}
