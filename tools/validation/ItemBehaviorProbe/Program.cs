using Octaryn.Basegame;
using Octaryn.Basegame.Gameplay.Actions;
using Octaryn.Basegame.Gameplay.Inventory;
using Octaryn.Basegame.Gameplay.Items;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Server.Host;
using Octaryn.Shared.Host.Api;

using var ecs = new ArchHostEcsApi();
var player = PlayerEntity.Create(ecs);
ecs.SetComponent(player, new PlayerBodyComponent { Initialized = true });
var replication = new RecordingReplication();
var inventory = new InventorySystem(null);
var items = new ItemSystem(null, replication, null, inventory);
var interaction = new InteractionSystem(null, null, replication, null, items);
Require(inventory.TryAdd(ecs, ItemCatalog.Apple, 160, 16) == 160, "fill hotbar");
var drop = items.Spawn(ecs, ItemCatalog.Apple, 5, 0, 0, -3, 0, 0, 0);
replication.AvailableChangeCapacity = 0;
interaction.Tick(ecs);
Require(replication.Events.Count == 0, "target defers while outbound channel is full");
replication.AvailableChangeCapacity = int.MaxValue;
interaction.Tick(ecs);
Require(replication.Events.Last() == (BasegameReplicationEvent.ItemTargeted, drop.Id, 2UL, 5UL),
    "publish initial target");
var eventCount = replication.Events.Count;
interaction.Tick(ecs);
Require(replication.Events.Count == eventCount, "unchanged target emits no event");
Require(!interaction.HandleAction(ecs, InteractionSystem.UseActionPrefix), "full inventory rejects pickup");
Require(InventoryCount() == 160 && WorldCount() == 5, "rejected pickup conserves counts");
Require(replication.Events.Count == eventCount, "rejected pickup emits no grant");

Require(inventory.TryRemoveSelected(ecs, 2).Count == 2, "make partial capacity");
replication.AvailableChangeCapacity = 0;
Require(!interaction.HandleAction(ecs, InteractionSystem.UseActionPrefix), "full outbound channel defers pickup");
Require(InventoryCount() == 158 && WorldCount() == 5, "deferred pickup conserves inventory and world");
replication.AvailableChangeCapacity = int.MaxValue;
Require(interaction.HandleAction(ecs, InteractionSystem.UseActionPrefix), "partial pickup succeeds");
Require(InventoryCount() == 160 && WorldCount() == 3, "partial pickup conserves counts");
Require(replication.Events.Last() == (BasegameReplicationEvent.ItemGranted, 1UL, 2UL, 2UL),
    "partial grant has exact amount");
interaction.Tick(ecs);
Require(replication.Events.Last() == (BasegameReplicationEvent.ItemTargeted, drop.Id, 2UL, 3UL),
    "same target publishes remaining count");

Require(inventory.TryRemoveSelected(ecs, 3).Count == 3, "make complete capacity");
Require(interaction.HandleAction(ecs, InteractionSystem.UseActionPrefix), "complete pickup succeeds");
Require(InventoryCount() == 160 && WorldCount() == 0, "complete pickup conserves counts");
Require(!items.TryCollectEntity(ecs, drop.Id), "collected entity cannot grant twice");
Require(replication.Events.Last() == (BasegameReplicationEvent.ItemGranted, 2UL, 2UL, 3UL),
    "complete grant advances receipt");
interaction.Tick(ecs);
Require(replication.Events.Last() == (BasegameReplicationEvent.ItemTargeted, 0UL, 0UL, 0UL),
    "removed target clears highlight");

replication.AvailableChangeCapacity = 0;
Require(!items.HandleAction(ecs, ItemSystem.DropStackAction), "full outbound channel defers drop before mutation");
Require(InventoryCount() == 160 && WorldCount() == 0, "deferred drop conserves inventory and world");
replication.AvailableChangeCapacity = int.MaxValue;
Require(items.HandleAction(ecs, ItemSystem.DropStackAction), "drop selected full stack");
Require(InventoryCount() == 144 && WorldCount() == 16, "whole-stack drop conserves counts");
Require(replication.Events.Last() == (BasegameReplicationEvent.ItemDropped, 3UL, 2UL, 16UL),
    "whole-stack drop publishes exact amount");
ulong stackId = 0;
ecs.Query<WorldItemComponent>((ModuleEntity entity, ref WorldItemComponent _) => stackId = entity.Id);
Require(items.TryCollectEntity(ecs, stackId), "whole-stack pickup succeeds");
Require(InventoryCount() == 160 && WorldCount() == 0, "whole-stack pickup conserves counts");

// Exercise removal of multiple items in the real host ECS query path.
Require(inventory.TryRemoveSelected(ecs, 8).Count == 8, "make multi-item capacity");
for (var i = 0; i < 8; ++i)
{
    var entity = items.Spawn(ecs, ItemCatalog.Apple, 1, 0, -0.9f, 0, 0, 0, 0);
    ecs.TryGetComponent(entity, out WorldItemComponent state);
    state.Grounded = true;
    ecs.SetComponent(entity, state);
}
items.Tick(ecs, 1.0 / 60.0);
Require(InventoryCount() == 160 && WorldCount() == 0, "vacuum collects every item without loss");

Require(inventory.TryRemoveSelected(ecs, 1).Count == 1, "make stale-target capacity");
var behind = items.Spawn(ecs, ItemCatalog.Apple, 1, 0, 0, -3, 0, 0, 0);
interaction.Tick(ecs);
ecs.SetComponent(player, new PlayerBodyComponent {
    Initialized = true, State = new HostCharacterState { Yaw = MathF.PI }
});
Require(!interaction.HandleAction(ecs, InteractionSystem.UseActionPrefix), "use refreshes changed view");
Require(InventoryCount() == 159 && WorldCount() == 1, "stale target cannot collect");
ecs.SetComponent(player, new PlayerBodyComponent { Initialized = true });
Require(interaction.HandleAction(ecs, InteractionSystem.UseActionPrefix), "use acquires current view immediately");
Require(InventoryCount() == 160 && WorldCount() == 0, "fresh target conserves counts");
Console.WriteLine("ITEM_BEHAVIOR_PROBE=passed");
if (!args.Contains("--behavior-only")) ItemScaleProbe.Run();

uint InventoryCount()
{
    ecs.TryGetComponent(player, out HotbarComponent hotbar);
    uint count = 0;
    foreach (var slot in hotbar.Slots)
        count += slot.Count;
    return count;
}

uint WorldCount()
{
    uint count = 0;
    ecs.Query<WorldItemComponent>((ModuleEntity _, ref WorldItemComponent item) => count += item.Count);
    return count;
}

static void Require(bool condition, string message)
{
    if (!condition)
        throw new InvalidOperationException(message);
    Console.WriteLine("PASS " + message);
}

sealed class RecordingReplication : IHostReplicationApi
{
    public int AvailableChangeCapacity { get; set; } = int.MaxValue;
    public int AvailableWorldItemCapacity => 10000;
    public bool PublishWorldItem(in HostWorldItemPose pose) => true;
    public List<(uint Kind, ulong Id, ulong Payload0, ulong Payload1)> Events { get; } = [];

    public bool PublishChange(uint changeKind, ulong replicationId, ulong payload0, ulong payload1)
    {
        Events.Add((changeKind, replicationId, payload0, payload1));
        return true;
    }

    public int SendMessage(ulong replicationId, ReadOnlySpan<byte> payload) => payload.Length;
}
