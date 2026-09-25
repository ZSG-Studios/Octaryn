using Octaryn.Shared.World;

namespace Octaryn.Basegame.Gameplay.Player;

public readonly record struct PlayerBlockSelectionState(BlockId SelectedBlock)
{
    public static PlayerBlockSelectionState Default => new(PlayerBlockSelectionRules.DefaultSelectedBlock);
}
