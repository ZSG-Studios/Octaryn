// Hierarchical-Z pyramid builder. Stores the farthest (maximum) depth of each
// 2x2 texel group; the map cull shader treats a box as occluded when its
// nearest depth is greater than the pyramid value over its screen rect.
// Out-of-bounds source reads return 0 (nearest), which never wins the max,
// so odd-sized mips shrink the rect instead of over-occluding.
Texture2D<float> hizSource;
RWTexture2D<float> hizTarget;
[shader("compute")]
[numthreads(8,8,1)]
void hiz_main(uint2 id:SV_DispatchThreadID) {
    uint width,height;
    hizTarget.GetDimensions(width,height);
    if(id.x>=width||id.y>=height)return;
    int2 base=int2(id)*2;
    float d=hizSource[base];
    d=max(d,hizSource[base+int2(1,0)]);
    d=max(d,hizSource[base+int2(0,1)]);
    d=max(d,hizSource[base+int2(1,1)]);
    hizTarget[id]=d;
}
