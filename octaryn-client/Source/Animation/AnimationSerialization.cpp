#include "AnimationSerialization.h"
#include <bit>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace octaryn::client::animation {
namespace {
static_assert(std::endian::native==std::endian::little && sizeof(Transform)==40 && sizeof(rendering::MapTexture)==48);
constexpr size_t maximum_bytes=1024ull*1024*1024;
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
struct Writer {
  std::vector<std::uint8_t> bytes;
  void raw(const void* source,size_t count) {
    require(count<=maximum_bytes-bytes.size(),"animation payload exceeds one GiB");
    const auto old=bytes.size();bytes.resize(old+count);if(count)std::memcpy(bytes.data()+old,source,count);
  }
  template<class T> void pod(const T& value) {static_assert(std::is_trivially_copyable_v<T>);raw(&value,sizeof(value));}
  void count(size_t value) {require(value<UINT32_MAX,"animation vector too large");pod(std::uint32_t(value));}
  template<class T> void vector(const std::vector<T>& values) {count(values.size());raw(values.data(),values.size()*sizeof(T));}
  void string(const std::string& value) {count(value.size());raw(value.data(),value.size());}
};
struct Reader {
  std::span<const std::uint8_t> bytes;
  void raw(void* output,size_t count) {
    require(count<=bytes.size(),"animation payload truncated");if(count)std::memcpy(output,bytes.data(),count);bytes=bytes.subspan(count);
  }
  template<class T> void into(T& value) {static_assert(std::is_trivially_copyable_v<T>);raw(&value,sizeof(value));}
  template<class T> T pod() {T value;into(value);return value;}
  unsigned count(size_t limit) {const auto value=pod<unsigned>();require(value<=limit,"animation vector exceeds limit");return value;}
  template<class T> void vector(std::vector<T>& values,size_t limit) {
    const auto size=count(limit);require(size<=bytes.size()/sizeof(T),"animation vector payload truncated");values.resize(size);raw(values.data(),size*sizeof(T));
  }
  std::string string(size_t limit) {const auto size=count(limit);require(size<=bytes.size(),"animation string truncated");std::string result(size,'\0');raw(result.data(),size);return result;}
};
void material(Writer& w,const rendering::MapMaterial& m) {
  w.pod(m.base_color);w.pod(m.metallic);w.pod(m.roughness);w.pod(m.alpha_cutoff);w.pod(m.normal_scale);w.pod(m.occlusion_strength);
  w.pod(m.emissive);w.pod(unsigned(m.alpha_mode));w.pod(unsigned(m.double_sided));w.pod(m.texture);w.pod(m.textures);
}
void material(Reader& r,rendering::MapMaterial& m) {
  r.into(m.base_color);r.into(m.metallic);r.into(m.roughness);r.into(m.alpha_cutoff);r.into(m.normal_scale);r.into(m.occlusion_strength);
  r.into(m.emissive);m.alpha_mode=rendering::MapAlphaMode(r.pod<unsigned>());
  const auto sided=r.pod<unsigned>();require(sided<=1,"animation double-sided flag invalid");m.double_sided=sided!=0;
  r.into(m.texture);r.into(m.textures);
}
}
std::vector<std::uint8_t> encode_animation_payload(const CookedAsset& cooked) {
  Writer w;const auto& a=cooked.animation;
  w.count(a.nodes.size());
  for(const auto& n:a.nodes) {w.pod(n.parent);w.pod(n.rest);w.pod(n.matrix);w.pod(unsigned(n.has_matrix));w.vector(n.weights);}
  w.count(a.skins.size());for(const auto& s:a.skins) {w.vector(s.joints);w.vector(s.inverse_bind);}
  w.count(a.clips.size());
  for(const auto& c:a.clips) {
    w.string(c.name);w.pod(c.duration);w.count(c.channels.size());
    for(const auto& channel:c.channels) {
      w.pod(channel.node);w.pod(channel.components);w.pod(unsigned(channel.path));w.pod(unsigned(channel.interpolation));
      w.vector(channel.times);w.vector(channel.values);
    }
  }
  w.count(a.primitives.size());
  for(const auto& p:a.primitives) {
    w.pod(p.node);w.pod(p.morph_count);w.pod(p.skin);w.pod(p.material);w.vector(p.vertices);w.vector(p.indices);w.vector(p.morphs);
  }
  w.count(cooked.materials.size());for(const auto& m:cooked.materials)material(w,m);
  w.count(cooked.images.size());for(const auto& image:cooked.images) {w.string(image.mime_type);w.vector(image.bytes);}
  w.vector(cooked.cluster_sources);w.vector(cooked.source_vertices);return std::move(w.bytes);
}
void decode_animation_payload(std::span<const std::uint8_t> bytes,CookedAsset& cooked) {
  Reader r{bytes};auto& a=cooked.animation;LoadLimits limits;
  a.nodes.resize(r.count(limits.nodes));
  for(auto& n:a.nodes) {
    r.into(n.parent);r.into(n.rest);r.into(n.matrix);const auto matrix=r.pod<unsigned>();require(matrix<=1,"animation node matrix flag invalid");
    n.has_matrix=matrix!=0;r.vector(n.weights,limits.morph_targets);
  }
  a.skins.resize(r.count(limits.nodes));for(auto& s:a.skins) {r.vector(s.joints,limits.joints);r.vector(s.inverse_bind,limits.joints);}
  a.clips.resize(r.count(limits.keys));
  for(auto& c:a.clips) {
    c.name=r.string(4096);r.into(c.duration);c.channels.resize(r.count(limits.nodes*4));
    for(auto& channel:c.channels) {
      r.into(channel.node);r.into(channel.components);channel.path=Path(r.pod<unsigned>());channel.interpolation=Interpolation(r.pod<unsigned>());
      r.vector(channel.times,limits.keys);r.vector(channel.values,limits.keys*limits.morph_targets*3);
    }
  }
  a.primitives.resize(r.count(limits.primitives));
  for(auto& p:a.primitives) {
    r.into(p.node);r.into(p.morph_count);r.into(p.skin);r.into(p.material);
    r.vector(p.vertices,limits.vertices);r.vector(p.indices,limits.vertices*3);r.vector(p.morphs,limits.morph_deltas);
  }
  cooked.materials.resize(r.count(limits.primitives));for(auto& m:cooked.materials)material(r,m);
  cooked.images.resize(r.count(1024));for(auto& image:cooked.images) {image.mime_type=r.string(64);r.vector(image.bytes,86ull*1024*1024);}
  r.vector(cooked.cluster_sources,limits.vertices*3);r.vector(cooked.source_vertices,limits.vertices*3);
  require(r.bytes.empty(),"animation payload has trailing bytes");
}
}
