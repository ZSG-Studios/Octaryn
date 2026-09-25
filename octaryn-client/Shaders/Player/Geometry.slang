#ifndef OCTARYN_PLAYER_GEOMETRY
#define OCTARYN_PLAYER_GEOMETRY
float3 player_rotate(float3 p,float2 rotation) {
    return float3(rotation.x*p.x-rotation.y*p.z,p.y,rotation.y*p.x+rotation.x*p.z);
}
void player_skinned_geometry(float3 c0,float3 c1,float3 c2,float3 c3,float3 position,float3 normal,
    float2 rotation,out float3 offset,out float3 direction) {
    offset=player_rotate(c0*position.x+c1*position.y+c2*position.z+c3,rotation);
    float determinant=dot(c0,cross(c1,c2));
    direction=cross(c1,c2)*normal.x+cross(c2,c0)*normal.y+cross(c0,c1)*normal.z;
    direction=normalize(player_rotate(direction*(determinant<0?-1:1),rotation));
}
#endif
