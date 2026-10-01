#include "SpatialTriangleOrder.h"
#include "FilePath.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <queue>
#include <stdexcept>

namespace octaryn::scene_geometry {
namespace {
static_assert(std::endian::native==std::endian::little);
struct Record {std::uint64_t key{},triangle{};std::array<float,6> bounds{};};
static_assert(sizeof(Record)==40);
struct Header {
  char magic[8]{'Z','S','G','T','R','I','O','1'};
  std::uint32_t version{1},mesh{},primitive{},reserved{};
  std::uint64_t triangles{};
  char source_hash[64]{};
};
static_assert(sizeof(Header)==96);
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
bool before(const Record& a,const Record& b) {return a.key<b.key || (a.key==b.key && a.triangle<b.triangle);}
void valid_bounds(const std::array<float,6>& bounds) {
  for(float value:bounds)require(std::isfinite(value),"spatial order bounds are nonfinite");
  for(unsigned axis=0;axis<3;++axis)require(bounds[axis]<=bounds[axis+3],"spatial order bounds are inverted");
}
void validate(const SpatialOrderConfig& config) {
  require(config.source_hash.size()==64 && std::all_of(config.source_hash.begin(),config.source_hash.end(),
      [](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}),"spatial order requires a source digest");
  require(config.triangles && config.triangles<=(1ull<<40) && config.part_triangles && config.part_triangles<=65536 &&
      config.run_triangles && config.run_triangles<=65536 && config.merge_width>=2 && config.merge_width<=32,
      "invalid spatial order limits");
  require(config.triangles/config.part_triangles<1000000,"spatial order part metadata exceeds its bound");
  require((config.triangles-1)/config.run_triangles<65536,"spatial order run metadata exceeds its bound");
  valid_bounds(config.bounds);
}
Header header(const SpatialOrderConfig& config) {
  Header result;result.mesh=config.mesh;result.primitive=config.primitive;result.triangles=config.triangles;
  std::copy(config.source_hash.begin(),config.source_hash.end(),result.source_hash);return result;
}
std::uint64_t morton(const SpatialTriangle& triangle,const SpatialOrderConfig& config) {
  std::array<std::uint32_t,3> quantized{};valid_bounds(triangle.bounds);
  double extent{};for(unsigned axis=0;axis<3;++axis)extent=std::max(extent,double(config.bounds[axis+3])-config.bounds[axis]);
  for(unsigned axis=0;axis<3;++axis) {
    const auto value=triangle.centroid[axis];require(std::isfinite(value),"spatial triangle centroid is nonfinite");
    require(triangle.bounds[axis]>=config.bounds[axis] && triangle.bounds[axis+3]<=config.bounds[axis+3],
        "spatial triangle exceeds source primitive bounds");
    require(value>=triangle.bounds[axis] && value<=triangle.bounds[axis+3],"spatial triangle centroid is outside its bounds");
    const auto fraction=extent>0?(double(value)-config.bounds[axis])/extent:0;
    quantized[axis]=std::uint32_t(std::clamp(fraction,0.,1.)*2097151.);
  }
  std::uint64_t key{};
  for(unsigned bit=0;bit<21;++bit)for(unsigned axis=0;axis<3;++axis)
    key|=std::uint64_t((quantized[axis]>>bit)&1)<<(bit*3+axis);
  return key;
}
struct Run {std::filesystem::path path;std::uint64_t count{};};
struct Work {
  const SpatialOrderConfig& config;
  const std::atomic_bool* cancel;
  SpatialOrderResult result;
  std::filesystem::path directory;
  std::vector<std::filesystem::path> files;
  std::uint64_t bytes{},sequence{};
  Work(const SpatialOrderConfig& settings,const std::filesystem::path& output,const std::atomic_bool* canceled):config(settings),cancel(canceled) {
    static std::atomic_uint64_t id{};auto parent=output.parent_path();if(parent.empty())parent=".";
    std::filesystem::create_directories(content::file_io_path(parent));
    directory=parent/("triangle-order-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(id++));
    require(std::filesystem::create_directory(content::file_io_path(directory)),"cannot create owned triangle order scratch");
  }
  ~Work() {
    std::error_code ignored;for(const auto& file:files)std::filesystem::remove(content::file_io_path(file),ignored);
    std::filesystem::remove(content::file_io_path(directory),ignored);
  }
  void check() const {require(!cancel || !cancel->load(std::memory_order_relaxed),"spatial order canceled");}
  Run create(std::uint64_t count) {
    Run run{directory/(std::to_string(sequence++)+".run"),count};files.push_back(run.path);return run;
  }
  void charge(std::uint64_t count) {
    require(bytes<=config.maximum_scratch_bytes && count<=config.maximum_scratch_bytes-bytes,"spatial order scratch budget exceeded");
    bytes+=count;result.peak_scratch_bytes=std::max(result.peak_scratch_bytes,bytes);
  }
  void remove(const Run& run) {
    require(std::filesystem::remove(content::file_io_path(run.path)),"cannot retire triangle order scratch run");bytes-=run.count*sizeof(Record);
  }
  void record(std::ofstream& file,const Record& value) {
    charge(sizeof(value));file.write(reinterpret_cast<const char*>(&value),sizeof(value));require(file.good(),"cannot write triangle order run");
  }
  Run merge(std::span<const Run> inputs) {
    struct Cursor {std::ifstream file;Record value;std::uint64_t remaining{};};
    std::vector<Cursor> cursors(inputs.size());std::uint64_t count{};
    const auto compare=[&](unsigned a,unsigned b){return before(cursors[b].value,cursors[a].value);};
    std::priority_queue<unsigned,std::vector<unsigned>,decltype(compare)> queue(compare);
    const auto next=[&](unsigned index) {
      auto& cursor=cursors[index];if(!cursor.remaining)return;
      cursor.file.read(reinterpret_cast<char*>(&cursor.value),sizeof(Record));require(cursor.file.good(),"incomplete triangle order run");
      --cursor.remaining;queue.push(index);
    };
    for(unsigned i=0;i<inputs.size();++i) {
      cursors[i].file.open(content::file_io_path(inputs[i].path),std::ios::binary);cursors[i].remaining=inputs[i].count;
      require(cursors[i].file.good(),"cannot open triangle order run");count+=inputs[i].count;next(i);
    }
    const auto output=create(count);std::ofstream file(content::file_io_path(output.path),std::ios::binary);require(file.good(),"cannot create merged triangle order run");
    std::uint64_t emitted{};
    while(!queue.empty()) {
      if(emitted%4096==0)check();const auto index=queue.top();queue.pop();record(file,cursors[index].value);++emitted;next(index);
    }
    require(emitted==count,"triangle order merge lost source coverage");file.close();require(file.good(),"cannot finish merged triangle order run");
    for(auto& cursor:cursors)cursor.file.close();for(const auto& input:inputs)remove(input);return output;
  }
};
}
bool spatial_triangle_order_path(const std::filesystem::path& catalog,const std::filesystem::path& relative,
    std::filesystem::path& output,std::string& error) {
  try {
    require(!relative.empty() && relative.is_relative() && !relative.has_root_name(),"spatial order path must be relative");
    for(const auto& component:relative)require(component!="..","spatial order path escapes catalog");
    const auto parent=content::canonical_file_path(std::filesystem::absolute(catalog).parent_path());
    const auto file=content::canonical_file_path(parent/relative);
    auto first=file.begin();for(auto expected=parent.begin();expected!=parent.end();++expected,++first)
      require(first!=file.end() && *first==*expected,"spatial order path resolves outside catalog");
    require(first!=file.end(),"spatial order path points to catalog directory");output=file;error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool write_spatial_triangle_order(const std::filesystem::path& output,const SpatialOrderConfig& config,const SpatialTriangleRead& read,
    SpatialOrderResult& result,std::string& error,const std::atomic_bool* cancel) {
  result={};
  try {
    validate(config);require(bool(read),"spatial order reader is missing");require(!std::filesystem::exists(content::file_io_path(output)),"spatial order output already exists");
    Work work(config,output,cancel);std::vector<Run> runs;
    for(std::uint64_t first=0;first<config.triangles;) {
      work.check();const auto count=std::uint32_t(std::min<std::uint64_t>(config.run_triangles,config.triangles-first));
      std::vector<SpatialTriangle> triangles(count);
      if(!read(first,triangles,error))throw std::runtime_error(error.empty()?"spatial source read failed":error);
      work.check();
      std::vector<Record> records(count);
      for(unsigned i=0;i<count;++i)records[i]={morton(triangles[i],config),first+i,triangles[i].bounds};
      work.result.peak_records=std::max<std::uint64_t>(work.result.peak_records,count);
      std::sort(records.begin(),records.end(),before);const auto run=work.create(count);std::ofstream file(content::file_io_path(run.path),std::ios::binary);
      require(file.good(),"cannot create triangle order run");for(const auto& record:records)work.record(file,record);
      file.close();require(file.good(),"cannot finish triangle order run");runs.push_back(run);first+=count;
    }
    while(runs.size()>1) {
      std::vector<Run> merged;
      for(std::size_t first=0;first<runs.size();) {
        const auto count=std::min<std::size_t>(config.merge_width,runs.size()-first);
        merged.push_back(count==1?runs[first]:work.merge(std::span(runs).subspan(first,count)));first+=count;
      }
      runs=std::move(merged);++work.result.merge_passes;
    }
    work.check();const auto staged=work.create(0);std::ofstream file(content::file_io_path(staged.path),std::ios::binary);const auto signature=header(config);
    work.charge(sizeof(signature));file.write(reinterpret_cast<const char*>(&signature),sizeof(signature));
    std::ifstream sorted(content::file_io_path(runs.front().path),std::ios::binary);Record previous{};bool has_previous=false;
    for(std::uint64_t index=0;index<config.triangles;++index) {
      if(index%4096==0)work.check();Record record;sorted.read(reinterpret_cast<char*>(&record),sizeof(record));require(sorted.good(),"incomplete final triangle order");
      require(record.triangle<config.triangles && (!has_previous || before(previous,record)),"triangle order is duplicated or unsorted");
      previous=record;has_previous=true;
      if(index%config.part_triangles==0)work.result.parts.push_back({index,0,record.bounds});
      auto& part=work.result.parts.back();++part.triangle_count;
      for(unsigned axis=0;axis<3;++axis) {part.bounds[axis]=std::min(part.bounds[axis],record.bounds[axis]);part.bounds[axis+3]=std::max(part.bounds[axis+3],record.bounds[axis+3]);}
      work.charge(sizeof(record.triangle));file.write(reinterpret_cast<const char*>(&record.triangle),sizeof(record.triangle));
    }
    sorted.close();file.close();require(file.good(),"cannot finish spatial triangle permutation");work.check();
    // Atomic link publication fails if any other writer claimed this output.
    std::filesystem::create_hard_link(content::file_io_path(staged.path),content::file_io_path(output));
    work.result.triangles=config.triangles;result=std::move(work.result);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool read_spatial_triangle_order(const std::filesystem::path& path,const SpatialOrderConfig& config,std::uint64_t first,
    std::uint32_t count,std::vector<std::uint64_t>& result,std::string& error) {
  result.clear();
  try {
    validate(config);require(count && count<=65536 && first<=config.triangles && count<=config.triangles-first,"invalid spatial triangle range");
    require(std::filesystem::file_size(content::file_io_path(path))==sizeof(Header)+config.triangles*sizeof(std::uint64_t),"spatial triangle permutation size mismatch");
    std::ifstream file(content::file_io_path(path),std::ios::binary);Header actual;file.read(reinterpret_cast<char*>(&actual),sizeof(actual));const auto expected=header(config);
    require(file.good() && std::memcmp(&actual,&expected,sizeof(actual))==0,"spatial triangle source identity mismatch");
    file.seekg(std::streamoff(sizeof(Header)+first*sizeof(std::uint64_t)));std::vector<std::uint64_t> next(count);
    file.read(reinterpret_cast<char*>(next.data()),std::streamsize(next.size()*sizeof(std::uint64_t)));require(file.good(),"incomplete spatial triangle range");
    for(auto triangle:next)require(triangle<config.triangles,"spatial triangle index is outside source");result=std::move(next);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool validate_spatial_triangle_order(const std::filesystem::path& path,const SpatialOrderConfig& config,std::string& error,
    const std::atomic_bool* cancel) {
  try {
    validate(config);const auto words=(config.triangles+63)/64;
    require(words<=((64ull<<20)/sizeof(std::uint64_t)),"spatial order coverage bitmap exceeds 64MiB");
    std::vector<std::uint64_t> seen(words),triangles;
    for(std::uint64_t first=0;first<config.triangles;) {
      require(!cancel || !cancel->load(std::memory_order_relaxed),"spatial order validation canceled");
      const auto count=std::uint32_t(std::min<std::uint64_t>(65536,config.triangles-first));
      if(!read_spatial_triangle_order(path,config,first,count,triangles,error))return false;
      for(auto id:triangles) {
        const auto mask=std::uint64_t(1)<<(id%64);require(!(seen[id/64]&mask),"spatial order repeats a source triangle");seen[id/64]|=mask;
      }
      first+=count;
    }
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
