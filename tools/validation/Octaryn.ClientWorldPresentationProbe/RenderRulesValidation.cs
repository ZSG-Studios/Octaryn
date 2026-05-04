using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.World;

internal static class RenderRulesValidation
{
    public static void Validate()
    {
        var rules = new BlockRenderRules();

        ProbeAssertions.Require(rules.RenderKind(BlockId.Air) == BlockRenderKind.Empty, "air has no render mesh");
        ProbeAssertions.Require(!rules.ShouldBuildCubeFaces(BlockId.Air), "air has no cube faces");

        var grass = rules.Properties(new BlockId(1));
        ProbeAssertions.Require(grass.Kind == BlockRenderKind.OpaqueCube, "grass renders as opaque cube");
        ProbeAssertions.Require(grass.IsOpaque, "grass is opaque");
        ProbeAssertions.Require(grass.HasOcclusion, "grass occludes faces");

        var leaves = rules.Properties(new BlockId(7));
        ProbeAssertions.Require(leaves.Kind == BlockRenderKind.OpaqueCube, "leaves render as opaque cube");
        ProbeAssertions.Require(leaves.IsOpaque, "leaves are opaque");
        ProbeAssertions.Require(!leaves.HasOcclusion, "leaves do not occlude neighbor faces");

        var cloud = rules.Properties(new BlockId(8));
        ProbeAssertions.Require(cloud.Kind == BlockRenderKind.Hidden, "cloud has no emitted render mesh");
        ProbeAssertions.Require(!rules.ShouldBuildCubeFaces(new BlockId(8)), "cloud skips cube faces");

        var bush = rules.Properties(new BlockId(9));
        ProbeAssertions.Require(bush.Kind == BlockRenderKind.Sprite, "bush renders as sprite");
        ProbeAssertions.Require(bush.IsSprite, "bush is sprite");
        ProbeAssertions.Require(!bush.HasOcclusion, "bush does not occlude neighbor faces");

        var waterSource = rules.Properties(new BlockId(14));
        ProbeAssertions.Require(waterSource.Kind == BlockRenderKind.Water, "water source renders as water");
        ProbeAssertions.Require(waterSource.IsFluid, "water source is fluid");
        ProbeAssertions.Require(waterSource.FluidLevel == 0, "water source fluid level");

        var waterFlow = rules.Properties(new BlockId(21));
        ProbeAssertions.Require(waterFlow.Kind == BlockRenderKind.Water, "flowing water renders as water");
        ProbeAssertions.Require(waterFlow.FluidLevel == 7, "flowing water max fluid level");

        var glass = rules.Properties(new BlockId(30));
        ProbeAssertions.Require(glass.Kind == BlockRenderKind.TransparentCube, "glass renders as transparent cube");
        ProbeAssertions.Require(!glass.HasOcclusion, "glass does not occlude neighbor faces");

        var lavaSource = rules.Properties(new BlockId(31));
        ProbeAssertions.Require(lavaSource.Kind == BlockRenderKind.Lava, "lava source renders as lava");
        ProbeAssertions.Require(lavaSource.IsFluid, "lava source is fluid");
        ProbeAssertions.Require(lavaSource.FluidLevel == 0, "lava source fluid level");

        var lavaFlow = rules.Properties(new BlockId(38));
        ProbeAssertions.Require(lavaFlow.Kind == BlockRenderKind.Lava, "flowing lava renders as lava");
        ProbeAssertions.Require(lavaFlow.FluidLevel == 7, "flowing lava max fluid level");

        ProbeAssertions.Require(rules.IsCubeFaceVisible(new BlockId(1), BlockId.Air), "air exposes opaque face");
        ProbeAssertions.Require(!rules.IsCubeFaceVisible(new BlockId(1), new BlockId(5)), "opaque neighbor hides opaque face");
        ProbeAssertions.Require(rules.IsCubeFaceVisible(new BlockId(1), new BlockId(9)), "sprite neighbor exposes opaque face");
        ProbeAssertions.Require(rules.IsCubeFaceVisible(new BlockId(1), new BlockId(30)), "glass neighbor exposes opaque face");
        ProbeAssertions.Require(!rules.IsCubeFaceVisible(new BlockId(30), new BlockId(30)), "glass neighbor hides glass face");
        ProbeAssertions.Require(rules.IsCubeFaceVisible(new BlockId(30), new BlockId(7)), "leaves expose glass face");
        ProbeAssertions.Require(rules.IsCubeFaceVisible(new BlockId(7), new BlockId(7)), "leaves neighbor keeps leaf face visible");
        ProbeAssertions.Require(!rules.IsCubeFaceVisible(new BlockId(8), BlockId.Air), "cloud emits no face against air");
        ProbeAssertions.Require(!rules.IsCubeFaceVisible(new BlockId(14), BlockId.Air), "water does not use cube face rule");
    }
}
