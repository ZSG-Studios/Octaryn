struct ScreenGrid {
    int2 cell,step;
    float2 next,stride;
    float entered;
};
ScreenGrid screen_grid_begin(float2 start,float2 end) {
    ScreenGrid grid;float2 delta=end-start;
    grid.cell=int2(floor(start));grid.step=int2(delta.x>=0?1:-1,delta.y>=0?1:-1);
    grid.stride=float2(abs(delta.x)>1e-8?1/abs(delta.x):1e30,abs(delta.y)>1e-8?1/abs(delta.y):1e30);
    float2 boundary=float2(grid.step.x>0?grid.cell.x+1:grid.cell.x,grid.step.y>0?grid.cell.y+1:grid.cell.y);
    grid.next=abs(boundary-start)*grid.stride;grid.entered=0;
    if(abs(delta.x)<=1e-8)grid.next.x=1e30;
    if(abs(delta.y)<=1e-8)grid.next.y=1e30;
    return grid;
}
float screen_grid_exit(ScreenGrid grid) {return min(1.f,min(grid.next.x,grid.next.y));}
void screen_grid_advance(inout ScreenGrid grid) {
    grid.entered=screen_grid_exit(grid);
    bool x=grid.next.x<=grid.next.y,y=grid.next.y<=grid.next.x;
    if(x){grid.cell.x+=grid.step.x;grid.next.x+=grid.stride.x;}
    if(y){grid.cell.y+=grid.step.y;grid.next.y+=grid.stride.y;}
}
