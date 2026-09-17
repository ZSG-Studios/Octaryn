// Surface.cpp is generated from the production Slang entry by validate_voxel_surface.py.
#include "Surface.cpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
struct Atlas final:ITexture {
    float alpha{1};
    TextureDimensions GetDimensions(int=-1) override {return {SLANG_TEXTURE_2D|SLANG_TEXTURE_ARRAY_FLAG,32,32,1,1,29};}
    void Load(const int32_t*,void* out,size_t size) override {write(out,size);}
    void Sample(SamplerState,const float*,void* out,size_t size) override {write(out,size);}
    void SampleLevel(SamplerState,const float*,float,void* out,size_t size) override {write(out,size);}
    void write(void* out,size_t size) {const float color[4]={.5f,.6f,.7f,alpha};require(size==sizeof(color),"atlas sample size");std::memcpy(out,color,size);}
};
struct Fixture {
    Atlas atlas;
    VoxelTraceChunkHeader_0 header{};
    std::array<Vector<uint32_t,2>,512> leaves{};
    std::array<Vector<uint32_t,2>,8> macros{};
    std::array<uint32_t,16384> voxels{};
    std::array<AtlasBlockMaterial_0,8> materials{};
    std::array<uint32_t,4> chunkHash{1,0,0,0}; // trace_chunk_hash(0,0,0)=0, entry=index+1.
    GlobalParams_0 globals{};
    Fixture() {
        header.maxLocalY_0=32;header.flags_0=1;header.geometryEpoch_1={11,3};
        materials[1].flags_1=1|16|1024;
        materials[2].flags_1=2|8|32;
        materials[3].flags_1=2|8;
        materials[4].flags_1=4|8;
        materials[5].flags_1=8|64|1024;
        materials[6].flags_1=4|8|512;
        materials[7].flags_1=8|128;
        for(unsigned i=0;i<6;++i) {materials[2].layers_0[i]=17;materials[3].layers_0[i]=10;}
        globals.voxelTraceChunks_0={&header,1};globals.voxelTraceLeaves_0={leaves.data(),leaves.size()};
        globals.voxelTraceMacros_0={macros.data(),macros.size()};globals.voxelTraceMaterials_0={voxels.data(),voxels.size()};
        globals.voxelTraceCounts_0={1,512,8,16384};globals.blockMaterials_0={materials.data(),materials.size()};
        globals.voxelTraceHash_0={chunkHash.data(),chunkHash.size()};globals.voxelTraceHashMask_0=3;
        globals.voxelTraceMaterialCount_0=unsigned(materials.size());globals.atlasAlbedo_0.texture=&atlas;
        globals.probeBounds_0={{0,0,0},{1,1,1}};
    }
    void clear() {header.macroMask_0=0;leaves={};macros={};voxels={};atlas.alpha=1;}
    void put(unsigned x,unsigned y,unsigned z,unsigned material) {
        const auto voxel=x+32*(y+32*z),leaf=x/4+8*(y/4+8*(z/4)),bit=x%4+4*(y%4+4*(z%4));
        auto& mask=leaves[leaf];(bit<32?mask.x:mask.y)|=1u<<(bit%32);
        const auto macro=x/16+2*(y/16+2*(z/16)),child=(x/4)%4+4*((y/4)%4+4*((z/4)%4));
        auto& parent=macros[macro];(child<32?parent.x:parent.y)|=1u<<(child%32);header.macroMask_0|=1u<<macro;
        voxels[voxel/2]|=material<<((voxel%2)*16);
    }
    VoxelSurfaceHit_0 trace(Vector<float,3> origin,Vector<float,3> direction,float maximum=5) {
        VoxelTraceRay_0 ray{{0,0,0},origin,direction,0,maximum,4096};VoxelSurfaceHit_0 hit{};
        globals.probeRays_0={&ray,1};globals.probeHits_0={&hit,1};
        ComputeVaryingInput dispatch{};dispatch.endGroupID={1,1,1};main_0(&dispatch,nullptr,&globals);return hit;
    }
};
void hit(const VoxelSurfaceHit_0& value,unsigned material,float distance,const char* label) {
    if(value.cell_0.status_0!=1 || value.cell_0.material_0!=material || std::abs(value.cell_0.distance_0-distance)>2e-5f)
        std::fprintf(stderr,"%s status=%u material=%u t=%.9g expected=%.9g\n",label,value.cell_0.status_0,value.cell_0.material_0,value.cell_0.distance_0,distance);
    require(value.cell_0.status_0==1 && value.cell_0.material_0==material && std::abs(value.cell_0.distance_0-distance)<=2e-5f,label);
}
}
int main() {
    try {
        Fixture f;f.put(8,8,8,1);
        hit(f.trace({8.5f,8.5f,7},{0,0,1}),1,1,"opaque cube entry");
        hit(f.trace({8.5f,8.5f,8.5f},{0,0,1}),1,0,"inside opaque start");
        require(f.trace({8.5f,8.5f,7},{0,0,1},1).cell_0.status_0==0,"finite tMax excludes entry");
        f.clear();f.put(8,8,8,2);
        hit(f.trace({8.5f,8.3f,7},{0,0,1}),2,1.4375f,"thin torch body");
        require(f.trace({8.1f,8.3f,7},{0,0,1},3).cell_0.status_0==0,"torch cell is not a solid cube");
        require(f.trace({8.5f,8.9f,7},{0,0,1},3).cell_0.status_0==0,"torch height is ten sixteenths");
        f.clear();f.put(8,8,8,3);
        hit(f.trace({8.25f,8.5f,7},{0,0,1}),3,1.25f,"crossed card geometry");
        f.atlas.alpha=.34f;require(f.trace({8.25f,8.5f,7},{0,0,1},3).cell_0.status_0==0,"cutout alpha hole");
        f.put(8,8,10,1);hit(f.trace({8.25f,8.5f,7},{0,0,1}),1,3,"opaque beyond alpha hole");
        f.clear();f.put(8,8,8,5);hit(f.trace({8.5f,8.5f,7},{0,0,1}),5,1,"glass remains a material surface");
        for(unsigned fluid:{4u,6u}) {
            f.clear();for(unsigned z=7;z<=9;++z)for(unsigned x=7;x<=9;++x)f.put(x,8,z,fluid);
            hit(f.trace({8.5f,10,8.5f},{0,-1,0}),fluid,10-(8+8.f/9),"fluid smoothed top");
            require(f.trace({8.5f,8.95f,7.5f},{0,0,1},2).cell_0.status_0==0,"above fluid is not an occupied cube");
        }
        f.clear();f.put(0,8,8,4);require(f.trace({.5f,10,8.5f},{0,-1,0}).cell_0.status_0==2,"unknown fluid neighbor is not air");
        f.clear();f.put(8,8,8,7);require(f.trace({8.5f,8.5f,7},{0,0,1},3).cell_0.status_0==0,"procedural cloud block is not meshed geometry");
        std::puts("voxel_surface=passed production_slang_cpu=1 cube=1 inside=1 finite_bounds=1 torch_shape=1 crossed_cards=1 alpha_holes=1 glass=1 water=1 lava=1 unknown_neighbor=1 gpu_runtime=0");
        return 0;
    } catch(const std::exception& error) {std::fprintf(stderr,"voxel_surface=failed %s\n",error.what());return 1;}
}
