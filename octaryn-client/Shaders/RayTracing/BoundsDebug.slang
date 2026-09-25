struct ColumnBounds {float4 minimum;float4 maximum;};
StructuredBuffer<ColumnBounds> bounds;
uniform float4 camera[5];
struct Varying {float4 position:SV_Position;float3 color:COLOR0;};
[shader("vertex")]
Varying vertex_main(uint vertex:SV_VertexID,uint instance:SV_InstanceID) {
    const uint endpoints[24]={0,1,2,3,4,5,6,7,0,2,1,3,4,6,5,7,0,4,1,5,2,6,3,7};
    uint corner=endpoints[vertex];ColumnBounds box=bounds[instance];
    float3 p=lerp(box.minimum.xyz,box.maximum.xyz,float3(corner&1,(corner>>1)&1,(corner>>2)&1))-camera[0].xyz;
    float z=dot(p,camera[3].xyz);
    Varying output;
    output.position=float4(dot(p,camera[1].xyz)*camera[4].x,dot(p,camera[2].xyz)*camera[4].y,
        z*camera[4].z-camera[4].w,z);
    output.position.xy+=float2(camera[1].w,camera[2].w)*z;
    uint h=(instance+1)*747796405u+2891336453u;h=((h>>((h>>28)+4))^h)*277803737u;h=(h>>22)^h;
    output.color=.12+.88*float3(h&255,(h>>8)&255,(h>>16)&255)/255;
    return output;
}
[shader("fragment")]
float4 fragment_main(Varying input):SV_Target {return float4(input.color,1);}
