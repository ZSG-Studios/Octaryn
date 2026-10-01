#include "GpuCounterState.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {std::atomic_flag active=ATOMIC_FLAG_INIT;}
std::string counter_json_string(const char* value) {
  std::string result="\"";
  for(const char character:std::string(value?value:"")) {
    const auto c=static_cast<unsigned char>(character);
    if(c=='"' || c=='\\') {result+='\\';result+=char(c);}
    else if(c<32) {const char hex[]="0123456789abcdef";result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
    else result+=char(c);
  }
  return result+'"';
}
bool GpuCounterProfile::State::check(GpaStatus status,const char* operation) {
  if(status==kGpaStatusOk)return true;
  fail(operation,status);return false;
}
bool GpuCounterProfile::State::flush_report() {
  report.flush();
  if(report)return true;
  fail("report_write_failed");
  std::fputs("profile_writer_failed capture_invalid=1 owner=gpu_counters\n",stderr);
  return false;
}
void GpuCounterProfile::State::fail(const char* operation,GpaStatus status) {
  if(phase==CounterPhase::Failed)return;
  phase=CounterPhase::Failed;
  report<<"{\"event\":\"unsupported_or_failed\",\"operation\":"<<counter_json_string(operation)
      <<",\"status\":"<<int(status)<<",\"description\":"
      <<counter_json_string(api.GpaGetStatusAsStr?api.GpaGetStatusAsStr(status):"API unavailable")<<"}\n";
  report.flush();
  std::fprintf(stderr,"gpu_counter_profile status=unsupported_or_failed operation=%s code=%d\n",operation,int(status));
}
void GpuCounterProfile::State::initialize(bool dx12) {
  const auto path=std::filesystem::u8path(std::getenv("OCTARYN_CLIENT_GPU_COUNTERS_PATH"));
  if(std::filesystem::exists(path))throw std::runtime_error("GPU counter output already exists; refusing to overwrite");
  report.open(path,std::ios::out|std::ios::binary);
  if(!report)throw std::runtime_error("GPU counter report could not be opened");
  report<<"{\"event\":\"configuration\",\"schema\":1,\"diagnostic_only\":true,\"clock_mode\":\"none\","
      "\"scope\":\"reflection_group_excludes_composition\",\"max_counters\":8,\"max_passes\":1,\"occupancy_supported\":false}\n";
  if(!dx12) {fail("requires_dx12");return;}
  if(const char* text=std::getenv("OCTARYN_CLIENT_GPU_COUNTERS_FRAME")) {
    const auto end=text+std::char_traits<char>::length(text);
    const auto parsed=std::from_chars(text,end,target);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || target>1000000) {fail("invalid_target_frame");return;}
  }
  const char* names=std::getenv("OCTARYN_CLIENT_GPU_COUNTERS_NAMES");
  explicit_names=names!=nullptr;
  std::istringstream list(names?names:"CSBusy,CSWavefrontsLaunched");
  for(std::string name;std::getline(list,name,',');) {
    if(name.empty() || requested.size()==8 || std::find(requested.begin(),requested.end(),name)!=requested.end()) {
      fail("invalid_counter_list");return;
    }
    requested.push_back(name);
  }
  if(requested.empty()) {fail("empty_counter_list");return;}
  if(active.test_and_set()) {fail("another_counter_context_active");return;}
  owns_global=true;
  const char* configured=std::getenv("OCTARYN_CLIENT_GPU_COUNTERS_DLL");
  const auto dll=std::filesystem::u8path(configured?configured:OCTARYN_GPA_DLL);
  if(!dll.is_absolute()) {fail("dll_requires_absolute_path");return;}
  library=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if(!library) {fail("dll_load");return;}
  const auto table=reinterpret_cast<GpaGetFuncTablePtrType>(GetProcAddress(static_cast<HMODULE>(library),"GpaGetFuncTable"));
  if(!table || !check(table(&api),"function_table")) {fail("function_table_missing");return;}
  GpaUInt32 major{},minor{},build{},update{};
  if(!check(api.GpaGetVersion(&major,&minor,&build,&update),"version"))return;
  report<<"{\"event\":\"sdk\",\"dll\":"<<counter_json_string(dll.string().c_str())<<",\"version\":["
      <<major<<','<<minor<<','<<build<<','<<update<<"],\"version_order\":\"major,minor,build,update\",\"release_version\":\""
      <<major<<'.'<<minor<<'.'<<update<<'.'<<build<<"\",\"requested_frame\":"<<target<<"}\n";
  if(major!=OCTARYN_GPA_MAJOR || minor!=OCTARYN_GPA_MINOR || build!=OCTARYN_GPA_BUILD || update!=OCTARYN_GPA_UPDATE) {
    fail("sdk_version_mismatch");return;
  }
  initialized=check(api.GpaInitialize(kGpaInitializeDefaultBit),"initialize_before_device");
}
void GpuCounterProfile::State::attach(rhi::IDevice* device) {
  if(phase==CounterPhase::Failed)return;
  rhi::DeviceNativeHandles handles{};
  if(SLANG_FAILED(device->getNativeDeviceHandles(&handles)) || handles.handles[0].type!=rhi::NativeHandleType::D3D12Device) {
    fail("native_dx12_device_unavailable");return;
  }
  if(!check(api.GpaOpenContext(reinterpret_cast<void*>(handles.handles[0].value),kGpaOpenContextClockModeNoneBit,&context),"open_context"))return;
  GpaContextSampleTypeFlags types{};
  if(!check(api.GpaGetSupportedSampleTypes(context,&types),"sample_types"))return;
  if(!(types&kGpaContextSampleTypeDiscreteCounter)) {fail("discrete_counters_unsupported");return;}
  const char* name{};GpaUInt32 device_id{},revision{};
  if(!check(api.GpaGetDeviceName(context,&name),"device_name") ||
      !check(api.GpaGetDeviceAndRevisionId(context,&device_id,&revision),"device_id"))return;
  report<<"{\"event\":\"device\",\"name\":"<<counter_json_string(name)<<",\"device_id\":"<<device_id
      <<",\"revision\":"<<revision<<"}\n";
  if(!check(api.GpaCreateSession(context,kGpaSessionSampleTypeDiscreteCounter,&session),"create_session"))return;
  enumerate();
}
void GpuCounterProfile::State::enumerate() {
  GpaUInt32 count{};
  if(!check(api.GpaGetNumCounters(session,&count),"counter_count"))return;
  if(count>8192) {fail("counter_inventory_limit");return;}
  std::vector<Counter> available;
  for(GpaUInt32 index=0;index<count;++index) {
    const char *name{},*description{},*group{},*type_name{},*usage_name{};
    GpaDataType type{};GpaUsageType usage{};GpaCounterSampleType sample{};GpaUuid uuid{};
    if(!check(api.GpaGetCounterName(session,index,&name),"counter_name") ||
        !check(api.GpaGetCounterDescription(session,index,&description),"counter_description") ||
        !check(api.GpaGetCounterGroup(session,index,&group),"counter_group") ||
        !check(api.GpaGetCounterDataType(session,index,&type),"counter_type") ||
        !check(api.GpaGetCounterUsageType(session,index,&usage),"counter_usage") ||
        !check(api.GpaGetCounterSampleType(session,index,&sample),"counter_sample_type") ||
        !check(api.GpaGetCounterUuid(session,index,&uuid),"counter_uuid") ||
        !check(api.GpaGetDataTypeAsStr(type,&type_name),"type_name") ||
        !check(api.GpaGetUsageTypeAsStr(usage,&usage_name),"usage_name"))return;
    char uuid_text[37]{};
    std::snprintf(uuid_text,sizeof(uuid_text),"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid.Data1,uuid.Data2,uuid.Data3,uuid.Data4[0],uuid.Data4[1],uuid.Data4[2],uuid.Data4[3],
        uuid.Data4[4],uuid.Data4[5],uuid.Data4[6],uuid.Data4[7]);
    report<<"{\"event\":\"counter\",\"index\":"<<index<<",\"name\":"<<counter_json_string(name)
        <<",\"description\":"<<counter_json_string(description)<<",\"group\":"<<counter_json_string(group)
        <<",\"type\":"<<counter_json_string(type_name)<<",\"usage\":"<<counter_json_string(usage_name)
        <<",\"uuid\":"<<counter_json_string(uuid_text)<<",\"sample_type\":"<<int(sample)<<"}\n";
    if(sample==kGpaCounterSampleTypeDiscrete)available.push_back({index,name,type});
  }
  for(const auto& name:requested) {
    const auto found=std::find_if(available.begin(),available.end(),[&](const Counter& counter){return counter.name==name;});
    if(found==available.end()) {
      if(explicit_names) {fail(("requested_counter_unavailable:"+name).c_str());return;}
      report<<"{\"event\":\"excluded\",\"name\":"<<counter_json_string(name.c_str())<<",\"reason\":\"not_available_discrete\"}\n";
      continue;
    }
    if(!check(api.GpaEnableCounter(session,found->index),"enable_counter"))return;
    if(!explicit_names) {
      GpaUInt32 candidate_passes{};
      if(!check(api.GpaGetPassCount(session,&candidate_passes),"candidate_pass_count"))return;
      if(candidate_passes!=1) {
        if(!check(api.GpaDisableCounter(session,found->index),"exclude_multipass_counter"))return;
        report<<"{\"event\":\"excluded\",\"name\":"<<counter_json_string(name.c_str())
            <<",\"reason\":\"would_require_multipass\",\"candidate_passes\":"<<candidate_passes<<"}\n";
      }
    }
  }
  GpaUInt32 passes{};
  if(!check(api.GpaGetPassCount(session,&passes),"pass_count"))return;
  if(passes!=1) {fail("requires_exactly_one_pass");return;}
  // Result order is SDK-enabled order, which need not match the requested list.
  GpaUInt32 enabled_count{};
  if(!check(api.GpaGetNumEnabledCounters(session,&enabled_count),"enabled_count"))return;
  if(!enabled_count || enabled_count>8 || (explicit_names && enabled_count!=requested.size())) {fail("enabled_counter_count_mismatch");return;}
  report<<"{\"event\":\"selection\",\"passes\":"<<passes<<",\"counters\":"<<enabled_count
      <<",\"explicit_names\":"<<(explicit_names?"true":"false")<<"}\n";
  for(GpaUInt32 ordinal=0;ordinal<enabled_count;++ordinal) {
    GpaUInt32 index{};
    if(!check(api.GpaGetEnabledIndex(session,ordinal,&index),"enabled_index"))return;
    const auto found=std::find_if(available.begin(),available.end(),[&](const Counter& counter){return counter.index==index;});
    if(found==available.end() || (found->type!=kGpaDataTypeUint64 && found->type!=kGpaDataTypeFloat64)) {
      fail("enabled_counter_type");return;
    }
    enabled.push_back(*found);
    report<<"{\"event\":\"enabled\",\"ordinal\":"<<ordinal<<",\"index\":"<<index
        <<",\"name\":"<<counter_json_string(found->name.c_str())<<"}\n";
  }
  if(!flush_report())return;
  phase=CounterPhase::Armed;
  std::printf("gpu_counter_profile status=armed counters=%zu passes=1 clock_mode=none frame=%llu\n",
      enabled.size(),static_cast<unsigned long long>(target));
}
void GpuCounterProfile::State::close() {
  if(closed)return;
  closed=true;
  if(phase!=CounterPhase::Complete && phase!=CounterPhase::Failed)fail("sample_not_completed_before_shutdown");
  // Queue drain precedes this method; no live GPU sample is destroyed here.
  if(session)check(api.GpaDeleteSession(session),"delete_session");
  if(context)check(api.GpaCloseContext(context),"close_context");
  if(initialized)check(api.GpaDestroy(),"destroy");
  fence.setNull();flush_report();report.close();
  if(report.fail())std::fputs("profile_writer_failed capture_invalid=1 owner=gpu_counters_close\n",stderr);
  if(library)FreeLibrary(static_cast<HMODULE>(library));
  library=nullptr;
  if(owns_global)active.clear();
}
}
#endif
