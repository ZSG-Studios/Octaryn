using Octaryn.Basegame.Content.Blocks;
using Octaryn.Basegame.Content.Fluids;
using Octaryn.Basegame.Gameplay.Interaction;
using Octaryn.Shared.World;

return BasegameInteractionProbe.Run();

internal static class BasegameInteractionProbe
{
    public static int Run()
    {
        var aboveGround = new BlockPosition(0, 2, 0);
        var bottomLayer = new BlockPosition(0, 0, 0);

        Require(BlockSupportRules.CanStaySupported(BlockId.Air, bottomLayer, BlockId.Air), "air is always supported");
        Require(BlockSupportRules.CanStaySupported(BlockCatalog.Grass, aboveGround, BlockId.Air), "grass does not need base support");
        Require(BlockSupportRules.CanStaySupported(BlockCatalog.Grass, bottomLayer, BlockCatalog.Grass), "basegame support rules do not own world floor policy");

        Require(BlockSupportRules.RequiresGrass(BlockCatalog.Bush), "bush requires grass");
        Require(BlockSupportRules.RequiresGrass(BlockCatalog.Lavender), "lavender requires grass");
        Require(!BlockSupportRules.RequiresGrass(BlockCatalog.RedTorch), "torch does not require grass");
        Require(BlockSupportRules.CanStaySupported(BlockCatalog.Bush, aboveGround, BlockCatalog.Grass), "bush accepts grass base");
        Require(!BlockSupportRules.CanStaySupported(BlockCatalog.Bush, aboveGround, BlockCatalog.Dirt), "bush rejects dirt base");

        Require(BlockSupportRules.RequiresSolidBase(BlockCatalog.RedTorch), "red torch requires solid base");
        Require(BlockSupportRules.RequiresSolidBase(BlockCatalog.WhiteTorch), "white torch requires solid base");
        Require(!BlockSupportRules.RequiresSolidBase(BlockCatalog.Bush), "bush does not require solid base");
        Require(BlockSupportRules.CanStaySupported(BlockCatalog.YellowTorch, aboveGround, BlockCatalog.Glass), "torch accepts glass base");
        Require(!BlockSupportRules.CanStaySupported(BlockCatalog.YellowTorch, aboveGround, BlockCatalog.WaterSource), "torch rejects water base");

        Require(BlockSupportRules.IsSolid(BlockCatalog.Grass), "grass is solid");
        Require(BlockSupportRules.IsSolid(BlockCatalog.Leaves), "leaves are solid");
        Require(BlockSupportRules.IsSolid(BlockCatalog.Planks), "planks are solid");
        Require(BlockSupportRules.IsSolid(BlockCatalog.Glass), "glass is solid");
        Require(!BlockSupportRules.IsSolid(BlockCatalog.Cloud), "cloud is not solid");
        Require(!BlockSupportRules.IsSolid(BlockCatalog.WaterSource), "water is not solid");
        Require(!BlockSupportRules.IsSolid(BlockCatalog.RedTorch), "torch is not solid");

        var authorityRules = new BlockAuthorityRules();
        Require(authorityRules.CanApplyEdit(new BlockEdit(new BlockPosition(3, 4, 5), BlockCatalog.Stone), BlockId.Air), "authority accepts unsupported-independent edit");
        Require(!authorityRules.CanApplyEdit(new BlockEdit(new BlockPosition(3, 4, 5), BlockCatalog.Bush), BlockCatalog.Dirt), "authority rejects unsupported grass block edit");
        Require(authorityRules.CanStaySupported(BlockCatalog.RedTorch, aboveGround, BlockCatalog.Glass), "authority accepts supported solid-base block");
        Require(authorityRules.IsClientPlaceable(BlockCatalog.LavaSource), "authority accepts client-placeable lava source");
        Require(!authorityRules.IsClientPlaceable(BlockCatalog.LavaLevelOne), "authority rejects non-placeable lava flow");
        ValidateReplacementRules();
        ValidateSkylightOpacity();
        ValidateFluids();
        return 0;
    }

    private static void ValidateReplacementRules()
    {
        Require(BlockReplacementRules.CanBeReplacedByFluid(BlockCatalog.Leaves), "fluid replaces leaves");
        Require(BlockReplacementRules.CanBeReplacedByFluid(BlockCatalog.Bush), "fluid replaces bush");
        Require(BlockReplacementRules.CanBeReplacedByFluid(BlockCatalog.Lavender), "fluid replaces lavender");
        Require(!BlockReplacementRules.CanBeReplacedByFluid(BlockId.Air), "fluid replacement rule does not include air");
        Require(!BlockReplacementRules.CanBeReplacedByFluid(BlockCatalog.Grass), "fluid does not replace grass");
        Require(!BlockReplacementRules.CanBeReplacedByFluid(BlockCatalog.WaterSource), "fluid replacement rule does not include fluids");
        Require(!BlockReplacementRules.CanBeReplacedByFluid(BlockCatalog.Glass), "fluid does not replace glass");
    }

