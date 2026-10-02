#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::client::animation {
using Vec3=std::array<float,3>;
using Vec4=std::array<float,4>;
// Column-major; GPU palettes are converted explicitly to rows.
using Matrix=std::array<float,16>;
constexpr Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct Transform {Vec3 translation{};Vec4 rotation{0,0,0,1};Vec3 scale{1,1,1};};
struct Node {
  std::int32_t parent{-1};
  Transform rest;
  Matrix matrix{identity};
  bool has_matrix{};
  std::vector<float> weights;
};
struct Skin {std::vector<std::uint32_t> joints;std::vector<Matrix> inverse_bind;};
enum class Path { Translation,Rotation,Scale,Weights };
enum class Interpolation { Step,Linear,CubicSpline };
struct Channel {
  std::uint32_t node{},components{};
  Path path{};Interpolation interpolation{};
  std::vector<float> times,values;
};
struct Clip {std::string name;float duration{};std::vector<Channel> channels;};
// All fields are 16-byte blocks, matching Deform.slang.
struct SourceVertex {
  Vec4 position{},normal{0,1,0,0},tangent{1,0,0,1},uv{},color{1,1,1,1};
  std::array<std::uint32_t,8> joints{};
  std::array<float,8> weights{};
};
static_assert(sizeof(SourceVertex)==144);
struct MorphDelta {Vec4 position{},normal{},tangent{};};
static_assert(sizeof(MorphDelta)==48);
struct Primitive {
  std::uint32_t node{},morph_count{};
  std::int32_t skin{-1},material{-1};
  std::vector<SourceVertex> vertices;
  std::vector<std::uint32_t> indices;
  // Target-major: target * vertices.size() + vertex.
  std::vector<MorphDelta> morphs;
};
struct Asset {std::vector<Node> nodes;std::vector<Skin> skins;std::vector<Clip> clips;std::vector<Primitive> primitives;};
struct LoadLimits {
  std::uint64_t source_bytes{512ull*1024*1024};
  std::size_t nodes{65536},vertices{8000000},morph_deltas{32000000},keys{4000000};
  std::size_t joints{4096},morph_targets{256},primitives{16384};
  // Clip/skeleton libraries may have no mesh; scene callers retain the strict default.
  bool require_geometry{true};
};
bool load_asset(const std::filesystem::path&,Asset&,std::string& error,const LoadLimits& limits={});
}
