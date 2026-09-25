#ifndef OCTARYN_BLOCK_TRANSPORT_SURFACE_ORIGIN
#define OCTARYN_BLOCK_TRANSPORT_SURFACE_ORIGIN
#include "Types.slang"
float bt_adjacent_float(float value,bool positive) {
    if(value==0)return asfloat(positive?0x00800000u:0x80800000u);
    return asfloat(asuint(value)+((value>0)==positive?1u:0xffffffffu));
}
bool bt_cell_bounds(int3 cell,out float3 low,out float3 high) {
    low=0;high=0;
    // Beyond exact integer float coordinates a world-space unit plane is ambiguous.
    if(any(cell < -16777216) || any(cell > 16777215))return false;
    low=float3(cell);high=float3(cell+1);
    return all(high-low==1);
}
bool bt_cube_interior(int4 key,inout float3 position) {
    float3 low,high;
    if(key.w<0 || key.w>=6 || !all(isfinite(position)) || !bt_cell_bounds(key.xyz,low,high))return false;
    float3 normal=bt_normal(key.w);
    for(uint axis=0;axis<3;++axis) {
        if(normal[axis]!=0) {position[axis]=normal[axis]>0?high[axis]:low[axis];continue;}
        float first=bt_adjacent_float(low[axis],true),last=bt_adjacent_float(high[axis],false);
        // The origin must remain strictly inside the unit face after world rounding.
        if(first>last || first<=low[axis] || last>=high[axis])return false;
        position[axis]=clamp(position[axis],first,last);
    }
    return all(isfinite(position));
}
bool bt_cube_position(int4 key,float2 sample,out float3 position) {
    position=0;float3 low,high;
    if(key.w<0 || key.w>=6 || !all(isfinite(sample)) || !bt_cell_bounds(key.xyz,low,high))return false;
    float3 normal=bt_normal(key.w),tangent=key.w<2?float3(0,0,1):float3(1,0,0);
    float3 local=.5+normal*.5+tangent*(sample.x-.5)+cross(normal,tangent)*(sample.y-.5);
    position=low+local;return bt_cube_interior(key,position);
}
bool bt_outward_origin(float3 position,float3 normal,out float3 origin) {
    origin=0;
    if(!all(isfinite(position)) || !all(isfinite(normal)) || dot(normal,normal)<.5)return false;
    // A tiny normal floor avoids subnormal flush at the zero plane. Else use one ULP.
    float3 rounded=position+normal*(1.0/1048576.0);
    for(uint axis=0;axis<3;++axis)origin[axis]=normal[axis]>0?
        max(rounded[axis],bt_adjacent_float(position[axis],true)):normal[axis]<0?
        min(rounded[axis],bt_adjacent_float(position[axis],false)):position[axis];
    // Crossed-plant normals have equal X/Z components: preserve the sampled UV.
    if(normal.y==0 && normal.x!=0 && abs(normal.x)==abs(normal.z)) {
        float step=max(max(abs(origin.x-position.x),abs(origin.z-position.z)),1.0/1048576.0);
        origin=position+float3(normal.x>0?step:-step,0,normal.z>0?step:-step);
        if(abs(origin.x-position.x)!=step || abs(origin.z-position.z)!=step)return false;
    }
    float3 delta=origin-position;
    // A whole-cell step means no usable unit-surface precision remains.
    return all(isfinite(origin)) && all(abs(delta)<1) && dot(delta,normal)>0;
}
#endif
