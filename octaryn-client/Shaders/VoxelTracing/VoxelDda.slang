#ifndef OCTARYN_VOXEL_DDA
#define OCTARYN_VOXEL_DDA

// Amanatides/Woo grid traversal; fixed-size levels are composed by VoxelQuery.
// Coordinates here are relative to an integer chunk origin, never global floats.
static const float voxel_trace_infinity = 3.402823466e+38;

struct VoxelRayInterval {
    float begin;
    float end;
    int3 normal;
};

bool voxel_clip_ray(float3 origin, float3 direction, float3 lower, float3 upper,
                    float tMin, float tMax, out VoxelRayInterval interval) {
    interval.begin=tMin; interval.end=tMax; interval.normal=0;
    if(!all(isfinite(origin)) || !all(isfinite(direction)) ||
       !isfinite(tMin) || !isfinite(tMax) || tMin<0 || tMax<=tMin ||
       !any(direction!=0) || any(upper<=lower))return false;
    for(uint axis=0;axis<3;++axis) {
        if(direction[axis]==0) {
            // Parallel rays on the upper face belong to the adjacent cell.
            if(origin[axis]<lower[axis] || origin[axis]>=upper[axis])return false;
            continue;
        }
        float a=(lower[axis]-origin[axis])/direction[axis];
        float b=(upper[axis]-origin[axis])/direction[axis];
        float nearTime=min(a,b),farTime=max(a,b);
        if(nearTime>interval.begin) {
            interval.begin=nearTime; interval.normal=0;
            interval.normal[axis]=direction[axis]>0?-1:1;
        }
        interval.end=min(interval.end,farTime);
        if(interval.begin>=interval.end)return false;
    }
    return true;
}

struct VoxelDda {
    int3 cell;
    int3 step;
    int3 normal;
    float3 next;
    float3 origin;
    float3 direction;
    float time;
    float end;
    int size;
};

void voxel_dda_boundaries(inout VoxelDda dda) {
    for(uint axis=0;axis<3;++axis) {
        dda.next[axis]=voxel_trace_infinity;
        if(dda.step[axis]!=0) {
            int boundary=dda.cell[axis]+(dda.step[axis]>0?dda.size:0);
            dda.next[axis]=(float(boundary)-dda.origin[axis])/dda.direction[axis];
        }
    }
}

VoxelDda voxel_dda_begin(float3 origin,float3 direction,VoxelRayInterval interval,
                         int size,int3 lower,int3 upper) {
    VoxelDda dda;
    dda.origin=origin; dda.direction=direction; dda.size=size;
    dda.time=interval.begin; dda.end=interval.end; dda.normal=interval.normal;
    float3 coordinate=(origin+direction*interval.begin)/float(size);
    for(uint axis=0;axis<3;++axis) {
        dda.step[axis]=direction[axis]>0?1:(direction[axis]<0?-1:0);
        int cell=int(floor(coordinate[axis]));
        // Directional half-open ownership at exact negative entry boundaries.
        if(direction[axis]<0 && coordinate[axis]==float(cell))--cell;
        dda.cell[axis]=clamp(cell*size,lower[axis],upper[axis]-size);
    }
    voxel_dda_boundaries(dda);
    return dda;
}

float voxel_dda_exit(VoxelDda dda) {
    return min(dda.end,min(dda.next.x,min(dda.next.y,dda.next.z)));
}

bool voxel_dda_step(inout VoxelDda dda) {
    float time=min(dda.next.x,min(dda.next.y,dda.next.z));
    if(time>=dda.end || !isfinite(time))return false;
    dda.normal=0;
    // Simultaneous crossings skip zero-length edge/corner neighbors. Normal ties
    // choose X, then Y, then Z; no epsilon expands or erases a voxel interval.
    bool normalSet=false;
    for(uint axis=0;axis<3;++axis)if(dda.next[axis]==time && dda.step[axis]!=0) {
        dda.cell[axis]+=dda.step[axis]*dda.size;
        if(!normalSet) {dda.normal[axis]=-dda.step[axis];normalSet=true;}
    }
    dda.time=max(dda.time,time);
    voxel_dda_boundaries(dda);
    return normalSet;
}
#endif
