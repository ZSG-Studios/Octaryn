#include "GeometryCache.h"
#include "GeometryPageCodec.h"
#include "FilePath.h"
#include "../MapWorld/MapTextureCache.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <random>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace octaryn::client::rendering::virtual_geometry {
namespace {
static_assert(std::endian::native==std::endian::little);
constexpr std::uint64_t maximum_cache_bytes=1024ull*1024*1024;
struct Header {
  std::uint32_t magic{0x4743564fu},version{geometry_version},flags{1},materials{};
  std::uint32_t clusters{},groups{},pages{},dependencies{},roots{},reserved{};
  std::uint64_t source_triangles{};
  std::array<char,64> source_hash{},metadata_hash{};
};
struct PageRecord {
  std::uint64_t offset{};
  std::uint32_t encoded{},decoded{},codec{},reserved{};
  std::array<char,64> checksum{};
};
static_assert(sizeof(Header)==176 && sizeof(PageRecord)==88);
void check(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
template<class T> void append(std::vector<std::uint8_t>& output,std::span<const T> values) {
  const auto bytes=std::as_bytes(values);const auto old=output.size();output.resize(old+bytes.size());
  std::memcpy(output.data()+old,bytes.data(),bytes.size());
}
template<class T> void extract(std::span<const std::uint8_t>& source,std::vector<T>& output,size_t count) {
  check(count<=source.size()/sizeof(T),"geometry metadata truncated");output.resize(count);
  std::memcpy(output.data(),source.data(),count*sizeof(T));source=source.subspan(count*sizeof(T));
}
std::uint64_t metadata_bytes(const Header& h) {
  return sizeof(Header)+std::uint64_t(h.clusters)*sizeof(GeometryCluster)+std::uint64_t(h.groups)*sizeof(GeometryGroup)+
      std::uint64_t(h.dependencies)*4+std::uint64_t(h.roots)*4+std::uint64_t(h.pages)*sizeof(PageRecord);
}
}
bool write_geometry_cache(const std::filesystem::path& path,const GeometryAsset& asset,std::string& error) {
  try {
    if(!validate_geometry(asset,error))return false;
    check(asset.payloads.size()==asset.pages.size(),"geometry write requires resident page payloads");
    Header header;header.flags=unsigned(asset.space);header.materials=asset.material_count;header.source_triangles=asset.source_triangles;
    header.clusters=unsigned(asset.clusters.size());header.groups=unsigned(asset.groups.size());header.pages=unsigned(asset.pages.size());
    header.dependencies=unsigned(asset.group_pages.size());header.roots=unsigned(asset.roots.size());
    std::copy(asset.source_hash.begin(),asset.source_hash.end(),header.source_hash.begin());
    std::uint64_t offset=metadata_bytes(header);std::vector<PageRecord> records;
    for(const auto& page:asset.pages) {
      records.push_back({offset,page.encoded_size,page.decoded_size,unsigned(page.codec),0,page.checksum});offset+=page.encoded_size;
    }
    check(offset<=maximum_cache_bytes,"geometry cache exceeds one GiB format safety limit");
    std::vector<std::uint8_t> metadata;metadata.reserve(size_t(metadata_bytes(header)));
    append(metadata,std::span<const Header>(&header,1));append(metadata,std::span<const GeometryCluster>(asset.clusters));
    append(metadata,std::span<const GeometryGroup>(asset.groups));append(metadata,std::span<const unsigned>(asset.group_pages));
    append(metadata,std::span<const unsigned>(asset.roots));append(metadata,std::span<const PageRecord>(records));
    const auto hash=map_texture_digest(metadata);std::copy(hash.begin(),hash.end(),header.metadata_hash.begin());
    std::memcpy(metadata.data(),&header,sizeof(header));
    if(!path.parent_path().empty())std::filesystem::create_directories(content::file_io_path(path.parent_path()));
    static std::atomic<std::uint64_t> sequence{};
    auto temporary=path;
    temporary+=".tmp-"+std::to_string(std::random_device{}())+"-"+std::to_string(++sequence);
    struct Temporary {
      std::filesystem::path path;
      ~Temporary() {std::error_code error;std::filesystem::remove(path,error);}
    } cleanup{content::file_io_path(temporary)};
    std::ofstream file(content::file_io_path(temporary),std::ios::binary|std::ios::trunc);check(bool(file),"geometry cache open failed");
    file.write(reinterpret_cast<const char*>(metadata.data()),std::streamsize(metadata.size()));
    for(const auto& payload:asset.payloads)file.write(reinterpret_cast<const char*>(payload.data()),std::streamsize(payload.size()));
    file.close();check(bool(file),"geometry cache write failed");
#ifdef _WIN32
    check(MoveFileExW(content::file_io_path(temporary).c_str(),content::file_io_path(path).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,
        "geometry cache atomic replacement failed");
#else
    std::filesystem::rename(temporary,path);
#endif
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool read_geometry_cache(const std::filesystem::path& path,const std::string& source_hash,
    GeometryAsset& output,std::string& error,bool load_payloads) {
  try {
    const auto size=std::filesystem::file_size(content::file_io_path(path));
    check(size>=sizeof(Header) && size<=maximum_cache_bytes,"geometry cache size invalid");
    std::ifstream file(content::file_io_path(path),std::ios::binary);Header header;
    check(bool(file.read(reinterpret_cast<char*>(&header),sizeof(header))),"geometry cache header truncated");
    check(header.magic==0x4743564fu && header.version==geometry_version && (header.flags==1 || header.flags==2) && header.reserved==0,
        "geometry cache version or flags invalid");
    check(std::string(header.source_hash.data(),64)==source_hash,"geometry cache source mismatch");
    const auto metadata_size=metadata_bytes(header);check(metadata_size<=size,"geometry cache counts exceed file");
    const auto expected_hash=header.metadata_hash;header.metadata_hash={};
    std::vector<std::uint8_t> metadata(size_t(metadata_size),0);std::memcpy(metadata.data(),&header,sizeof(header));
    check(bool(file.read(reinterpret_cast<char*>(metadata.data()+sizeof(header)),std::streamsize(metadata.size()-sizeof(header)))),
        "geometry cache metadata truncated");
    check(map_texture_digest(metadata)==std::string(expected_hash.data(),64),"geometry cache metadata checksum mismatch");
    GeometryAsset asset;asset.space=GeometrySpace(header.flags);asset.source_hash=source_hash;
    asset.material_count=header.materials;asset.source_triangles=header.source_triangles;
    std::span<const std::uint8_t> view(metadata);view=view.subspan(sizeof(header));
    extract(view,asset.clusters,header.clusters);extract(view,asset.groups,header.groups);
    extract(view,asset.group_pages,header.dependencies);extract(view,asset.roots,header.roots);
    std::vector<PageRecord> records;extract(view,records,header.pages);check(view.empty(),"geometry metadata excess data");
    std::uint64_t offset=metadata_size;
    for(const auto& record:records) {
      check(record.offset==offset && record.reserved==0 && record.encoded<=size-offset,"geometry page file range invalid");
      asset.pages.push_back({record.offset,record.encoded,record.decoded,GeometryCodec(record.codec),record.checksum});
      offset+=record.encoded;
    }
    check(offset==size,"geometry cache payload size mismatch");
    if(!validate_geometry(asset,error))return false;
    if(load_payloads)for(const auto& page:asset.pages) {
      auto& encoded=asset.payloads.emplace_back(page.encoded_size);
      check(bool(file.read(reinterpret_cast<char*>(encoded.data()),encoded.size())),"geometry page payload truncated");
    }
    if(load_payloads && !validate_geometry(asset,error))return false;
    output=std::move(asset);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool read_geometry_page(const std::filesystem::path& path,const GeometryPage& page,
    std::vector<std::uint8_t>& decoded,std::string& error) {
  decoded.clear();
  try {
    const auto size=std::filesystem::file_size(content::file_io_path(path));
    check(page.encoded_size && page.encoded_size<=page_bytes && page.file_offset<=size && page.encoded_size<=size-page.file_offset,
        "geometry page file range invalid");
    std::ifstream file(content::file_io_path(path),std::ios::binary);file.seekg(std::streamoff(page.file_offset));
    std::vector<std::uint8_t> encoded(page.encoded_size);
    check(bool(file.read(reinterpret_cast<char*>(encoded.data()),encoded.size())),"geometry page payload truncated");
    return decode_geometry_payload(page,encoded,decoded,error);
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
