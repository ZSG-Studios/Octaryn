struct CopyCluster {
  uint cluster,page,slot,generation,sourceVertex,sourceTriangle,vertexCount,triangleCount;
  uint firstVertex,firstTriangle,material,flags;
};
StructuredBuffer<CopyCluster> copies;
StructuredBuffer<uint4> pageTable;
ByteAddressBuffer pagePool;
RWByteAddressBuffer rayVertices;
RWStructuredBuffer<uint> rayIndices;
RWStructuredBuffer<uint4> triangleRecords;
RWStructuredBuffer<uint> validation;
uniform uint copyCount;
uniform uint vertexStride;
[shader("compute")]
[numthreads(128,1,1)]
void expandRayGeometry(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
  if(group.x>=copyCount)return;
  CopyCluster copy=copies[group.x];uint4 page=pageTable[copy.page];
  uint sourceStride=(copy.flags&512u)!=0?12:112;
  bool valid=page.x==copy.slot&&page.y==copy.generation&&page.z!=0&&page.w<65536&&(page.w&15u)==0;
  uint remaining=valid?65536-page.w:0;
  valid=valid&&copy.sourceVertex<=remaining&&copy.sourceTriangle<=remaining;
  valid=valid&&copy.vertexCount<=(remaining-min(copy.sourceVertex,remaining))/sourceStride&&
      copy.triangleCount<=(remaining-min(copy.sourceTriangle,remaining))/4;
  if(!valid)InterlockedOr(validation[0],1);
  if(lane<copy.vertexCount) {
    uint destination=(copy.firstVertex+lane)*vertexStride;
    if(vertexStride==12) {
      uint source=copy.slot*65536+page.w+copy.sourceVertex+lane*12;
      rayVertices.Store3(destination,valid?pagePool.Load3(source):uint3(0));
    } else if((copy.flags&512u)!=0) {
      uint source=copy.slot*65536+page.w+copy.sourceVertex+lane*12;
      for(uint block=0;block<6;++block)rayVertices.Store4(destination+block*16,uint4(0));
      rayVertices.Store2(destination+96,uint2(0));
      rayVertices.Store3(destination,valid?pagePool.Load3(source):uint3(0));
      rayVertices.Store4(destination+56,asuint(float4(1)));
      rayVertices.Store4(destination+72,asuint(float4(1,0,0,0)));
      // Zero normals select the geometric triangle normal in ray hit shading.
    } else {
      uint source=copy.slot*65536+page.w+copy.sourceVertex+lane*112;
      for(uint block=0;block<2;++block)rayVertices.Store4(destination+block*16,valid?pagePool.Load4(source+block*16):uint4(0));
      rayVertices.Store2(destination+32,valid?pagePool.Load2(source+32):uint2(0));
      rayVertices.Store4(destination+40,valid?pagePool.Load4(source+48):uint4(0));
      rayVertices.Store4(destination+56,valid?pagePool.Load4(source+64):uint4(0));
      rayVertices.Store4(destination+72,valid?pagePool.Load4(source+80):uint4(0));
      rayVertices.Store4(destination+88,valid?pagePool.Load4(source+96):uint4(0));
    }
  }
  if(lane<copy.triangleCount) {
    uint packed=valid?pagePool.Load(copy.slot*65536+page.w+copy.sourceTriangle+lane*4):0;
    uint3 local=uint3(packed&255,(packed>>8)&255,(packed>>16)&255);
    if(any(local>=copy.vertexCount)) {local=0;InterlockedOr(validation[0],2);}
    uint triangle=copy.firstTriangle+lane;
    rayIndices[triangle*3]=copy.firstVertex+local.x;rayIndices[triangle*3+1]=copy.firstVertex+local.y;rayIndices[triangle*3+2]=copy.firstVertex+local.z;
    triangleRecords[triangle]=uint4(copy.cluster,lane,copy.material,copy.flags);
  }
}
