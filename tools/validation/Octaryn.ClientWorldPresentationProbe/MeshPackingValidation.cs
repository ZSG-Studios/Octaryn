using System.Runtime.InteropServices;
using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.World;

internal static class MeshPackingValidation
{
    public static void ValidatePackedChunkMesh()
    {
        var rules = new BlockRenderRules();
        var packer = new ChunkMeshPacker(rules);

        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(1), Direction.PositiveY) == 1, "grass top atlas layer");
        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(1), Direction.NegativeY) == 3, "grass bottom atlas layer");
        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(6), Direction.PositiveX) == 8, "log side atlas layer");
        ProbeAssertions.Require(rules.AtlasLayer(new BlockId(6), Direction.PositiveY) == 7, "log top atlas layer");

        ProbeAssertions.Require((int)PackedMeshDirectionMap.FromDirection(Direction.PositiveZ) == 0, "positive z maps to old north");
        ProbeAssertions.Require((int)PackedMeshDirectionMap.FromDirection(Direction.NegativeZ) == 1, "negative z maps to old south");
        ProbeAssertions.Require((int)PackedMeshDirectionMap.FromDirection(Direction.PositiveX) == 2, "positive x maps to old east");
        ProbeAssertions.Require((int)PackedMeshDirectionMap.FromDirection(Direction.NegativeX) == 3, "negative x maps to old west");
        ProbeAssertions.Require((int)PackedMeshDirectionMap.FromDirection(Direction.PositiveY) == 4, "positive y maps to old up");
        ProbeAssertions.Require((int)PackedMeshDirectionMap.FromDirection(Direction.NegativeY) == 5, "negative y maps to old down");

        var boundaryFace = new CubeMeshFace(new BlockId(1), BlockRenderKind.OpaqueCube, 31, 255, 31, Direction.PositiveY);
        var packedBoundaryFace = PackedCubeFace.Pack(boundaryFace, rules);
        ProbeAssertions.Require(PackedCubeFace.X(packedBoundaryFace) == 31, "packed cube x field");
        ProbeAssertions.Require(PackedCubeFace.Y(packedBoundaryFace) == 255, "packed cube y field");
        ProbeAssertions.Require(PackedCubeFace.Z(packedBoundaryFace) == 31, "packed cube z field");
        ProbeAssertions.Require(PackedCubeFace.Direction(packedBoundaryFace) == 4, "packed cube direction field");
        ProbeAssertions.Require(PackedCubeFace.UExtent(packedBoundaryFace) == 1, "packed cube u extent");
        ProbeAssertions.Require(PackedCubeFace.VExtent(packedBoundaryFace) == 1, "packed cube v extent");
        ProbeAssertions.Require(PackedCubeFace.AtlasLayer(packedBoundaryFace) == 1, "packed cube atlas layer");
        ProbeAssertions.Require(PackedCubeFace.HasOcclusion(packedBoundaryFace), "packed cube occlusion bit");
        ProbeAssertions.Require(PackedCubeFace.ChunkSlot(packedBoundaryFace) == PackedCubeFace.UnsetChunkSlot, "packed cube unset chunk slot");
        ProbeAssertions.Require(PackedCubeFace.WaterLevel(packedBoundaryFace) == 0, "packed cube water level default");
        ProbeAssertions.Require(!PackedCubeFace.IsWater(packedBoundaryFace), "packed cube water flag default");
        ProbeAssertions.Require(PackedCubeFace.WaterBaseHeight(packedBoundaryFace) == 0, "packed cube water base default");

        var glassFace = new CubeMeshFace(new BlockId(30), BlockRenderKind.TransparentCube, 4, 4, 4, Direction.NegativeZ);
        var packedGlassFace = PackedCubeFace.Pack(glassFace, rules);
        ProbeAssertions.Require(PackedCubeFace.Direction(packedGlassFace) == 1, "packed glass direction field");
        ProbeAssertions.Require(PackedCubeFace.AtlasLayer(packedGlassFace) == 25, "packed glass atlas layer");
        ProbeAssertions.Require(!PackedCubeFace.HasOcclusion(packedGlassFace), "packed glass occlusion bit");

        var isolatedFaces = new[]
        {
            new CubeMeshFace(new BlockId(1), BlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveZ),
            new CubeMeshFace(new BlockId(1), BlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeZ),
            new CubeMeshFace(new BlockId(1), BlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveX),
            new CubeMeshFace(new BlockId(1), BlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeX),
            new CubeMeshFace(new BlockId(1), BlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveY),
            new CubeMeshFace(new BlockId(1), BlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeY)
        };
        var packedIsolatedFaces = packer.Pack(new ChunkMeshPlan(isolatedFaces, [], []));
        ProbeAssertions.Require(packedIsolatedFaces.OpaqueCubeFaces.Select(PackedCubeFace.Direction).SequenceEqual([0, 1, 2, 3, 4, 5]), "packed cube face sequence matches old order");

        var spriteFace = new SpriteMeshFace(new BlockId(9), 8, 4, 8, Direction.PositiveZ);
        var packedSprite0 = PackedSpriteVertex.Pack(spriteFace, rules, 0);
        var packedSprite1 = PackedSpriteVertex.Pack(spriteFace, rules, 1);
        ProbeAssertions.Require(PackedSpriteVertex.PackedDirection(packedSprite0) == 6, "packed sprite direction starts after cube directions");
        ProbeAssertions.Require(PackedSpriteVertex.AtlasLayer(packedSprite0) == 15, "packed sprite atlas layer");
        ProbeAssertions.Require(PackedSpriteVertex.U(packedSprite0) == 1, "packed sprite first u");
        ProbeAssertions.Require(PackedSpriteVertex.V(packedSprite0) == 1, "packed sprite first v");
        ProbeAssertions.Require(PackedSpriteVertex.U(packedSprite1) == 1, "packed sprite second u");
        ProbeAssertions.Require(PackedSpriteVertex.V(packedSprite1) == 0, "packed sprite second v");

        var plan = new ChunkMeshPlan(
            [boundaryFace],
            [
                new SpriteMeshFace(new BlockId(9), 8, 4, 8, Direction.PositiveZ),
                new SpriteMeshFace(new BlockId(22), 10, 4, 10, Direction.NegativeY)
            ],
            [new FluidMeshBlock(new BlockId(14), BlockRenderKind.Water, 12, 4, 12, 0)]);
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

        var transparentPlan = new ChunkMeshPlan([glassFace], [], []);
        var packedTransparent = packer.Pack(transparentPlan);
        ProbeAssertions.Require(packedTransparent.OpaqueCubeFaces.Count == 0, "packer keeps glass out of opaque bucket");
        ProbeAssertions.Require(packedTransparent.TransparentCubeFaces.Count == 1, "packer emits transparent cube bucket");
    }

    public static void ValidateNonFluidUploadPlan()
    {
        ProbeAssertions.Require(Marshal.SizeOf<PackedMeshUploadDescriptor>() == PackedMeshUploadDescriptor.SizeValue, "upload descriptor managed ABI size");
        ProbeAssertions.Require(Marshal.SizeOf<ChunkMeshUploadRecord>() == ChunkMeshUploadRecord.SizeValue, "chunk mesh upload record managed ABI size");

        var empty = PackedMeshUploadValidator.CreateNonFluidPlan(new PackedChunkMesh([], [], [], []));
        ProbeAssertions.Require(empty.OpaqueFaceCount == 0, "empty upload opaque count");
        ProbeAssertions.Require(empty.TransparentFaceCount == 0, "empty upload transparent count");
        ProbeAssertions.Require(empty.SpriteVertexCount == 0, "empty upload sprite vertex count");
        ProbeAssertions.Require(empty.SpriteIndexCount == 0, "empty upload sprite index count");
        ProbeAssertions.Require(empty.ClearsOpaqueFaces, "empty upload clears opaque");
        ProbeAssertions.Require(empty.ClearsTransparentFaces, "empty upload clears transparent");
        ProbeAssertions.Require(empty.ClearsSpriteVertices, "empty upload clears sprites");
        ProbeAssertions.Require(!empty.RequiresUploadSubmit, "empty upload does not require submit");
        ValidateEmptyDescriptor(empty.ToDescriptor());

        var rules = new BlockRenderRules();
        var planner = new ChunkMeshPlanner(rules);
        var packer = new ChunkMeshPacker(rules);
        var store = new BlockPresentationStore();
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 6, 1), new BlockId(1)), "upload plan opaque block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(4, 6, 4), new BlockId(30)), "upload plan transparent block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(8, 6, 8), new BlockId(9)), "upload plan sprite block");

        var packed = packer.Pack(planner.Build(store.CaptureNeighborhood(
            new PresentationChunkKey(0, 0, 0),
            NeighborhoodBoundaryBlocks.Air)));
        var upload = PackedMeshUploadValidator.CreateNonFluidPlan(packed);
        ValidateUploadPlan(packed, upload);
        ValidateChunkUploadRecord(packed, upload);

        ProbeAssertions.RequireThrows<InvalidOperationException>(
            () => PackedMeshUploadValidator.CreateNonFluidPlan(new PackedChunkMesh([], [], [1, 2, 3], [])),
            "upload plan rejects incomplete sprite quad");
        ProbeAssertions.RequireThrows<InvalidOperationException>(
            () => PackedMeshUploadValidator.CreateNonFluidPlan(new PackedChunkMesh(
                [],
                [],
                [],
                [new FluidMeshBlock(new BlockId(14), BlockRenderKind.Water, 1, 2, 3, 0)])),
            "upload plan rejects fluids");
    }

    private static void ValidateEmptyDescriptor(PackedMeshUploadDescriptor emptyDescriptor)
    {
        ProbeAssertions.Require(emptyDescriptor.Version == PackedMeshUploadDescriptor.VersionValue, "empty upload descriptor version");
        ProbeAssertions.Require(emptyDescriptor.Size == PackedMeshUploadDescriptor.SizeValue, "empty upload descriptor size");
        ProbeAssertions.Require(emptyDescriptor.OpaqueFaceCount == 0, "empty upload descriptor opaque count");
        ProbeAssertions.Require(emptyDescriptor.TransparentFaceCount == 0, "empty upload descriptor transparent count");
        ProbeAssertions.Require(emptyDescriptor.SpriteVertexCount == 0, "empty upload descriptor sprite vertex count");
        ProbeAssertions.Require(emptyDescriptor.SpriteIndexCount == 0, "empty upload descriptor sprite index count");
        ProbeAssertions.Require(emptyDescriptor.OpaqueByteCount == 0, "empty upload descriptor opaque bytes");
        ProbeAssertions.Require(emptyDescriptor.TransparentByteCount == 0, "empty upload descriptor transparent bytes");
        ProbeAssertions.Require(emptyDescriptor.SpriteByteCount == 0, "empty upload descriptor sprite bytes");
        ProbeAssertions.Require(emptyDescriptor.Flags == (
            PackedMeshUploadDescriptor.ClearOpaqueFacesFlag |
            PackedMeshUploadDescriptor.ClearTransparentFacesFlag |
            PackedMeshUploadDescriptor.ClearSpriteVerticesFlag), "empty upload descriptor clear flags");
    }

    private static void ValidateUploadPlan(PackedChunkMesh packed, PackedMeshUploadPlan upload)
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

    private static void ValidateChunkUploadRecord(PackedChunkMesh packed, PackedMeshUploadPlan upload)
    {
        var record = ChunkMeshUploadRecord.Create(
            new PresentationChunkKey(1, 2, 3),
            packed,
            opaqueFaceOffset: 4,
            transparentFaceOffset: 8,
            spriteVertexOffset: 12);
        ProbeAssertions.Require(record.Version == ChunkMeshUploadRecord.VersionValue, "chunk upload record version");
        ProbeAssertions.Require(record.Size == ChunkMeshUploadRecord.SizeValue, "chunk upload record size");
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
        ProbeAssertions.Require(record.Flags == ChunkMeshUploadRecord.ClearFluidBlocksFlag, "chunk upload record fluid clear flag");
    }
}
