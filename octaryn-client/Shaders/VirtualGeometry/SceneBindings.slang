struct GeometrySceneDraw {
    uint cluster,transform,pageBase,materialBase;
    uint generation,padding0,padding1,padding2;
};
struct GeometrySceneInstance {
    float4 world[3];
    float4 normal[3];
    float orientation;
    uint padding0,padding1,padding2;
};
StructuredBuffer<GeometrySceneDraw> geometrySceneDraws;
StructuredBuffer<GeometrySceneInstance> geometrySceneInstances;
uniform uint geometrySceneFrame;
bool geometry_draw_valid(uint id) {
    if(id>=geometryExtent.z)return false;
    GeometrySceneDraw draw=geometrySceneDraws[id];
    uint clusters,clusterStride,instances,instanceStride;
    geometryClusters.GetDimensions(clusters,clusterStride);
    geometrySceneInstances.GetDimensions(instances,instanceStride);
    return draw.generation==geometrySceneFrame && draw.cluster<clusters && draw.transform<instances;
}
GeometryCluster geometry_cluster(uint id) {
    if(!geometry_draw_valid(id))return (GeometryCluster)0;
    GeometrySceneDraw draw=geometrySceneDraws[id];
    GeometryCluster cluster=geometryClusters[draw.cluster];
    cluster.page+=draw.pageBase;cluster.material+=draw.materialBase;return cluster;
}
float geometry_orientation(uint id) {return geometrySceneInstances[geometrySceneDraws[id].transform].orientation;}
MapVertex geometry_transform_vertex(MapVertex vertex,uint id) {
    GeometrySceneInstance instance=geometrySceneInstances[geometrySceneDraws[id].transform];
    return map_transform_vertex(vertex,instance.world[0],instance.world[1],instance.world[2],
        instance.normal[0],instance.normal[1],instance.normal[2],instance.orientation);
}
