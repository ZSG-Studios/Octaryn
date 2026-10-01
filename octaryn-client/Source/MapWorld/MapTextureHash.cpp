#include "MapTextureCache.h"
#include "ResourceDigest.h"
#include <array>
#include <bit>

namespace octaryn::client::rendering {
std::string map_texture_digest_parts(std::span<const std::span<const std::uint8_t>> parts) {
  return content::resource_digest_parts(parts);
}
std::string map_texture_digest(std::span<const std::uint8_t> input) {
  return content::resource_digest(input);
}
std::string map_texture_file_digest(const std::filesystem::path& path,std::string& error,
    std::uint64_t maximum_bytes,const std::atomic_bool* cancel) {
  return content::resource_file_digest(path,error,maximum_bytes,cancel);
}
std::string map_texture_cache_key(const MapModelImage& image,const MapMipOptions& options) {
  // Version covers filtering, encoder settings, quality gates and DDS contract.
  std::array<std::uint8_t,28> bytes{};size_t offset{};
  const auto append=[&](std::uint32_t value) {for(unsigned i=0;i<4;++i)bytes[offset++]=static_cast<std::uint8_t>(value>>(i*8));};
  append(0x5a534754);append(map_texture_cache_version);append(static_cast<unsigned>(options.role));
  append(options.alpha_weighted?1:0);append(options.preserve_coverage?1:0);
  append(std::bit_cast<std::uint32_t>(options.alpha_cutoff));append(std::bit_cast<std::uint32_t>(options.alpha_factor));
  const std::array<std::span<const std::uint8_t>,2> parts{bytes,image.bytes};
  return map_texture_digest_parts(parts);
}
}
