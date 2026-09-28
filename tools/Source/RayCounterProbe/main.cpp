#include "ShaderCache.h"
#include "RayDiagnosticMode.h"
#include "RayDiagnosticProfile.h"
#include "ReflectionWaveMode.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <array>
using namespace octaryn::client::rendering;
namespace {
void require(bool value,const char* message) {
  if(!value) {std::fprintf(stderr,"ray_counter_probe failed: %s\n",message);std::exit(1);}
}
bool equal(ISlangBlob* a,ISlangBlob* b) {
  return a && b && a->getBufferSize()==b->getBufferSize() &&
    !std::memcmp(a->getBufferPointer(),b->getBufferPointer(),a->getBufferSize());
}
struct Compiled {Slang::ComPtr<ISlangBlob> key,code;};
Compiled compile(slang::IGlobalSession* global,const std::filesystem::path& source,
    const std::filesystem::path& diagnostic,bool enabled,SlangCompileTarget format,unsigned wave=0,bool telemetry=false,bool map_only=true) {
  const auto directory=diagnostic.string(),hdr=(diagnostic.parent_path()/"Hdr").string();
  const char* search[]{directory.c_str(),hdr.c_str()};
  slang::TargetDesc target{};target.format=format;
  target.profile=global->findProfile(format==SLANG_DXIL?"sm_6_8":"spirv_1_5");
  if(format==SLANG_SPIRV)target.flags=SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  const auto wave_text=std::to_string(wave);
  const slang::PreprocessorMacroDesc macros[]{
    {"OCTARYN_RAY_COUNTERS",enabled?"1":"0"},{"OCTARYN_REFLECTION_WAVE_SIZE",wave_text.c_str()},
    {"OCTARYN_RAY_WAVE_TELEMETRY",telemetry?"1":"0"},{"OCTARYN_MAP_ONLY_RAYS",map_only?"1":"0"}};
  slang::SessionDesc desc{};desc.searchPaths=search;desc.searchPathCount=2;
  desc.targets=&target;desc.targetCount=1;desc.preprocessorMacros=macros;desc.preprocessorMacroCount=4;
  Slang::ComPtr<slang::ISession> session;
  require(SLANG_SUCCEEDED(global->createSession(desc,session.writeRef())),"session");
  Slang::ComPtr<ISlangBlob> diagnostics;auto* module=session->loadModule(source.string().c_str(),diagnostics.writeRef());
  if(diagnostics)std::fwrite(diagnostics->getBufferPointer(),1,diagnostics->getBufferSize(),stderr);
  require(module!=nullptr,"production diagnostics include");
  Slang::ComPtr<slang::IEntryPoint> entry;require(SLANG_SUCCEEDED(module->findEntryPointByName("main",entry.writeRef())),"entry");
  slang::IComponentType* components[]{module,entry};Slang::ComPtr<slang::IComponentType> composite,linked;
  require(SLANG_SUCCEEDED(session->createCompositeComponentType(components,2,composite.writeRef(),diagnostics.writeRef())),"composite");
  require(SLANG_SUCCEEDED(composite->link(linked.writeRef(),diagnostics.writeRef())),"link");
  auto* layout=linked->getLayout(0,diagnostics.writeRef());require(layout!=nullptr,"layout");
  SlangUInt actual_wave{};layout->getEntryPointByIndex(0)->getComputeWaveSize(&actual_wave);
  require(actual_wave==(map_only?wave:0),"reflection reports only requested map entry wave width");
  bool counter=false,flag=false;
  for(unsigned i=0;i<layout->getParameterCount();++i) {
    const char* name=layout->getParameterByIndex(i)->getName();
    counter=counter || (name && !std::strcmp(name,"rayDiagnosticCounters"));
    flag=flag || (name && !std::strcmp(name,"rayDiagnosticsEnabled"));
  }
  require(counter==enabled && flag==enabled,"counter and runtime flag reflection match compiled variant");
  Compiled result;linked->getEntryPointHash(0,0,result.key.writeRef());
  require(result.key && result.key->getBufferSize(),"entry hash");
  const auto status=linked->getEntryPointCode(0,0,result.code.writeRef(),diagnostics.writeRef());
  if(diagnostics)std::fwrite(diagnostics->getBufferPointer(),1,diagnostics->getBufferSize(),stderr);
  require(SLANG_SUCCEEDED(status) && result.code && result.code->getBufferSize(),"target code");
  return result;
}
void mode_cases() {
  ReflectionWaveMode wave;
  for(const auto* option:{"", "auto", "16", "32x", "-1"})
    require(!wave.configure(option,rhi::DeviceType::D3D12,false),"invalid wave controls rejected");
  require(!wave.configure("32",rhi::DeviceType::Vulkan,false),"Vulkan forced wave rejected");
  require(wave.configure("64",rhi::DeviceType::D3D12,false),"DX12 wave selected");
  require(wave.supported(true,32,64) && !wave.supported(true,32,32) && !wave.supported(false,32,64),"actual capability range required");
  require(wave.path(true,true,false,true,true,1024) && !wave.path(true,true,true,true,true,1024) &&
    !wave.path(false,true,false,true,true,1024) && !wave.path(true,true,false,false,true,1024) &&
    !wave.path(true,true,false,true,true,0),"forced wave cannot silently miss eligible path");
  require(wave.configure(nullptr,rhi::DeviceType::Vulkan,false) && wave.supported(false,0,0),"quiet default needs no WaveOps");
  require(wave.configure("0",rhi::DeviceType::Vulkan,true) && !wave.supported(false,0,0),"collector requires wave telemetry feature");
  RayDiagnosticMode mode;
  require(resolve_ray_diagnostic_mode(nullptr,nullptr,mode,false) && !mode.compiled && !mode.collecting,"qualified production auto");
  require(resolve_ray_diagnostic_mode(nullptr,nullptr,mode) && mode.compiled && !mode.collecting,"unqualified backend legacy auto");
  require(resolve_ray_diagnostic_mode("1",nullptr,mode) && mode.compiled && !mode.collecting,"quiet runtime-disabled reference");
  std::string path="counter.csv";
  require(resolve_ray_diagnostic_mode(nullptr,path.c_str(),mode) && mode.compiled && mode.collecting,"collector auto");
  path="changed.csv";require(mode.path=="counter.csv","collection path is owned/frozen");
  require(!resolve_ray_diagnostic_mode("0","counter.csv",mode),"collector compile-off mismatch rejected");
  for(const auto* value:{"auto","true","2","-1","1garbage"})
    require(!resolve_ray_diagnostic_mode(value,nullptr,mode),"strict control rejected");
  require(std::strcmp(ray_counter_cache_variant(false),ray_counter_cache_variant(true))!=0,"cache namespace separation");
  RayDiagnosticProfile production;
  require(production.configure("0",nullptr),"configure production profile");
  require(!production.configure("1",nullptr),"profile cannot change mode after device setup");
  require(production.begin(nullptr,nullptr,0,0) && production.begin(nullptr,nullptr,1,1),
    "compiled-off profile performs no GPU allocation or recording");
  production.end(nullptr);require(production.drain(nullptr),"compiled-off profile has no GPU readback");
}
}
int main(int argc,char** argv) {
  require(argc==3,"fresh output directory and repository arguments");mode_cases();
  const std::filesystem::path output=argv[1],repository=argv[2];
  require(!std::filesystem::exists(output),"fresh fixture path");std::filesystem::create_directories(output);
  const auto source=output/"CounterProbe.slang";
  {std::ofstream file(source);file<<"#include \"Diagnostics.slang\"\nRWStructuredBuffer<uint> result;\n"
    "[shader(\"compute\")][numthreads(8,8,1)]\n#include \"ReflectionWaveAttributes.slang\"\n"
    "void main(uint3 id:SV_DispatchThreadID) {"
    "ray_diagnostic_wave();ray_diagnostic(17);ray_diagnostic_max(13,id.x);result[id.x]=id.x+1;}\n";}
  Slang::ComPtr<slang::IGlobalSession> global;
  require(SLANG_SUCCEEDED(slang::createGlobalSession(global.writeRef())),"global session");
  for(const auto target:{SLANG_DXIL,SLANG_SPIRV}) {
    const auto directory=output/(target==SLANG_DXIL?"dxil":"spirv");
    std::array<Compiled,2> modes;
    for(unsigned index=0;index<2;++index)
      modes[index]=compile(global,source,repository/"octaryn-client/Shaders/RayTracing",index!=0,target);
    require(!equal(modes[0].key,modes[1].key),"macro changes actual Slang cache key");
    require(!equal(modes[0].code,modes[1].code),"macro changes compiled code");
    // Same cache directory deliberately tests SDK-key separation independent of namespace isolation.
    for(unsigned index=0;index<2;++index) {
      auto cache=create_shader_cache(directory);require(bool(cache),"cache");
      require(SLANG_SUCCEEDED(cache->writeCache(modes[index].key,modes[index].code)),"write target variant");
    }
    for(unsigned index:{1u,0u,1u,0u}) {
      const auto repeated=compile(global,source,repository/"octaryn-client/Shaders/RayTracing",index!=0,target);
      require(equal(repeated.key,modes[index].key),"same variant stable across fresh session");
      auto cache=create_shader_cache(directory);Slang::ComPtr<ISlangBlob> found;
      require(SLANG_SUCCEEDED(cache->queryCache(repeated.key,found.writeRef())) && equal(found,modes[index].code),
        "alternating persistent lookup returns correct variant");
    }
    const auto observed=compile(global,source,repository/"octaryn-client/Shaders/RayTracing",true,target,0,true);
    require(!equal(observed.key,modes[1].key) && !equal(observed.code,modes[1].code),"collector wave observation separate from quiet shaders");
    if(target==SLANG_DXIL) {
      const auto shaders=repository/"octaryn-client/Shaders/RayTracing";
      std::array<Compiled,3> waves{modes[0],compile(global,source,shaders,false,target,32),compile(global,source,shaders,false,target,64)};
      require(!equal(waves[0].key,waves[1].key) && !equal(waves[1].key,waves[2].key),"wave macro changes SDK key");
      auto cache=create_shader_cache(directory);
      for(auto& variant:waves)require(SLANG_SUCCEEDED(cache->writeCache(variant.key,variant.code)),"store wave variant");
      for(unsigned index:{2u,0u,1u,2u,1u,0u}) {
        const auto repeated=compile(global,source,shaders,false,target,index==0?0:index==1?32:64);
        Slang::ComPtr<ISlangBlob> found;
        require(SLANG_SUCCEEDED(cache->queryCache(repeated.key,found.writeRef())) && equal(found,waves[index].code),"alternating wave cache exact binary");
      }
      compile(global,source,shaders,false,target,64,false,false);
    }
  }
  std::puts("PASS ray counter/wave variants: production declarations absent; quiet uniforms retained; collector-only wave code; DXIL wave0/32/64 reflection; generic width unchanged; SDK hashes and alternating cache correct; strict capability/path controls; gpu_runtime=0");
}
