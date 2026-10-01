#include "GltfMappedViews.h"
#include "FilePath.h"
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace octaryn::assets {
GltfMappedViews::GltfMappedViews(std::filesystem::path parent,const std::filesystem::path& scratch,
    std::size_t budget,const std::atomic_bool* cancel):parent_(std::move(parent)),budget_(budget),cancel_(cancel) {
  static std::atomic_uint64_t sequence{};
  std::filesystem::create_directories(content::file_io_path(scratch));
  directory_=content::file_io_path(scratch)/("gltf-views-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+
      "-"+std::to_string(sequence.fetch_add(1)));
  if(!std::filesystem::create_directory(directory_))throw std::runtime_error("cannot create isolated glTF scratch directory");
}
GltfMappedViews::~GltfMappedViews() {
  clear();std::error_code ignored;std::filesystem::remove(directory_,ignored);
}
void GltfMappedViews::clear() {
  views_.clear();
  for(const auto& file:files_) {std::error_code ignored;std::filesystem::remove(file,ignored);}
  files_.clear();
}
fastgltf::span<const std::byte> GltfMappedViews::operator()(const fastgltf::Asset& asset,std::size_t index) const {
  if(cancel_ && cancel_->load(std::memory_order_relaxed))throw std::runtime_error("map preparation cancelled");
  if(const auto found=views_.find(index);found!=views_.end()) {
    const auto bytes=static_cast<fastgltf::span<std::byte>>(found->second);
    return fastgltf::span<const std::byte>(bytes.data(),bytes.size());
  }
  const auto path=directory_/(std::to_string(index)+".view");files_.push_back(path);
  {
    GltfBufferViews decoder(parent_,budget_,cancel_);const auto decoded=decoder(asset,index);
    std::ofstream file(content::file_io_path(path),std::ios::binary|std::ios::trunc);
    file.write(reinterpret_cast<const char*>(decoded.data()),static_cast<std::streamsize>(decoded.size()));
    file.close();if(!file.good())throw std::runtime_error("cannot write decoded glTF scratch view");
  }
  auto mapped=fastgltf::MappedGltfFile::FromPath(content::file_io_path(path));
  if(mapped.error()!=fastgltf::Error::None)throw std::runtime_error("cannot map decoded glTF scratch view");
  auto& stored=views_.emplace(index,std::move(mapped.get())).first->second;
  const auto bytes=static_cast<fastgltf::span<std::byte>>(stored);
  return fastgltf::span<const std::byte>(bytes.data(),bytes.size());
}
}
