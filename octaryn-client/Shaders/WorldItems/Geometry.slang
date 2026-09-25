#ifndef OCTARYN_ITEM_GEOMETRY
#define OCTARYN_ITEM_GEOMETRY
float3 item_face_normal(uint face,bool sprite) {
    const float3 normals[6]={float3(-1,0,0),float3(1,0,0),float3(0,-1,0),float3(0,1,0),float3(0,0,-1),float3(0,0,1)};
    return normals[sprite?5:face];
}
float3 item_face_point(uint face,float2 uv,bool sprite) {
    if(sprite)return float3(uv.x,uv.y,.5)-.5;
    if(face==0)return float3(0,uv.y,uv.x)-.5;
    if(face==1)return float3(1,uv.y,1-uv.x)-.5;
    if(face==2)return float3(uv.x,0,uv.y)-.5;
    if(face==3)return float3(uv.x,1,1-uv.y)-.5;
    if(face==4)return float3(1-uv.x,uv.y,0)-.5;
    return float3(uv.x,uv.y,1)-.5;
}
float3 item_rotate(float3 p,float angle) {
    return float3(cos(angle)*p.x-sin(angle)*p.z,p.y,sin(angle)*p.x+cos(angle)*p.z);
}
float3 item_offset(float phase,float3 local,float time) {
    return item_rotate(local,time*1.6+phase)*.25+float3(0,.1+.025*sin(time*2.5+phase),0);
}
#endif
