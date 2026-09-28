namespace Octaryn.Basegame.Gameplay.Items;

// Module content catalog: every item the basegame rules can create, keyed by
// stable item id. Content declarations move to module data once the content
// pipeline lands; ids are save-facing and must stay stable.
public static class ItemCatalog
{
    public readonly record struct Entry(ushort ItemId, string Name, uint MaxStack);

    public const ushort Coin = 1;
    public const ushort Apple = 2;
    public const ushort Torch = 3;
    public const ushort Pebble = 4;

    private static readonly Entry[] s_entries =
    [
        new Entry(Coin, "coin", 99),
        new Entry(Apple, "apple", 16),
        new Entry(Torch, "torch", 16),
        new Entry(Pebble, "pebble", 64),
    ];

    public static bool TryGet(ushort itemId, out Entry entry)
    {
        foreach (var candidate in s_entries)
        {
            if (candidate.ItemId == itemId)
            {
                entry = candidate;
                return true;
            }
        }

        entry = default;
        return false;
    }

    public static bool IsValid(ushort itemId)
    {
        return TryGet(itemId, out _);
    }
}
