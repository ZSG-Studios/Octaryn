#pragma once
#include "FarField.h"
#include <cstddef>

namespace octaryn::client::world_presentation {struct StreamColumn;}
namespace octaryn::client::rendering {
struct FarFieldEdit {std::int32_t x{},y{},z{};std::uint16_t material{};};
// Complete is an explicit server edit-coverage guarantee for this exact key.
// Resident stream absence is not such a guarantee. Edits are ordered; last wins.
struct FarFieldAuthority {
  FarFieldKey key;
  std::uint64_t revision{};
  std::span<const FarFieldEdit> edits;
  bool complete{};
};
struct FarFieldWork {
  std::size_t column_samples{},voxel_samples{},edit_records{};
};
enum class FarFieldBuildState { Ready, Refine, Deferred, Unknown };
struct FarFieldBuild {FarFieldBuildState state{FarFieldBuildState::Unknown};FarFieldNode node;};
using FarFieldMaterialFeatures=std::uint32_t(*)(std::uint16_t,void*);
// Revision-3 generator parity. No voxel traversal is performed for 16/64 cells:
// empty-envelope proofs complete directly; other cells require bounded children.
FarFieldBuild far_field_generate(FarFieldKey,const FarFieldAuthority&,unsigned generator_revision,
    FarFieldWork&,FarFieldMaterialFeatures,void* context=nullptr);
FarFieldBuild far_field_resident(FarFieldKey,const world_presentation::StreamColumn&,
    FarFieldMaterialFeatures,void* context=nullptr);
} // namespace octaryn::client::rendering
