#include "../../../octaryn-client/Shaders/Hdr/ScreenGrid.slang"
StructuredBuffer<float4> cases;
RWStructuredBuffer<int2> cells;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    float4 c=cases[id.x];ScreenGrid grid=screen_grid_begin(c.xy,c.zw);
    for(uint i=0;i<96;++i) {
        cells[id.x*96+i]=grid.cell;
        if(screen_grid_exit(grid)>=1)break;
        screen_grid_advance(grid);
    }
}
