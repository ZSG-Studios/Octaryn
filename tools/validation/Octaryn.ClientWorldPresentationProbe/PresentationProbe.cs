internal static class PresentationProbe
{
    public static int Run()
    {
        StoreValidation.Validate();
        NeighborhoodValidation.ValidateSnapshot();
        RenderRulesValidation.Validate();
        NeighborhoodValidation.ValidateFaceVisibility();
        MeshPlanningValidation.ValidateChunkMeshPlanner();
        MeshPackingValidation.ValidatePackedChunkMesh();
        MeshPlanningValidation.ValidateNonFluidPlannerToPackerPipeline();
        MeshPackingValidation.ValidateNonFluidUploadPlan();
        SnapshotConsumerValidation.ValidateTickOrder();
        return 0;
    }
}
