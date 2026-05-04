using Octaryn.Basegame.Content.Blocks;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Shared.World;

return BasegamePlayerProbe.Run();

internal static class BasegamePlayerProbe
{
    public static int Run()
    {
        Require(PlayerBlockSelectionState.Default.SelectedBlock == BlockCatalog.YellowTorch, "default selected block is yellow torch");
        Require(PlayerBlockSelectionRules.IsPlaceable(BlockCatalog.Grass), "grass is placeable");
        Require(PlayerBlockSelectionRules.IsPlaceable(BlockCatalog.WaterSource), "water source is placeable");
        Require(!PlayerBlockSelectionRules.IsPlaceable(BlockId.Air), "air is not placeable");
        Require(!PlayerBlockSelectionRules.IsPlaceable(BlockCatalog.Cloud), "cloud is not placeable");
        Require(!PlayerBlockSelectionRules.IsPlaceable(BlockCatalog.WaterLevelOne), "flowing water is not placeable");
        Require(!PlayerBlockSelectionRules.IsPlaceable(BlockCatalog.LavaLevelOne), "flowing lava is not placeable");

        var selected = new PlayerBlockSelectionState(BlockCatalog.YellowTorch);
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(selected, 1).SelectedBlock == BlockCatalog.CyanTorch, "change block forward");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(selected, -1).SelectedBlock == BlockCatalog.BlueTorch, "change block backward");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.Leaves), 1).SelectedBlock == BlockCatalog.Bush, "change skips cloud");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.Leaves), 2).SelectedBlock == BlockCatalog.Bush, "change raw block offset skips cloud");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.Cloud), 1).SelectedBlock == BlockCatalog.Bush, "change from cloud advances to bush");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.Cloud), -1).SelectedBlock == BlockCatalog.Leaves, "change from cloud retreats to leaves");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.WaterSource), 1).SelectedBlock == BlockCatalog.RedTorch, "change skips flowing water");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.WaterLevelOne), -1).SelectedBlock == BlockCatalog.WaterSource, "change from flowing water retreats to source");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.LavaSource), 1).SelectedBlock == BlockCatalog.Grass, "change wraps forward");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.LavaLevelSeven), -1).SelectedBlock == BlockCatalog.LavaSource, "change from flowing lava retreats to source");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.Grass), -1).SelectedBlock == BlockCatalog.LavaSource, "change wraps backward");
        Require(PlayerBlockSelectionRules.ChangeSelectedBlock(new PlayerBlockSelectionState(BlockCatalog.Cloud), 0).SelectedBlock == BlockCatalog.YellowTorch, "zero change from invalid block returns default");

        Require(PlayerBlockSelectionRules.SelectTargetBlock(selected, BlockId.Air) == selected, "select ignores air");
        Require(PlayerBlockSelectionRules.SelectTargetBlock(selected, BlockCatalog.Cloud) == selected, "select ignores cloud");
        Require(PlayerBlockSelectionRules.SelectTargetBlock(selected, BlockCatalog.WaterSource) == selected, "select ignores water source");
        Require(PlayerBlockSelectionRules.SelectTargetBlock(selected, BlockCatalog.WaterLevelOne) == selected, "select ignores flowing water");
        Require(PlayerBlockSelectionRules.SelectTargetBlock(selected, BlockCatalog.LavaSource) == selected, "select ignores lava source");
        Require(PlayerBlockSelectionRules.SelectTargetBlock(selected, BlockCatalog.Stone).SelectedBlock == BlockCatalog.Stone, "select target block");
        Require(PlayerBlockSelectionRules.SelectTargetBlock(selected, BlockCatalog.Bush).SelectedBlock == BlockCatalog.Bush, "select target sprite block");
        return 0;
    }

    private static void Require(bool condition, string name)
    {
        if (!condition)
        {
            throw new InvalidOperationException(name);
        }
    }
}
