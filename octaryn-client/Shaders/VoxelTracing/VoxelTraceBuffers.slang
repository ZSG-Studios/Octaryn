#ifndef OCTARYN_VOXEL_TRACE_BUFFERS
#define OCTARYN_VOXEL_TRACE_BUFFERS
#include "VoxelQuery.slang"

// Matches Source/Rendering/VoxelTracing/VoxelTraceTypes.h, 48-byte header.
struct VoxelTraceChunkHeader {
    int3 coordinate;
    uint macroMask;
    uint2 geometryEpoch;
    uint leafOffset;
    uint macroOffset;
    uint materialOffset;
    uint minLocalY;
    uint maxLocalY;
    uint flags;
};
StructuredBuffer<VoxelTraceChunkHeader> voxelTraceChunks;
StructuredBuffer<uint2> voxelTraceLeaves;
StructuredBuffer<uint2> voxelTraceMacros;
StructuredBuffer<uint> voxelTraceMaterials;
uniform uint4 voxelTraceCounts; // Header, uint2 leaf, uint2 macro, uint material counts.
// Open-addressed chunk index from VoxelTraceUploadPlan: linear probing, stored
// entry is header index + 1, zero means an empty slot that ends the probe run.
StructuredBuffer<uint> voxelTraceHash;
uniform uint voxelTraceHashMask;
uniform uint2 voxelTraceGeneration;

// Bit-identical mirror of trace_chunk_hash (VoxelTraceUploadPlan.h). Negative
// coordinates cast modulo 2^32, matching uint32_t(int32_t) on the host.
uint voxel_trace_chunk_hash(int3 key) {
    uint h=uint(key.x)*0x8da6b343u ^ uint(key.y)*0xd8163841u ^ uint(key.z)*0xcb1ab31fu;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
uint2 voxel_trace_generation() {return voxelTraceGeneration;}

// Missing keys and partial-height gaps are UNKNOWN, not a known empty chunk.
struct VoxelTraceBuffers : IVoxelTraceAccessor {
    uint chunk(int3 coordinate,out uint token) {
        token=0;
        if(voxelTraceHashMask==0 || voxelTraceCounts.x==0)return VOXEL_CHUNK_UNKNOWN;
        uint at=voxel_trace_chunk_hash(coordinate)&voxelTraceHashMask;
        // The plan keeps load at or below 1/2, so a zero slot always terminates;
        // the probe cap only defends against a corrupt or stale table.
        for(uint probes=0;probes<256;++probes) {
            uint entry=voxelTraceHash[at];
            if(entry==0)return VOXEL_CHUNK_UNKNOWN;
            uint index=entry-1;
            if(index<voxelTraceCounts.x) {
                VoxelTraceChunkHeader header=voxelTraceChunks[index];
                if(all(header.coordinate==coordinate)) {
                    token=index;
                    if((header.flags&1)==0 || header.minLocalY>=header.maxLocalY ||
                       header.maxLocalY>32)return VOXEL_CHUNK_UNKNOWN;
                    if(header.macroMask==0)return header.minLocalY==0 && header.maxLocalY==32?
                        VOXEL_CHUNK_EMPTY:VOXEL_CHUNK_RESIDENT;
                    if(header.leafOffset>voxelTraceCounts.y || voxelTraceCounts.y-header.leafOffset<512 ||
                       header.macroOffset>voxelTraceCounts.z || voxelTraceCounts.z-header.macroOffset<8 ||
                       header.materialOffset>voxelTraceCounts.w ||
                       voxelTraceCounts.w-header.materialOffset<16384)return VOXEL_CHUNK_UNKNOWN;
                    return VOXEL_CHUNK_RESIDENT;
                }
            }
            at=(at+1)&voxelTraceHashMask;
        }
        return VOXEL_CHUNK_UNKNOWN;
    }
    uint2 knownY(uint token) {return uint2(voxelTraceChunks[token].minLocalY,voxelTraceChunks[token].maxLocalY);}
    uint2 epoch(uint token) {return voxelTraceChunks[token].geometryEpoch;}
    uint2 macroMask(uint token) {return uint2(voxelTraceChunks[token].macroMask,0);}
    uint2 brickMask(uint token,uint macroIndex) {return voxelTraceMacros[voxelTraceChunks[token].macroOffset+macroIndex];}
    uint2 voxelMask(uint token,uint brickIndex) {return voxelTraceLeaves[voxelTraceChunks[token].leafOffset+brickIndex];}
    uint material(uint token,uint3 localVoxel) {
        uint index=voxel_linear(localVoxel,32);
        uint packed=voxelTraceMaterials[voxelTraceChunks[token].materialOffset+index/2];
        return (packed>>((index&1)*16))&65535;
    }
};
#endif
