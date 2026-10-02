#include "ScenePreparation.h"
#include "ResourceDigest.h"
#include <array>
#include <algorithm>
#include <vector>
#include <cstring>
#include <fstream>
#include <cstdio>
#include <stdexcept>
using namespace octaryn::scene_loading;
struct Header {
  std::uint32_t magic=0x4743564fu,version{},flags=2,materials=1,clusters=1,groups=1,pages=1,dependencies=1,roots=1,reserved{};
  std::uint64_t triangles=1;std::array<char,64> source_hash{},metadata_hash{};
};
struct Page {std::uint64_t offset{};std::uint32_t encoded=65536,decoded=65536,codec{},reserved{};std::array<char,64> checksum{};};
static_assert(sizeof(Header)==176 && sizeof(Page)==88);
int main(int argc,char** argv) {
  try {
    if(argc!=2)throw std::runtime_error("Fixture output directory required");
    const auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directory(root);
    Preparation work;work.root=root;const std::string source_hash(64,'a');
    for(unsigned version:{2u,3u,4u}) {
      Header header;header.version=version;header.source_hash.fill('a');
      constexpr std::size_t metadata_bytes=176+56+40+4+4+88;
      std::vector<std::uint8_t> metadata(metadata_bytes),payload(65536);
      std::memcpy(metadata.data(),&header,sizeof(header));
      Page page;page.offset=metadata_bytes;
      const auto checksum=octaryn::content::resource_digest(payload);std::copy_n(checksum.begin(),64,page.checksum.begin());
      std::memcpy(metadata.data()+metadata_bytes-sizeof(Page),&page,sizeof(page));
      const auto digest=octaryn::content::resource_digest(metadata);
      std::copy_n(digest.begin(),64,header.metadata_hash.begin());std::memcpy(metadata.data(),&header,sizeof(header));
      const auto path=root/("v"+std::to_string(version)+".vgeom");
      {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(metadata.data()),metadata.size());file.write(reinterpret_cast<const char*>(payload.data()),payload.size());if(!file)throw std::runtime_error("Fixture write failed");}
      bool accepted=false;
      try {cook_identity(work,path,source_hash,1,1,1);accepted=true;}catch(const std::exception& error) {
        if(version==3)throw;
        if(std::string(error.what()).find("source/window identity") == std::string::npos)throw;
      }
      if(accepted!=(version==3))throw std::runtime_error("Cook generation admission mismatch");
    }
    std::puts("scene_cook_generations passed=1 assertions=3 accepted_version=3 rejected_versions=2,4 gpu=0");return 0;
  }catch(const std::exception& error) {std::fprintf(stderr,"scene_cook_generations failed=%s\n",error.what());return 1;}
}
