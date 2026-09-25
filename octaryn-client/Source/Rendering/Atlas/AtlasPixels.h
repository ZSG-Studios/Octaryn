#pragma once
#include <SDL3/SDL.h>
inline constexpr int ATLAS_BLOCK_WIDTH=32;
inline constexpr int ATLAS_LAYER_COUNT=29;
inline constexpr int ATLAS_MIP_LEVELS=6;
inline constexpr float ATLAS_ALPHA_CUTOFF_U8=0.35f*255;
enum atlas_mip_kind_t { ATLAS_MIP_ALBEDO, ATLAS_MIP_LABPBR_NORMAL, ATLAS_MIP_LABPBR_SPECULAR };
float atlas_alpha_coverage(const Uint8*,int);
void atlas_preserve_alpha_coverage(Uint8*,int,float);
void atlas_dilate_transparent_rgb(Uint8*,int);
void atlas_downsample_tile_rgba(const Uint8*,int,Uint8*);
void atlas_pack_layer_mips(Uint8*,Uint32*,Uint8*,Uint8*,Uint32,atlas_mip_kind_t);
