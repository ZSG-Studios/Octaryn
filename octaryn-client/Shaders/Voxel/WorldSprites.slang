// Original voxel_common.glsl sprite position/UV contract.
void original_sprite(uint face,uint layer,float2 c,out float3 offset,out float2 uv) {
    if (layer>=17 && layer<=23) {
        float lo=7.0/16.0,hi=9.0/16.0,top=10.0/16.0;
        if (face==0) offset=float3(lerp(lo,hi,c.x),top*c.y,hi);
        else if (face==1) offset=float3(lerp(hi,lo,c.x),top*c.y,lo);
        else if (face==2) offset=float3(hi,top*c.y,lerp(hi,lo,c.x));
        else if (face==3) offset=float3(lo,top*c.y,lerp(lo,hi,c.x));
        else if (face==4) offset=float3(lerp(lo,hi,c.x),top,lerp(hi,lo,c.y));
        else offset=float3(lerp(lo,hi,c.x),0,lerp(lo,hi,c.y));
        float4 rect=float4(7,6,9,16)/16.0;
        if (face==4) rect=float4(7,6,9,8)/16.0;
        if (face==5) rect=float4(7,13,9,15)/16.0;
        uv=float2(lerp(rect.x,rect.z,c.x),lerp(rect.w,rect.y,c.y));
    } else {
        offset=face<2?float3(1-c.x,1-c.y,1-c.x):float3(1-c.x,1-c.y,c.x);
        uv=c;
    }
}
