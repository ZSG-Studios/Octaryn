struct MatrixRows {float4 r0,r1,r2,r3;};
struct SourceVertex {
  float4 position,normal,tangent,uv,color;
  uint4 joints0,joints1;
  float4 weights0,weights1;
};
struct MorphDelta {float4 position,normal,tangent;};
struct DeformedVertex {
  float4 position,previousPosition,normal,tangent,uv,color;
};
StructuredBuffer<SourceVertex> sourceVertices;
StructuredBuffer<MorphDelta> morphDeltas;
StructuredBuffer<MatrixRows> currentJoints;
StructuredBuffer<MatrixRows> previousJoints;
StructuredBuffer<float> currentWeights;
StructuredBuffer<float> previousWeights;
RWStructuredBuffer<DeformedVertex> deformedVertices;
// Ordered-float minima xyz, maxima xyz, invalid-output flag. Separate buffers per primitive.
RWStructuredBuffer<uint> deformationBounds;
uniform uint vertexCount,morphCount,jointCount,dispatchWidth;

uint orderedFloat(float value) {
  uint bits=asuint(value);return (bits&0x80000000u)!=0?~bits:bits^0x80000000u;
}
[shader("compute")]
[numthreads(1,1,1)]
void clearBounds() {
  for(uint c=0;c<3;++c) {deformationBounds[c]=0xffffffffu;deformationBounds[c+3]=0;}
  deformationBounds[6]=0;
}
MatrixRows skinMatrix(SourceVertex vertex,bool previous) {
  MatrixRows matrix;
  matrix.r0=0;matrix.r1=0;matrix.r2=0;matrix.r3=0;
  if(jointCount==0) {matrix.r0.x=1;matrix.r1.y=1;matrix.r2.z=1;matrix.r3.w=1;return matrix;}
  float sum=0;
  for(uint i=0;i<8;++i) {
    uint joint=i<4?vertex.joints0[i]:vertex.joints1[i-4];
    float weight=i<4?vertex.weights0[i]:vertex.weights1[i-4];
    if(weight<=0)continue;
    if(joint>=jointCount) {InterlockedOr(deformationBounds[6],1);continue;}
    MatrixRows transform=previous?previousJoints[joint]:currentJoints[joint];
    matrix.r0+=transform.r0*weight;matrix.r1+=transform.r1*weight;
    matrix.r2+=transform.r2*weight;matrix.r3+=transform.r3*weight;sum+=weight;
  }
  if(sum>0) {matrix.r0/=sum;matrix.r1/=sum;matrix.r2/=sum;matrix.r3/=sum;}
  else InterlockedOr(deformationBounds[6],1);
  return matrix;
}
float3 transformPoint(MatrixRows matrix,float3 position) {
  float4 p=float4(position,1);return float3(dot(matrix.r0,p),dot(matrix.r1,p),dot(matrix.r2,p));
}
float3 transformDirection(MatrixRows matrix,float3 direction) {
  return float3(dot(matrix.r0.xyz,direction),dot(matrix.r1.xyz,direction),dot(matrix.r2.xyz,direction));
}
float3 safeUnit(float3 value,float3 fallback) {
  float length2=dot(value,value);return length2>1e-20&&isfinite(length2)?value*rsqrt(length2):fallback;
}
[shader("compute")]
[numthreads(64,1,1)]
void deform(uint3 thread:SV_DispatchThreadID) {
  uint index=thread.x+thread.y*dispatchWidth;if(index>=vertexCount)return;
  SourceVertex source=sourceVertices[index];
  float3 position=source.position.xyz,previousPosition=position;
  float3 normal=source.normal.xyz,tangent=source.tangent.xyz;
  for(uint target=0;target<morphCount;++target) {
    MorphDelta delta=morphDeltas[target*vertexCount+index];
    float weight=currentWeights[target];
    position+=delta.position.xyz*weight;previousPosition+=delta.position.xyz*previousWeights[target];
    normal+=delta.normal.xyz*weight;tangent+=delta.tangent.xyz*weight;
  }
  MatrixRows matrix=skinMatrix(source,false),previousMatrix=skinMatrix(source,true);
  position=transformPoint(matrix,position);previousPosition=transformPoint(previousMatrix,previousPosition);
  float3 cofactor0=cross(matrix.r1.xyz,matrix.r2.xyz);
  float determinant=dot(matrix.r0.xyz,cofactor0);
  if(abs(determinant)<1e-20) {InterlockedOr(deformationBounds[6],1);normal=source.normal.xyz;}
  else normal=float3(dot(cofactor0,normal),dot(cross(matrix.r2.xyz,matrix.r0.xyz),normal),dot(cross(matrix.r0.xyz,matrix.r1.xyz),normal))/determinant;
  normal=safeUnit(normal,float3(0,1,0));tangent=transformDirection(matrix,tangent);
  float3 tangentFallback=safeUnit(cross(abs(normal.y)<.9?float3(0,1,0):float3(1,0,0),normal),float3(1,0,0));
  tangent=safeUnit(tangent-normal*dot(normal,tangent),tangentFallback);
  if(!all(isfinite(position))||!all(isfinite(previousPosition))) {InterlockedOr(deformationBounds[6],1);return;}
  DeformedVertex output;
  output.position=float4(position,1);output.previousPosition=float4(previousPosition,1);
  output.normal=float4(normal,0);output.tangent=float4(tangent,source.tangent.w*(determinant<0?-1:1));
  output.uv=source.uv;output.color=source.color;deformedVertices[index]=output;
  for(uint c=0;c<3;++c) {
    InterlockedMin(deformationBounds[c],orderedFloat(min(position[c],previousPosition[c])));
    InterlockedMax(deformationBounds[c+3],orderedFloat(max(position[c],previousPosition[c])));
  }
}
