// Original world/chunks/build_mesh.cpp fluid corner weighting and downhill flow, on GPU.
#include "WorldFluidTypes.slang"
RWStructuredBuffer<FluidFace> fluidFaces;
bool sameFluid(uint a,uint b) {
  uint x=blockMaterials[a].flags,y=blockMaterials[b].flags;
  return (x&4)!=0 && (y&4)!=0 && (x&512)==(y&512);
}
float fluidSample(uint center,int3 p,int height) {
  uint block=blockAt(p,height);
  if(!sameFluid(center,block)) return (blockMaterials[block].flags&1024)!=0?-1:0;
  if(sameFluid(center,blockAt(p+int3(0,1,0),height))) return 1;
  return float(clamp(8-int(blockMaterials[block].fluidLevel),1,8))/9;
}
float fluidCorner(uint block,int3 p,int height,int dx,int dz) {
  float self=fluidSample(block,p,height),x=fluidSample(block,p+int3(dx,0,0),height);
  float z=fluidSample(block,p+int3(0,0,dz),height);
  if(x>=1 || z>=1) return 1;
  float diagonal=-1;
  if(x>0 || z>0) {
    diagonal=fluidSample(block,p+int3(dx,0,dz),height);if(diagonal>=1) return 1;
  }
  float4 values=float4(self,x,z,diagonal);
  float4 weights=select(values>=.8,10.0,select(values>=0,1.0,0.0));
  return dot(values,weights)/max(dot(weights,1.0),1.0);
}
FluidFace originalFluidFace(uint block,int3 p,int height) {
  FluidFace result;
  result.heights=float4(fluidCorner(block,p,height,-1,-1),fluidCorner(block,p,height,1,-1),
      fluidCorner(block,p,height,1,1),fluidCorner(block,p,height,-1,1));
  float center=fluidSample(block,p,height);float2 flow=0;
  const int2 offsets[4]={int2(1,0),int2(-1,0),int2(0,1),int2(0,-1)};
  for(uint i=0;i<4;++i) {
    int3 q=p+int3(offsets[i].x,0,offsets[i].y);uint neighbor=blockAt(q,height);float pull=0;
    if(sameFluid(block,neighbor)) pull=center-fluidSample(block,q,height);
    else if((blockMaterials[neighbor].flags&1)==0 && sameFluid(block,blockAt(q-int3(0,1,0),height)))
      pull=center-(fluidSample(block,q-int3(0,1,0),height)-8.0/9.0);
    flow+=float2(offsets[i])*pull;
  }
  float magnitude=length(flow);
  result.flow=magnitude>.0001?float4(flow/magnitude,clamp(floor(magnitude*15+.5),1,15),0):0;
  result.flow.w=sameFluid(block,blockAt(p+int3(0,1,0),height))?1:0;
  return result;
}
