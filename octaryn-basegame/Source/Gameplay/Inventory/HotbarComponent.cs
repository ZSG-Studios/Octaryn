namespace Octaryn.Basegame.Gameplay.Inventory;

// Module-owned hotbar: ten stacks plus a selection cursor. Plain struct
// component; systems mutate it through the host ECS query reference.
public struct HotbarComponent
{
    public const int SlotCount = 10;

    public (ushort ItemId, uint Count)[] Slots = new (ushort ItemId, uint Count)[SlotCount];

    public int SelectedSlot;

    public HotbarComponent()
    {
    }
}
