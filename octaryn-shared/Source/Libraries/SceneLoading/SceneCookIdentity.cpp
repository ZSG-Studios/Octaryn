#include "ScenePreparation.h"
#include "ResourceDigest.h"
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <span>
namespace octaryn::scene_loading {
namespace {
struct Header {
    std::uint32_t magic,version,flags,materials,clusters,groups,pages,dependencies,roots,reserved;
    std::uint64_t source_triangles;std::array<char,64> source_hash,metadata_hash;
};
struct Page {std::uint64_t offset;std::uint32_t encoded,decoded,codec,reserved;std::array<char,64> checksum;};
static_assert(sizeof(Header)==176 && sizeof(Page)==88 && std::endian::native==std::endian::little);
}
void cook_identity(const Preparation& work,const std::filesystem::path& path,const std::string& hash,
                   std::uint64_t triangles,std::uint32_t clusters,std::uint32_t pages) {
    work.check();const auto bytes=std::filesystem::file_size(path);const auto stamp=std::filesystem::last_write_time(path);
    require(bytes<=1024ull*1024*1024,"cooked geometry exceeds format safety limit");
    std::ifstream file(path,std::ios::binary);Header header{};
    require(bool(file.read(reinterpret_cast<char*>(&header),sizeof(header))),"cooked geometry header truncated");
    require(header.magic==0x4743564fu && header.version==3 && header.flags==2 && header.materials==1 && !header.reserved &&
            header.source_triangles==triangles && header.clusters==clusters && header.pages==pages && header.groups && header.roots &&
            std::string(header.source_hash.data(),64)==hash,"cooked geometry source/window identity differs");
    const auto metadata=176ull+std::uint64_t(header.clusters)*56+std::uint64_t(header.groups)*40+
        std::uint64_t(header.dependencies)*4+std::uint64_t(header.roots)*4+std::uint64_t(header.pages)*88;
    require(metadata<=bytes && metadata<=8ull*1024*1024,"cooked geometry metadata exceeds bounded loader budget");
    const auto expected=header.metadata_hash;header.metadata_hash={};
    std::vector<std::uint8_t> data(static_cast<std::size_t>(metadata));std::memcpy(data.data(),&header,sizeof(header));
    require(bool(file.read(reinterpret_cast<char*>(data.data()+sizeof(header)),static_cast<std::streamsize>(data.size()-sizeof(header)))),
            "cooked geometry metadata truncated");
    require(content::resource_digest(data)==std::string(expected.data(),64),"cooked geometry metadata digest differs");
    const auto pageStart=metadata-std::uint64_t(header.pages)*88;std::uint64_t offset=metadata;
    for(std::uint32_t i=0;i<header.pages;++i) {
        work.check();Page page{};std::memcpy(&page,data.data()+pageStart+std::uint64_t(i)*88,sizeof(page));
        require(page.offset==offset && !page.reserved && page.encoded && page.encoded<=65536 && page.decoded==65536 &&
                page.codec<=1 && (page.codec!=0 || page.encoded==65536) && page.encoded<=bytes-offset &&
                hash_valid(std::string(page.checksum.data(),64)),"cooked page range or codec invalid");
        add(offset,page.encoded);
    }
    require(offset==bytes && stamp==std::filesystem::last_write_time(path) && bytes==std::filesystem::file_size(path),
            "cooked geometry file changed or has excess payload");
}
}
