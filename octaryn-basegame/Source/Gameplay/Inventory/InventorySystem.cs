using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Inventory;

// Hotbar rules: selection actions from the UI channel plus stack add/remove
// with strict count conservation. All state lives in HotbarComponent inside
// the host ECS; the system is stateless logic with only API handles.
public sealed class InventorySystem
{
    public const string SelectActionPrefix = "inventory.select.";
    public const string DropAction = "inventory.drop";
    public const string DropStackAction = "inventory.drop_stack";

    private readonly IHostDiagnosticsApi? _diagnostics;

    public InventorySystem(IHostDiagnosticsApi? diagnostics)
    {
        _diagnostics = diagnostics;
    }

    // Action router callback: "inventory.select.N" moves the cursor.
    public bool HandleAction(IHostEcsApi ecs, string actionId)
    {
        if (!actionId.StartsWith(SelectActionPrefix, StringComparison.Ordinal) ||
            !int.TryParse(actionId[SelectActionPrefix.Length..], out var slot))
        {
            return false;
        }

        var handled = false;
        ecs.Query<HotbarComponent>((ModuleEntity _, ref HotbarComponent hotbar) =>
        {
            if (slot < 0 || slot >= HotbarComponent.SlotCount || slot == hotbar.SelectedSlot)
            {
                return;
            }

            hotbar.SelectedSlot = slot;
            handled = true;
            _diagnostics?.Write(
                HostLogLevel.Debug,
                $"octaryn.basegame inventory select slot={slot}");
        });
        return handled;
    }

    // Adds up to count items, respecting the stack cap. Returns the amount
    // actually added; the caller keeps ownership of any remainder.
    public uint TryAdd(IHostEcsApi ecs, ushort itemId, uint count, uint maxStack)
    {
        var remaining = count;
        ecs.Query<HotbarComponent>((ModuleEntity _, ref HotbarComponent hotbar) =>
        {
            for (var slot = 0; slot < HotbarComponent.SlotCount && remaining > 0; slot++)
            {
                if (hotbar.Slots[slot].ItemId != itemId || hotbar.Slots[slot].Count == 0 ||
                    hotbar.Slots[slot].Count >= maxStack)
                {
                    continue;
                }

                var take = System.Math.Min(remaining, maxStack - hotbar.Slots[slot].Count);
                hotbar.Slots[slot].Count += take;
                remaining -= take;
            }

            for (var slot = 0; slot < HotbarComponent.SlotCount && remaining > 0; slot++)
            {
                if (hotbar.Slots[slot].Count != 0)
                {
                    continue;
                }

                var take = System.Math.Min(remaining, maxStack);
                hotbar.Slots[slot] = (itemId, take);
                remaining -= take;
            }
        });
        return count - remaining;
    }

    // Removes up to count items from the selected slot. Returns the removed
    // item id and amount; nothing is removed from an empty slot.
    public (ushort ItemId, uint Count) TryRemoveSelected(IHostEcsApi ecs, uint count)
    {
        var removed = (ItemId: (ushort)0, Count: 0u);
        ecs.Query<HotbarComponent>((ModuleEntity _, ref HotbarComponent hotbar) =>
        {
            ref var slot = ref hotbar.Slots[hotbar.SelectedSlot];
            if (slot.Count == 0)
            {
                return;
            }

            var take = System.Math.Min(count, slot.Count);
            removed = (slot.ItemId, take);
            slot.Count -= take;
            if (slot.Count == 0)
            {
                slot = default;
            }
        });
        return removed;
    }
}
