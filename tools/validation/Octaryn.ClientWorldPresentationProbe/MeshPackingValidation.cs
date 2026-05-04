using System.Runtime.InteropServices;
using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.World;

internal static class MeshPackingValidation
{
    public static void ValidatePackedChunkMesh()
    {
        var rules = new ClientBlockRenderRules();
        var packer = new ClientChunkMeshPacker(rules);

        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(1), Direction.PositiveY) == 1, "grass top atlas layer");
        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(1), Direction.NegativeY) == 3, "grass bottom atlas layer");
        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(6), Direction.PositiveX) == 8, "log side atlas layer");
        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(6), Direction.PositiveY) == 7, "log top atlas layer");

        ProbeAssertions.Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.PositiveZ) == 0, "positive z maps to old north");
        ProbeAssertions.Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.NegativeZ) == 1, "negative z maps to old south");
        ProbeAssertions.Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.PositiveX) == 2, "positive x maps to old east");
        ProbeAssertions.Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.NegativeX) == 3, "negative x maps to old west");
        ProbeAssertions.Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.PositiveY) == 4, "positive y maps to old up");
        ProbeAssertions.Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.NegativeY) == 5, "negative y maps to old down");

        var boundaryFace = new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 31, 255, 31, Direction.PositiveY);
        var packedBoundaryFace = ClientPackedCubeFace.Pack(boundaryFace, rules);
        ProbeAssertions.Require(ClientPackedCubeFace.X(packedBoundaryFace) == 31, "packed cube x field");
        ProbeAssertions.Require(ClientPackedCubeFace.Y(packedBoundaryFace) == 255, "packed cube y field");
        ProbeAssertions.Require(ClientPackedCubeFace.Z(packedBoundaryFace) == 31, "packed cube z field");
        ProbeAssertions.Require(ClientPackedCubeFace.Direction(packedBoundaryFace) == 4, "packed cube direction field");
        ProbeAssertions.Require(ClientPackedCubeFace.UExtent(packedBoundaryFace) == 1, "packed cube u extent");
        ProbeAssertions.Require(ClientPackedCubeFace.VExtent(packedBoundaryFace) == 1, "packed cube v extent");
        ProbeAssertions.Require(ClientPackedCubeFace.AtlasLayer(packedBoundaryFace) == 1, "packed cube atlas layer");
        ProbeAssertions.Require(ClientPackedCubeFace.HasOcclusion(packedBoundaryFace), "packed cube occlusion bit");
        ProbeAssertions.Require(ClientPackedCubeFace.ChunkSlot(packedBoundaryFace) == ClientPackedCubeFace.UnsetChunkSlot, "packed cube unset chunk slot");
        ProbeAssertions.Require(ClientPackedCubeFace.WaterLevel(packedBoundaryFace) == 0, "packed cube water level default");
        ProbeAssertions.Require(!ClientPackedCubeFace.IsWater(packedBoundaryFace), "packed cube water flag default");
        ProbeAssertions.Require(ClientPackedCubeFace.WaterBaseHeight(packedBoundaryFace) == 0, "packed cube water base default");

        var glassFace = new ClientCubeMeshFace(new BlockId(30), ClientBlockRenderKind.TransparentCube, 4, 4, 4, Direction.NegativeZ);
        var packedGlassFace = ClientPackedCubeFace.Pack(glassFace, rules);
        ProbeAssertions.Require(ClientPackedCubeFace.Direction(packedGlassFace) == 1, "packed glass direction field");
        ProbeAssertions.Require(ClientPackedCubeFace.AtlasLayer(packedGlassFace) == 25, "packed glass atlas layer");
        ProbeAssertions.Require(!ClientPackedCubeFace.HasOcclusion(packedGlassFace), "packed glass occlusion bit");

        var isolatedFaces = new[]
        {
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveZ),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeZ),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveX),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeX),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveY),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeY)
        };
        var packedIsolatedFaces = packer.Pack(new ClientChunkMeshPlan(isolatedFaces, [], []));
        ProbeAssertions.Require(packedIsolatedFaces.OpaqueCubeFaces.Select(ClientPackedCubeFace.Direction).SequenceEqual([0, 1, 2, 3, 4, 5]), "packed cube face sequence matches old order");

        var spriteFace = new ClientSpriteMeshFace(new BlockId(9), 8, 4, 8, Direction.PositiveZ);
        var packedSprite0 = ClientPackedSpriteVertex.Pack(spriteFace, rules, 0);
        var packedSprite1 = ClientPackedSpriteVertex.Pack(spriteFace, rules, 1);
        ProbeAssertions.Require(ClientPackedSpriteVertex.PackedDirection(packedSprite0) == 6, "packed sprite direction starts after cube directions");
        ProbeAssertions.Require(ClientPackedSpriteVertex.AtlasLayer(packedSprite0) == 15, "packed sprite atlas layer");
        ProbeAssertions.Require(ClientPackedSpriteVertex.U(packedSprite0) == 1, "packed sprite first u");
        ProbeAssertions.Require(ClientPackedSpriteVertex.V(packedSprite0) == 1, "packed sprite first v");
        ProbeAssertions.Require(ClientPackedSpriteVertex.U(packedSprite1) == 1, "packed sprite second u");
        ProbeAssertions.Require(ClientPackedSpriteVertex.V(packedSprite1) == 0, "packed sprite second v");

        var plan = new ClientChunkMeshPlan(
            [boundaryFace],
            [
                new ClientSpriteMeshFace(new BlockId(9), 8, 4, 8, Direction.PositiveZ),
                new ClientSpriteMeshFace(new BlockId(22), 10, 4, 10, Direction.NegativeY)
            ],
            [new ClientFluidMeshBlock(new BlockId(14), ClientBlockRenderKind.Water, 12, 4, 12, 0)]);
        var packed = packer.Pack(plan);
        var repeatedPacked = packer.Pack(plan);
        ProbeAssertions.Require(packed.OpaqueCubeFaces.Count == 1, "packer emits opaque cube bucket");
        ProbeAssertions.Require(packed.TransparentCubeFaces.Count == 0, "packer keeps transparent bucket empty");
        ProbeAssertions.Require(packed.SpriteVertices.Count == 8, "packer emits four vertices per sprite face");
        ProbeAssertions.Require(packed.FluidBlocks.Count == 1, "packer preserves deferred fluid blocks");
        ProbeAssertions.Require(packed.OpaqueCubeFaces.SequenceEqual(repeatedPacked.OpaqueCubeFaces), "packed opaque output is deterministic");
        ProbeAssertions.Require(packed.TransparentCubeFaces.SequenceEqual(repeatedPacked.TransparentCubeFaces), "packed transparent output is deterministic");
        ProbeAssertions.Require(packed.SpriteVertices.SequenceEqual(repeatedPacked.SpriteVertices), "packed sprite output is deterministic");
        ProbeAssertions.Require(packed.FluidBlocks.SequenceEqual(repeatedPacked.FluidBlocks), "packed fluid output is deterministic");

        var transparentPlan = new ClientChunkMeshPlan([glassFace], [], []);
        var packedTransparent = packer.Pack(transparentPlan);
        ProbeAssertions.Require(packedTransparent.OpaqueCubeFaces.Count == 0, "packer keeps glass out of opaque bucket");
        ProbeAssertions.Require(packedTransparent.TransparentCubeFaces.Count == 1, "packer emits transparent cube bucket");
    }

    public static void ValidateNonFluidUploadPlan()
    {
        ProbeAssertions.Require(Marshal.SizeOf<ClientPackedMeshUploadDescriptor>() == ClientPackedMeshUploadDescriptor.SizeValue, "upload descriptor managed ABI size");
        ProbeAssertions.Require(Marshal.SizeOf<ClientChunkMeshUploadRecord>() == ClientChunkMeshUploadRecord.SizeValue, "chunk mesh upload record managed ABI size");

        var empty = ClientPackedMeshUploadValidator.CreateNonFluidPlan(new ClientPackedChunkMesh([], [], [], []));
        ProbeAssertions.Require(empty.OpaqueFaceCount == 0, "empty upload opaque count");
        ProbeAssertions.Require(empty.TransparentFaceCount == 0, "empty upload transparent count");
        ProbeAssertions.Require(empty.SpriteVertexCount == 0, "empty upload sprite vertex count");
        ProbeAssertions.Require(empty.SpriteIndexCount == 0, "empty upload sprite index count");
        ProbeAssertions.Require(empty.ClearsOpaqueFaces, "empty upload clears opaque");
        ProbeAssertions.Require(empty.ClearsTransparentFaces, "empty upload clears transparent");
        ProbeAssertions.Require(empty.ClearsSpriteVertices, "empty upload clears sprites");
        ProbeAssertions.Require(!empty.RequiresUploadSubmit, "empty upload does not require submit");
        ValidateEmptyDescriptor(empty.ToDescriptor());

        var rules = new ClientBlockRenderRules();
        var planner = new ClientChunkMeshPlanner(rules);
        var packer = new ClientChunkMeshPacker(rules);
        var store = new ClientBlockPresentationStore();
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 6, 1), new BlockId(1)), "upload plan opaque block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(4, 6, 4), new BlockId(30)), "upload plan transparent block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(8, 6, 8), new BlockId(9)), "upload plan sprite block");

        var packed = packer.Pack(planner.Build(store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            ClientNeighborhoodBoundaryBlocks.Air)));
        var upload = ClientPackedMeshUploadValidator.CreateNonFluidPlan(packed);
        ValidateUploadPlan(packed, upload);
        ValidateChunkUploadRecord(packed, upload);

        ProbeAssertions.RequireThrows<InvalidOperationException>(
            () => ClientPackedMeshUploadValidator.CreateNonFluidPlan(new ClientPackedChunkMesh([], [], [1, 2, 3], [])),
            "upload plan rejects incomplete sprite quad");
        ProbeAssertions.RequireThrows<InvalidOperationException>(
            () => ClientPackedMeshUploadValidator.CreateNonFluidPlan(new ClientPackedChunkMesh(
                [],
                [],
                [],
                [new ClientFluidMeshBlock(new BlockId(14), ClientBlockRenderKind.Water, 1, 2, 3, 0)])),
            "upload plan rejects fluids");
    }

    private static void ValidateEmptyDescriptor(ClientPackedMeshUploadDescriptor emptyDescriptor)
    {
        ProbeAssertions.Require(emptyDescriptor.Version == ClientPackedMeshUploadDescriptor.VersionValue, "empty upload descriptor version");
        ProbeAssertions.Require(emptyDescriptor.Size == ClientPackedMeshUploadDescriptor.SizeValue, "empty upload descriptor size");
        ProbeAssertions.Require(emptyDescriptor.OpaqueFaceCount == 0, "empty upload descriptor opaque count");
        ProbeAssertions.Require(emptyDescriptor.TransparentFaceCount == 0, "empty upload descriptor transparent count");
        ProbeAssertions.Require(emptyDescriptor.SpriteVertexCount == 0, "empty upload descriptor sprite vertex count");
        ProbeAssertions.Require(emptyDescriptor.SpriteIndexCount == 0, "empty upload descriptor sprite index count");
        ProbeAssertions.Require(emptyDescriptor.OpaqueByteCount == 0, "empty upload descriptor opaque bytes");
        ProbeAssertions.Require(emptyDescriptor.TransparentByteCount == 0, "empty upload descriptor transparent bytes");
        ProbeAssertions.Require(emptyDescriptor.SpriteByteCount == 0, "empty upload descriptor sprite bytes");
        ProbeAssertions.Require(emptyDescriptor.Flags == (
            ClientPackedMeshUploadDescriptor.ClearOpaqueFacesFlag |
            ClientPackedMeshUploadDescriptor.ClearTransparentFacesFlag |
            ClientPackedMeshUploadDescriptor.ClearSpriteVerticesFlag), "empty upload descriptor clear flags");
    }

    private static void ValidateUploadPlan(ClientPackedChunkMesh packed, ClientPackedMeshUploadPlan upload)
    {
        ProbeAssertions.Require(upload.OpaqueFaceCount == packed.OpaqueCubeFaces.Count, "upload plan opaque count");
        ProbeAssertions.Require(upload.TransparentFaceCount == packed.TransparentCubeFaces.Count, "upload plan transparent count");
        ProbeAssertions.Require(upload.SpriteVertexCount == packed.SpriteVertices.Count, "upload plan sprite vertex count");
        ProbeAssertions.Require(upload.SpriteIndexCount == 24, "upload plan sprite index count");
        ProbeAssertions.Require(!upload.ClearsOpaqueFaces, "upload plan keeps opaque");
        ProbeAssertions.Require(!upload.ClearsTransparentFaces, "upload plan keeps transparent");
        ProbeAssertions.Require(!upload.ClearsSpriteVertices, "upload plan keeps sprites");
        ProbeAssertions.Require(upload.RequiresUploadSubmit, "upload plan requires submit");
        ProbeAssertions.Require(upload.OpaqueByteCount == (ulong)packed.OpaqueCubeFaces.Count * sizeof(ulong), "upload plan opaque byte count");
        ProbeAssertions.Require(upload.TransparentByteCount == (ulong)packed.TransparentCubeFaces.Count * sizeof(ulong), "upload plan transparent byte count");
        ProbeAssertions.Require(upload.SpriteByteCount == (ulong)packed.SpriteVertices.Count * sizeof(uint), "upload plan sprite byte count");

        var descriptor = upload.ToDescriptor();
        ProbeAssertions.Require(descriptor.OpaqueFaceCount == (uint)upload.OpaqueFaceCount, "upload descriptor opaque count");
        ProbeAssertions.Require(descriptor.TransparentFaceCount == (uint)upload.TransparentFaceCount, "upload descriptor transparent count");
        ProbeAssertions.Require(descriptor.SpriteVertexCount == (uint)upload.SpriteVertexCount, "upload descriptor sprite vertex count");
        ProbeAssertions.Require(descriptor.SpriteIndexCount == (uint)upload.SpriteIndexCount, "upload descriptor sprite index count");
        ProbeAssertions.Require(descriptor.OpaqueByteCount == upload.OpaqueByteCount, "upload descriptor opaque bytes");
        ProbeAssertions.Require(descriptor.TransparentByteCount == upload.TransparentByteCount, "upload descriptor transparent bytes");
        ProbeAssertions.Require(descriptor.SpriteByteCount == upload.SpriteByteCount, "upload descriptor sprite bytes");
        ProbeAssertions.Require(descriptor.Flags == 0, "upload descriptor has no clear flags");
    }

    private static void ValidateChunkUploadRecord(ClientPackedChunkMesh packed, ClientPackedMeshUploadPlan upload)
    {
        var record = ClientChunkMeshUploadRecord.Create(
            new ClientPresentationChunkKey(1, 2, 3),
            packed,
            opaqueFaceOffset: 4,
            transparentFaceOffset: 8,
            spriteVertexOffset: 12);
        ProbeAssertions.Require(record.Version == ClientChunkMeshUploadRecord.VersionValue, "chunk upload record version");
        ProbeAssertions.Require(record.Size == ClientChunkMeshUploadRecord.SizeValue, "chunk upload record size");
        ProbeAssertions.Require(record.ChunkX == 1 && record.ChunkY == 2 && record.ChunkZ == 3, "chunk upload record key");
        ProbeAssertions.Require(record.OpaqueFaceCount == (uint)packed.OpaqueCubeFaces.Count, "chunk upload record opaque count");
        ProbeAssertions.Require(record.TransparentFaceCount == (uint)packed.TransparentCubeFaces.Count, "chunk upload record transparent count");
        ProbeAssertions.Require(record.SpriteVertexCount == (uint)packed.SpriteVertices.Count, "chunk upload record sprite count");
        ProbeAssertions.Require(record.SpriteIndexCount == 24, "chunk upload record sprite indices");
        ProbeAssertions.Require(record.FluidBlockCount == 0, "chunk upload record fluid count");
        ProbeAssertions.Require(record.OpaqueFaceOffset == 4, "chunk upload record opaque offset");
        ProbeAssertions.Require(record.TransparentFaceOffset == 8, "chunk upload record transparent offset");
        ProbeAssertions.Require(record.SpriteVertexOffset == 12, "chunk upload record sprite offset");
        ProbeAssertions.Require(record.OpaqueByteCount == upload.OpaqueByteCount, "chunk upload record opaque bytes");
        ProbeAssertions.Require(record.TransparentByteCount == upload.TransparentByteCount, "chunk upload record transparent bytes");
        ProbeAssertions.Require(record.SpriteByteCount == upload.SpriteByteCount, "chunk upload record sprite bytes");
        ProbeAssertions.Require(record.Flags == ClientChunkMeshUploadRecord.ClearFluidBlocksFlag, "chunk upload record fluid clear flag");
    }
}
