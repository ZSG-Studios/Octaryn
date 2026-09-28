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
[shader("compute")]
[numthreads(128,1,1)]
void expandRayGeometry(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
  if(group.x>=copyCount)return;
  CopyCluster copy=copies[group.x];uint4 page=pageTable[copy.page];
  bool valid=page.x==copy.slot&&page.y==copy.generation&&page.z!=0;
  if(!valid)InterlockedOr(validation[0],1);
  if(lane<copy.vertexCount) {
    uint source=copy.slot*65536+copy.sourceVertex+lane*80;
    uint destination=(copy.firstVertex+lane)*80;
    for(uint block=0;block<5;++block)rayVertices.Store4(destination+block*16,valid?pagePool.Load4(source+block*16):uint4(0));
  }
  if(lane<copy.triangleCount) {
    uint packed=valid?pagePool.Load(copy.slot*65536+copy.sourceTriangle+lane*4):0;
    uint3 local=uint3(packed&255,(packed>>8)&255,(packed>>16)&255);
    if(any(local>=copy.vertexCount)) {local=0;InterlockedOr(validation[0],2);}
    uint triangle=copy.firstTriangle+lane;
    rayIndices[triangle*3]=copy.firstVertex+local.x;rayIndices[triangle*3+1]=copy.firstVertex+local.y;rayIndices[triangle*3+2]=copy.firstVertex+local.z;
    triangleRecords[triangle]=uint4(copy.cluster,lane,copy.material,copy.flags);
  }
}
