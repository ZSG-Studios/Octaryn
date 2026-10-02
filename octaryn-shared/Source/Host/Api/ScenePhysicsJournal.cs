namespace Octaryn.Shared.Host.Api;

// Bounded host transport DTOs; JSON is host-owned and never provided as a
// general filesystem or networking capability to modules.
public sealed class ScenePhysicsCommand
{
    public ulong RequestId { get; set; }
    public string Kind { get; set; } = "";
    public ulong SourceId { get; set; }
    public float X { get; set; }
    public float Y { get; set; }
    public float Z { get; set; }
    public float Dx { get; set; }
    public float Dy { get; set; }
    public float Dz { get; set; }
    public float Reach { get; set; }
    public float HitX { get; set; }
    public float HitY { get; set; }
    public float HitZ { get; set; }
}
public sealed class ScenePhysicsCommandJournal
{
    public int Version { get; set; } = 1;
    public ulong Epoch { get; set; }
    public ulong Seq { get; set; }
    public ScenePhysicsCommand[] Commands { get; set; } = [];
}
public sealed class ScenePhysicsSnapshot
{
    public int Version { get; set; } = 1;
    public string ScenePath { get; set; } = "";
    public bool ResidencyPending { get; set; }
    public ulong Epoch { get; set; }
    public ulong Seq { get; set; }
    public ulong Tick { get; set; }
    public HostSceneBodyHit? Target { get; set; }
    public HostSceneBodyPose[] Bodies { get; set; } = [];
    public HostScenePhysicsReceipt[] Receipts { get; set; } = [];
    public HostSceneInventoryCount[] Inventory { get; set; } = [];
}
public readonly record struct HostSceneInventoryCount(string ItemBase, uint Count);
