#include "GltfBufferViews.h"
#include "FilePath.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace octaryn::assets {
namespace {
void require(bool valid,const char* reason) {if(!valid)throw std::runtime_error(reason);}
std::span<const std::byte> resident(const fastgltf::DataSource& data) {
  using namespace fastgltf;
  if(const auto* array=std::get_if<sources::Array>(&data))return {array->bytes.data(),array->bytes.size()};
  if(const auto* view=std::get_if<sources::ByteView>(&data))return {view->bytes.data(),view->bytes.size()};
  if(const auto* vector=std::get_if<sources::Vector>(&data))return {vector->bytes.data(),vector->bytes.size()};
  return {};
}
}
GltfBufferViews::GltfBufferViews(std::filesystem::path parent,std::size_t budget,const std::atomic_bool* cancel):
    parent_(std::move(parent)),budget_(budget),cancel_(cancel) {}
void GltfBufferViews::clear() {views_.clear();retained_=0;}
std::vector<std::byte> GltfBufferViews::read(const fastgltf::Asset& asset,std::size_t index,
    std::size_t offset,std::size_t count) const {
  require(index<asset.buffers.size(),"map buffer index out of range");
  const auto& buffer=asset.buffers[index];
  require(offset<=buffer.byteLength && count<=buffer.byteLength-offset,"map buffer range exceeds declared length");
  require(count<=budget_-retained_,"map buffer working set exceeds preparation budget");
  if(cancel_ && cancel_->load(std::memory_order_relaxed))throw std::runtime_error("map preparation cancelled");
  const auto bytes=resident(buffer.data);
  std::vector<std::byte> result(count);
  if(!bytes.empty()) {
    require(offset<=bytes.size() && count<=bytes.size()-offset,"map buffer range exceeds resident data");
    std::memcpy(result.data(),bytes.data()+offset,count);return result;
  }
  const auto* uri=std::get_if<fastgltf::sources::URI>(&buffer.data);
  require(uri && uri->uri.isLocalPath(),"map buffer requires local source data");
  const auto path=parent_/uri->uri.fspath();
  const auto size=std::filesystem::file_size(content::file_io_path(path));
  require(uri->fileByteOffset<=size && offset<=size-uri->fileByteOffset &&
      count<=size-uri->fileByteOffset-offset,"map buffer range exceeds external file");
  const auto position=uri->fileByteOffset+offset;
  require(position<=std::uint64_t(std::numeric_limits<std::streamoff>::max()),"map buffer offset exceeds file API range");
  std::ifstream file(content::file_io_path(path),std::ios::binary);
  require(file.good(),"cannot read external map buffer");
  file.seekg(static_cast<std::streamoff>(position));
  require(file.good(),"cannot seek external map buffer");
  constexpr std::size_t batch=1024*1024;
  for(std::size_t done=0;done<count;) {
    if(cancel_ && cancel_->load(std::memory_order_relaxed))throw std::runtime_error("map preparation cancelled");
    const auto part=std::min(batch,count-done);
    file.read(reinterpret_cast<char*>(result.data()+done),static_cast<std::streamsize>(part));
    require(file.good() && file.gcount()==static_cast<std::streamsize>(part),"short external map buffer read");
    done+=part;
  }
  return result;
}
fastgltf::span<const std::byte> GltfBufferViews::operator()(const fastgltf::Asset& asset,std::size_t index) const {
  if(cancel_ && cancel_->load(std::memory_order_relaxed))throw std::runtime_error("map preparation cancelled");
  require(index<asset.bufferViews.size(),"map buffer view index out of range");
  const auto& view=asset.bufferViews[index];
  if(const auto found=views_.find(index);found!=views_.end())return fastgltf::span<const std::byte>(found->second.data(),found->second.size());
  require(view.bufferIndex<asset.buffers.size(),"map buffer index out of range");
  require(view.byteOffset<=asset.buffers[view.bufferIndex].byteLength &&
      view.byteLength<=asset.buffers[view.bufferIndex].byteLength-view.byteOffset,"map buffer view exceeds declared length");
  if(!view.meshoptCompression) {
    const auto bytes=resident(asset.buffers[view.bufferIndex].data);
    if(!bytes.empty()) {
      require(view.byteOffset<=bytes.size() && view.byteLength<=bytes.size()-view.byteOffset,"map view exceeds resident buffer");
      return fastgltf::span<const std::byte>(bytes.data()+view.byteOffset,view.byteLength);
    }
    auto decoded=read(asset,view.bufferIndex,view.byteOffset,view.byteLength);
    retained_+=decoded.size();auto& stored=views_.emplace(index,std::move(decoded)).first->second;
    return fastgltf::span<const std::byte>(stored.data(),stored.size());
  }
  const auto& compression=*view.meshoptCompression;
  require(!view.byteStride || *view.byteStride==compression.byteStride,"meshopt stride differs from its buffer view");
  require(compression.count && compression.byteStride &&
      compression.count<=std::numeric_limits<std::size_t>::max()/compression.byteStride &&
      compression.count*compression.byteStride==view.byteLength,"invalid meshopt decoded buffer length");
  require(view.byteLength<=budget_-retained_,"meshopt decoded view exceeds preparation budget");
  // Account for the decoded destination while reading its compressed source.
  retained_+=view.byteLength;
  auto encoded=read(asset,compression.bufferIndex,compression.byteOffset,compression.byteLength);
  std::vector<std::byte> decoded(view.byteLength);
  const auto* source=reinterpret_cast<const unsigned char*>(encoded.data());int status=-1;
  using Mode=fastgltf::MeshoptCompressionMode;
  using Filter=fastgltf::MeshoptCompressionFilter;
  if(compression.mode==Mode::Attributes) {
    require(compression.byteStride<=256 && compression.byteStride%4==0,"invalid meshopt vertex stride");
    status=meshopt_decodeVertexBuffer(decoded.data(),compression.count,compression.byteStride,source,encoded.size());
  } else if(compression.mode==Mode::Triangles || compression.mode==Mode::Indices) {
    require(compression.byteStride==2 || compression.byteStride==4,"invalid meshopt index stride");
    require(compression.filter==Filter::None,"meshopt index filters are invalid");
    if(compression.mode==Mode::Triangles) {
      require(compression.count%3==0,"invalid meshopt triangle count");
      status=meshopt_decodeIndexBuffer(decoded.data(),compression.count,compression.byteStride,source,encoded.size());
    } else status=meshopt_decodeIndexSequence(decoded.data(),compression.count,compression.byteStride,source,encoded.size());
  }
  require(status==0,"meshopt compressed buffer is invalid");
  if(compression.filter==Filter::Octahedral) {
    require(compression.byteStride==4 || compression.byteStride==8,"invalid meshopt octahedral stride");
    meshopt_decodeFilterOct(decoded.data(),compression.count,compression.byteStride);
  } else if(compression.filter==Filter::Quaternion) {
    require(compression.byteStride==8,"invalid meshopt quaternion stride");
    meshopt_decodeFilterQuat(decoded.data(),compression.count,compression.byteStride);
  } else if(compression.filter==Filter::Exponential) {
    require(compression.byteStride%4==0,"invalid meshopt exponential stride");
    meshopt_decodeFilterExp(decoded.data(),compression.count,compression.byteStride);
  } else require(compression.filter==Filter::None,"unsupported meshopt filter");
  auto& stored=views_.emplace(index,std::move(decoded)).first->second;
  return fastgltf::span<const std::byte>(stored.data(),stored.size());
}
}