    private static void ValidateSkylightOpacity()
    {
        Require(BlockCatalog.SkylightOpacity(BlockId.Air) == 0, "air has no skylight opacity");
        Require(BlockCatalog.SkylightOpacity(BlockCatalog.Leaves) == 1, "leaves have low skylight opacity");
        Require(BlockCatalog.SkylightOpacity(BlockCatalog.WaterSource) == 2, "water source has fluid skylight opacity");
        Require(BlockCatalog.SkylightOpacity(BlockCatalog.WaterLevelSeven) == 2, "flowing water has fluid skylight opacity");
        Require(BlockCatalog.SkylightOpacity(BlockCatalog.LavaSource) == 2, "lava source has fluid skylight opacity");
        Require(BlockCatalog.SkylightOpacity(BlockCatalog.LavaLevelSeven) == 2, "flowing lava has fluid skylight opacity");
        Require(BlockCatalog.SkylightOpacity(BlockCatalog.Glass) == 0, "glass has no skylight opacity");
        Require(BlockCatalog.SkylightOpacity(BlockCatalog.Stone) == 15, "stone blocks skylight");
        Require(BlockCatalog.SkylightOpacity(new BlockId(ushort.MaxValue)) == 0, "unknown block has no skylight opacity");
    }

    private static void ValidateFluids()
    {
        Require(BlockCatalog.GetFluidKind(BlockCatalog.WaterSource) == FluidKind.Water, "water fluid kind");
        Require(BlockCatalog.GetFluidKind(BlockCatalog.WaterLevelOne) == FluidKind.Water, "flowing water fluid kind");
        Require(BlockCatalog.GetFluidKind(BlockCatalog.LavaSource) == FluidKind.Lava, "lava fluid kind");
        Require(BlockCatalog.GetFluidKind(BlockCatalog.LavaLevelOne) == FluidKind.Lava, "flowing lava fluid kind");
        Require(BlockCatalog.GetFluidKind(BlockCatalog.Grass) == FluidKind.None, "grass has no fluid kind");

        Require(BlockCatalog.IsWater(BlockCatalog.WaterSource), "water source is water");
        Require(BlockCatalog.IsWater(BlockCatalog.WaterLevelSeven), "water level seven is water");
        Require(!BlockCatalog.IsWater(BlockCatalog.LavaSource), "lava is not water");
        Require(BlockCatalog.IsLava(BlockCatalog.LavaSource), "lava source is lava");
        Require(BlockCatalog.IsLava(BlockCatalog.LavaLevelSeven), "lava level seven is lava");
        Require(!BlockCatalog.IsLava(BlockCatalog.WaterSource), "water is not lava");

        Require(BlockCatalog.IsFluid(BlockCatalog.WaterSource), "water source is fluid");
        Require(BlockCatalog.IsFluid(BlockCatalog.LavaLevelSeven), "lava level seven is fluid");
        Require(!BlockCatalog.IsFluid(BlockCatalog.Glass), "glass is not fluid");
        Require(BlockCatalog.IsFluidSource(BlockCatalog.WaterSource), "water source is fluid source");
        Require(!BlockCatalog.IsFluidSource(BlockCatalog.WaterLevelOne), "flowing water is not fluid source");
        Require(BlockCatalog.IsFluidSource(BlockCatalog.LavaSource), "lava source is fluid source");
        Require(!BlockCatalog.IsFluidSource(BlockCatalog.LavaLevelOne), "flowing lava is not fluid source");

        Require(BlockCatalog.FluidLevel(BlockCatalog.WaterSource) == 0, "water source level");
        Require(BlockCatalog.FluidLevel(BlockCatalog.WaterLevelOne) == 1, "water level one");
        Require(BlockCatalog.FluidLevel(BlockCatalog.WaterLevelSeven) == 7, "water level seven");
        Require(BlockCatalog.FluidLevel(BlockCatalog.LavaSource) == 0, "lava source level");
        Require(BlockCatalog.FluidLevel(BlockCatalog.LavaLevelOne) == 1, "lava level one");
        Require(BlockCatalog.FluidLevel(BlockCatalog.LavaLevelSeven) == 7, "lava level seven");
        Require(BlockCatalog.FluidLevel(BlockId.Air) == -1, "air has no fluid level");

        Require(BlockCatalog.MakeWater(-1) == BlockCatalog.WaterSource, "make water clamps low");
        Require(BlockCatalog.MakeWater(0) == BlockCatalog.WaterSource, "make water source");
        Require(BlockCatalog.MakeWater(1) == BlockCatalog.WaterLevelOne, "make water level one");
        Require(BlockCatalog.MakeWater(7) == BlockCatalog.WaterLevelSeven, "make water level seven");
        Require(BlockCatalog.MakeWater(99) == BlockCatalog.WaterLevelSeven, "make water clamps high");
        Require(BlockCatalog.MakeLava(-1) == BlockCatalog.LavaSource, "make lava clamps low");
        Require(BlockCatalog.MakeLava(0) == BlockCatalog.LavaSource, "make lava source");
        Require(BlockCatalog.MakeLava(1) == BlockCatalog.LavaLevelOne, "make lava level one");
        Require(BlockCatalog.MakeLava(7) == BlockCatalog.LavaLevelSeven, "make lava level seven");
        Require(BlockCatalog.MakeLava(99) == BlockCatalog.LavaLevelSeven, "make lava clamps high");
        var generatedWater = BlockCatalog.MakeFluid(FluidKind.Water, 3);
        Require(BlockCatalog.GetFluidKind(generatedWater) == FluidKind.Water, "make generic water kind");
        Require(BlockCatalog.FluidLevel(generatedWater) == 3, "make generic water level");

        var generatedLava = BlockCatalog.MakeFluid(FluidKind.Lava, 3);
        Require(BlockCatalog.GetFluidKind(generatedLava) == FluidKind.Lava, "make generic lava kind");
        Require(BlockCatalog.FluidLevel(generatedLava) == 3, "make generic lava level");
        Require(BlockCatalog.MakeFluid(FluidKind.None, 3) == BlockId.Air, "make no fluid");
    }

    private static void Require(bool condition, string name)
    {
        if (!condition)
        {
            throw new InvalidOperationException(name);
        }
    }
}
