namespace Octaryn.Client.WorldPresentation;

internal static class PackedMeshUploadValidator
{
    public static PackedMeshUploadPlan CreateNonFluidPlan(PackedChunkMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        ArgumentNullException.ThrowIfNull(mesh.OpaqueCubeFaces);
        ArgumentNullException.ThrowIfNull(mesh.TransparentCubeFaces);
        ArgumentNullException.ThrowIfNull(mesh.SpriteVertices);
        ArgumentNullException.ThrowIfNull(mesh.FluidBlocks);

        if (mesh.FluidBlocks.Count > 0)
        {
            throw new InvalidOperationException("Non-fluid upload plans cannot contain deferred fluid blocks.");
        }

        if (mesh.SpriteVertices.Count % 4 != 0)
        {
            throw new InvalidOperationException("Packed sprite vertex count must be divisible by four.");
        }

        return new PackedMeshUploadPlan(
            mesh.OpaqueCubeFaces.Count,
            mesh.TransparentCubeFaces.Count,
            mesh.SpriteVertices.Count,
            spriteIndexCount: mesh.SpriteVertices.Count / 4 * 6);
    }
}
