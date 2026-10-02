using System.Text.Json;
using Octaryn.Shared.Host.Api;
namespace Octaryn.Server.World.MapWorld;

internal sealed class ScenePhysicsSavedState
{
    public int Version { get; set; }=1;
    public ulong[] Removed { get; set; }=[];
    public HostSceneInventoryCount[] Inventory { get; set; }=[];
    public HostSceneBodyPose[] Bodies { get; set; }=[];
    public ulong ReceiptEpoch { get; set; }
    public HostScenePhysicsReceipt[] Receipts { get; set; }=[];
}
internal static class ScenePhysicsPersistence
{
    internal static ScenePhysicsSavedState Load(string directory)
    {
        var path=Path.Combine(directory,"scene_physics.state.json");
        if(!File.Exists(path))return new();
        if(new FileInfo(path).Length>33554432)throw new InvalidDataException("Scene physics state exceeds its bound.");
        var state=JsonSerializer.Deserialize<ScenePhysicsSavedState>(File.ReadAllBytes(path));
        if(state?.Version!=1 || state.Removed is not { Length: <=131072 } || state.Inventory is not { Length: <=4096 } ||
            state.Bodies is not { Length: <=65536 } || state.Receipts is not { Length: <=256 })
            throw new InvalidDataException("Invalid scene physics state.");
        if(state.Removed.Any(v=>v==0) || state.Removed.Distinct().Count()!=state.Removed.Length ||
            state.Inventory.Any(v=>string.IsNullOrWhiteSpace(v.ItemBase) || v.ItemBase.Length>128 || v.Count is 0 or >1000000) ||
            state.Inventory.Select(v=>v.ItemBase).Distinct(StringComparer.Ordinal).Count()!=state.Inventory.Length ||
            state.Bodies.Select(v=>v.SourceId).Distinct().Count()!=state.Bodies.Length || state.Bodies.Any(v=>!ValidPose(v)) ||
            state.Receipts.Any(v=>v.RequestId==0 || v.Reason is null || v.Reason.Length>128))
            throw new InvalidDataException("Scene physics state identities or poses are invalid.");
        return state;
    }
    private static bool ValidPose(HostSceneBodyPose p) => p.SourceId!=0 &&
        float.IsFinite(p.X) && float.IsFinite(p.Y) && float.IsFinite(p.Z) &&
        float.IsFinite(p.RotationX) && float.IsFinite(p.RotationY) && float.IsFinite(p.RotationZ) && float.IsFinite(p.RotationW) &&
        p.RotationX*p.RotationX+p.RotationY*p.RotationY+p.RotationZ*p.RotationZ+p.RotationW*p.RotationW is >0.99f and <1.01f;
    internal static void Save(string directory,ScenePhysicsSavedState state)
    {
        var bytes=JsonSerializer.SerializeToUtf8Bytes(state);
        if(bytes.Length>33554432)throw new InvalidOperationException("Scene physics state exceeds its bound.");
        var path=Path.Combine(directory,"scene_physics.state.json");
        var temporary=path+".tmp";
        using(var stream=new FileStream(temporary,FileMode.Create,FileAccess.Write,FileShare.None))
        {stream.Write(bytes);stream.Flush(flushToDisk:true);}
        File.Move(temporary,path,true);
    }
}
