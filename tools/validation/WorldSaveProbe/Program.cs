using System.Buffers.Binary;
using System.Reflection;
using Octaryn.Basegame;
using Octaryn.Basegame.Gameplay.Inventory;
using Octaryn.Basegame.Gameplay.Items;
using Octaryn.Basegame.Gameplay.Player;
using Octaryn.Basegame.Module;
using Octaryn.Server.Host;
using Octaryn.Server.Modules;
using Octaryn.Server.Persistence.World;
using Octaryn.Server.Simulation.Players;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host.Api;

var mode = args[0]; var world = Path.GetFullPath(args[1]); var variant = args[2] == "b";
if (mode == "queue") { SaveQueueProbe.Run(); return; }
Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", world);
Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", Path.Combine(world, "world_blocks.json"));
var expectedSlots = variant ? 6u : 4u;
const BindingFlags fields = BindingFlags.NonPublic | BindingFlags.Instance;
var prior = mode == "verify" ? new WorldSaveStore(world).Read("octaryn.basegame", "octaryn.basegame.save.v1") : null;
using (var module = new ModuleActivator(new ModuleRegistration()))
{
    if (module.Activate(new ConsoleCommandSink()) != 0) throw new InvalidOperationException("Module activation failed.");
    var context = (GameContext)typeof(ModuleActivator).GetField("_instance", fields)!.GetValue(module)!;
    var ecs = (IHostEcsApi)typeof(GameContext).GetField("_ecs", fields)!.GetValue(context)!;
    var items = (ItemSystem)typeof(GameContext).GetField("_items", fields)!.GetValue(context)!;
    if (!PlayerEntity.TryGet(ecs, out var player)) throw new InvalidOperationException("Player missing.");
    if (mode == "seed")
    {
        ecs.TryGetComponent(player, out HotbarComponent hotbar);
        hotbar.Slots[0].Count = expectedSlots + 1;
        ecs.SetComponent(player, hotbar);
        items.Spawn(ecs, ItemCatalog.Apple, 7 - expectedSlots, 10, 2, 10, .25f, .5f, .75f);
        if (!items.HandleAction(ecs, ItemSystem.DropAction)) throw new InvalidOperationException("Actual drop action failed.");
        new InventorySystem(null).HandleAction(ecs, "inventory.select.7");
        var removed = ecs.CreateEntity(); ecs.DestroyEntity(removed);
        var controller = (PlayerController)typeof(ModuleActivator).GetField("_playerController", fields)!.GetValue(module)!;
        controller.SetState(new(variant ? -3 : 3, 1.62f, variant ? -4 : 4, -.2f, .7f, 0, 0, 0, true, 0));
        module.SetWorldTimeHourOffset(variant ? 6 : 3);
        module.SetWorldTimeSpeedMultiplier(1.5);
        var bytes = new byte[IGameModuleSaveState.MaximumBytes];
        var length = context.CaptureSaveState(bytes);
        foreach (var offset in new[] { 0, 92 + 54 })
        {
            var malformed = bytes.AsSpan(0, length).ToArray();
            if (offset == 0) malformed[0] ^= 1;
            else malformed.AsSpan(92, 8).CopyTo(malformed.AsSpan(offset, 8));
            try { context.RestoreSaveState(malformed); throw new InvalidOperationException("Invalid snapshot accepted."); }
            catch (ArgumentException) { }
            var unchanged = new byte[IGameModuleSaveState.MaximumBytes];
            if (context.CaptureSaveState(unchanged) != length || !bytes.AsSpan(0, length).SequenceEqual(unchanged.AsSpan(0, length)))
                throw new InvalidOperationException("Invalid snapshot changed live state.");
        }
        var saves = (ModuleWorldSave)typeof(ModuleActivator).GetField("_worldSave", fields)!.GetValue(module)!;
        saves.Tick(5);
        if (!SpinWait.SpinUntil(() =>
        {
            saves.Tick(0);
            try { return new WorldSaveStore(world).Read("octaryn.basegame", "octaryn.basegame.save.v1")?.Generation >= 2; }
            catch (IOException error) when ((error.HResult & 0xffff) is 32 or 33) { return false; }
        }, 10000))
            throw new InvalidOperationException("Live autosave did not commit.");
    }
    else if (mode == "verify")
    {
        ecs.TryGetComponent(player, out HotbarComponent hotbar);
        if (hotbar.SelectedSlot != 7 || hotbar.Slots[0].Count != expectedSlots)
            throw new InvalidOperationException("Inventory restarted or leaked across worlds.");
        var total = hotbar.Slots.Where(slot => slot.ItemId == ItemCatalog.Apple).Sum(slot => (long)slot.Count);
        var ids = new List<ulong>();
        ecs.Query<WorldItemComponent>((ModuleEntity entity, ref WorldItemComponent state) =>
        {
            ids.Add(entity.Id);
            if (state.ItemId == ItemCatalog.Apple) total += state.Count;
        });
        if (total != 8 || !ids.Order().SequenceEqual(new ulong[] { 2, 3 }))
            throw new InvalidOperationException("World item identities/counts were not restored.");
        if (ecs.EntityIdWatermark != 4 || ecs.CreateEntity().Id != 5)
            throw new InvalidOperationException("Removed entity identity was reused.");
        var pose = module.SnapshotPlayer();
        if (Math.Abs(pose.X - (variant ? -3 : 3)) > .001 || Math.Abs(pose.Z - (variant ? -4 : 4)) > .001)
            throw new InvalidOperationException("Authoritative saved pose was not restored.");
        var clock = (Octaryn.Server.World.Time.WorldTimeClock)typeof(ModuleActivator).GetField("_worldTime", fields)!.GetValue(module)!;
        var clockSave = clock.CaptureSave();
        if (clockSave.SpeedMultiplier != 1.5)
            throw new InvalidOperationException("Clock settings were not restored.");
        if (clockSave.DayIndex != prior!.Clock.DayIndex || clockSave.SecondsOfDay != prior.Clock.SecondsOfDay)
            throw new InvalidOperationException("Clock phase was not restored through its native blob.");
        module.SetWorldTimeHourOffset(1);
        var stepped = clock.CaptureSave();
        if (Math.Abs((stepped.DayIndex - clockSave.DayIndex) * 86400 + stepped.SecondsOfDay - clockSave.SecondsOfDay - 3600) > .001)
            throw new InvalidOperationException("First clock intent after restart used a stale offset.");
        var bytes = new byte[IGameModuleSaveState.MaximumBytes];
        context.CaptureSaveState(bytes);
        var receipt = BinaryPrimitives.ReadUInt64LittleEndian(bytes.AsSpan(8));
        if (receipt != 2ul << 32) throw new InvalidOperationException("Receipt session namespace was not advanced.");
    }
    else if (mode is "scale-seed" or "scale-verify")
    {
        if (mode == "scale-seed")
            for (var i = 0; i < 10000; ++i) items.Spawn(ecs, ItemCatalog.Apple, 1, 10, 2, 10, 0, 0, 0);
        var count = 0;
        ecs.Query<WorldItemComponent>((ModuleEntity _, ref WorldItemComponent _) => ++count);
        if (count != 10000) throw new InvalidOperationException("Bounded 10000-item snapshot failed.");
        var bytes = new byte[IGameModuleSaveState.MaximumBytes];
        if (context.CaptureSaveState(bytes) > IGameModuleSaveState.MaximumBytes)
            throw new InvalidOperationException("Snapshot exceeded its byte budget.");
    }
    else if (mode == "receipt-crash")
    {
        if (!items.HandleAction(ecs, ItemSystem.DropAction)) throw new InvalidOperationException("Crash fixture action failed.");
        Console.WriteLine("receipt_crash_fixture status=issued process_exit_before_autosave=1");
        Environment.Exit(0);
    }
    else if (mode == "receipt-recover")
    {
        var bytes = new byte[IGameModuleSaveState.MaximumBytes]; context.CaptureSaveState(bytes);
        if (BinaryPrimitives.ReadUInt64LittleEndian(bytes.AsSpan(8)) != 2ul << 32)
            throw new InvalidOperationException("Crash caused receipt identity reuse.");
    }
    else if (mode != "load") throw new ArgumentException("Unknown probe mode.");
}
var store = new WorldSaveStore(world);
var saved = store.Read("octaryn.basegame", "octaryn.basegame.save.v1")!;
Console.WriteLine($"world_save_probe status=passed mode={mode} world={args[2]} generation={saved.Generation} session={saved.SessionId}");
