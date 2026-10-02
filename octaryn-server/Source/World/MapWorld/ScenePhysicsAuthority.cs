using System.Text.Json;
using Octaryn.Server.Simulation.Players;
using Octaryn.Shared.Host.Api;
namespace Octaryn.Server.World.MapWorld;

// Runs at the authority tick barrier. Local clients communicate through bounded,
// acknowledged journals; no client may create bodies or author their poses.
internal sealed unsafe class ScenePhysicsAuthority : IDisposable
{
    private readonly IntPtr _world;
    private readonly string _directory;
    private readonly string _scenePath;
    private readonly Dictionary<ulong,SceneBodyEntry> _bodies;
    private readonly HashSet<ulong> _removed=[];
    private readonly Dictionary<string,uint> _inventory=new(StringComparer.Ordinal);
    private readonly Dictionary<ulong,HostSceneBodyPose> _savedPoses=[];
    private double _saveAge;
    private ulong _savedReceiptEpoch;
    private HostScenePhysicsReceipt[] _savedReceipts=[];
    private int _snapshotFailures;
    private readonly Queue<HostScenePhysicsReceipt> _receipts=new(256);
    private ulong _epoch,_sequence,_grabbed;
    private HostSceneBodyHit? _target;
    private ScenePhysicsCommand? _aim;
    private double _aimAge=1,_grabAge=1;
    internal ScenePhysicsAuthority(IntPtr world,string directory,string glb)
    {
        _world=world;_directory=Path.GetFullPath(directory);
        _scenePath=Path.GetFullPath(glb);
        Directory.CreateDirectory(_directory);
        _bodies=SceneBodyCatalog.Load(world,glb);
        var state=ScenePhysicsPersistence.Load(_directory);
        foreach(var source in state.Removed)_removed.Add(source);
        foreach(var item in state.Inventory)_inventory.Add(item.ItemBase,item.Count);
        foreach(var pose in state.Bodies)_savedPoses.Add(pose.SourceId,pose);
        _savedReceiptEpoch=state.ReceiptEpoch;_savedReceipts=state.Receipts;
        foreach(var pair in _bodies)
        {
            if(!_savedPoses.TryGetValue(pair.Key,out var pose))continue;
            pair.Value.Source.Position=[pose.X,pose.Y,pose.Z];
            pair.Value.Source.Rotation=[pose.RotationX,pose.RotationY,pose.RotationZ,pose.RotationW];
        }
        LiveDebugLog.Write($"server_scene_physics_catalog active=1 bodies={_bodies.Count} source={_scenePath}");
    }
    internal void Tick(double dt,ulong tick,PlayerState player)
    {
        if(!double.IsFinite(dt) || dt<=0 || dt>0.25)return;
        var activated=0;
        foreach(var entry in _bodies.Values)
        {
            if(entry.Handle!=0 || _removed.Contains(entry.Source.SourceId) ||
                !NearPlayer(entry.Source.Position[0],entry.Source.Position[1],entry.Source.Position[2],player,32))continue;
            if(SceneBodyCatalog.TryCreate(_world,entry.Source,out var handle))entry.Handle=handle;
            if(++activated==32)break;
        }
        _aimAge+=dt;_grabAge+=dt;
        ReadCommands(player);
        if(_grabbed!=0 && _grabAge>0.25){NativeMapWorld.ReleaseBodyGrab(_world);_grabbed=0;}
        var physicsStatus=_bodies.Count>0 ? NativeMapWorld.StepBodies(_world,dt) : 0;
        if(physicsStatus is not (0 or 2))throw new InvalidOperationException("Scene physics authority step failed.");
        _saveAge+=dt;
        if(_saveAge>=1){SaveState();_saveAge=0;}
        RefreshTarget(player);
        var poses=new List<HostSceneBodyPose>(_bodies.Count);
        foreach(var pair in _bodies)
        {
            if(_removed.Contains(pair.Key))
            {
                var s=pair.Value.Source;poses.Add(new(pair.Key,s.Position[0],s.Position[1],s.Position[2],s.Rotation[0],s.Rotation[1],s.Rotation[2],s.Rotation[3],0,0,0,4));
            }
            else if(pair.Value.Handle!=0 && NativeMapWorld.BodyPose(_world,pair.Value.Handle,out var native))poses.Add(native.Managed());
            else
            {
                var s=pair.Value.Source;
                poses.Add(new(pair.Key,s.Position[0],s.Position[1],s.Position[2],s.Rotation[0],s.Rotation[1],s.Rotation[2],s.Rotation[3],0,0,0,9));
            }
        }
        var snapshot=new ScenePhysicsSnapshot{ScenePath=_scenePath,ResidencyPending=physicsStatus==2,Epoch=_epoch,Seq=_sequence,Tick=tick,Target=_target,
            Bodies=poses.ToArray(),Receipts=_receipts.ToArray(),Inventory=_inventory.Select(v=>new HostSceneInventoryCount(v.Key,v.Value)).ToArray()};
        var path=Path.Combine(_directory,"scene_physics.snapshot.json");
        var temporary=path+".tmp";
        try
        {
            File.WriteAllBytes(temporary,JsonSerializer.SerializeToUtf8Bytes(snapshot));File.Move(temporary,path,true);
            _snapshotFailures=0;
        }
        catch(Exception error) when(error is IOException or UnauthorizedAccessException)
        {
            // Windows readers can briefly deny replacement; retain the journal
            // and retry next tick, with a bounded failure rather than lost acks.
            if(++_snapshotFailures>=120)throw new InvalidOperationException("Scene physics snapshot publication stalled.",error);
        }
    }
    private void ReadCommands(PlayerState player)
    {
        var path=Path.Combine(_directory,"scene_physics.commands.json");
        try
        {
            if(!File.Exists(path) || new FileInfo(path).Length>131072)return;
            var journal=JsonSerializer.Deserialize<ScenePhysicsCommandJournal>(File.ReadAllBytes(path));
            if(journal?.Version!=1 || journal.Epoch==0 || journal.Commands.Length>256)return;
            if(journal.Epoch!=_epoch)
            {
                NativeMapWorld.ReleaseBodyGrab(_world);_grabbed=0;
                if(journal.Seq<(ulong)journal.Commands.Length)return;
                _epoch=journal.Epoch;_sequence=journal.Seq-(ulong)journal.Commands.Length;
                _receipts.Clear();_target=null;_aim=null;
                if(_epoch==_savedReceiptEpoch)
                    foreach(var receipt in _savedReceipts)_receipts.Enqueue(receipt);
            }
            if(journal.Seq<=_sequence)return;
            var pending=journal.Seq-_sequence;
            if(pending>(ulong)journal.Commands.Length)return;
            var start=journal.Commands.Length-(int)pending;
            for(var i=start;i<journal.Commands.Length;++i)
            {
                Handle(journal.Commands[i],player);++_sequence;
            }
        }
        catch(IOException) { }
        catch(JsonException) { }
    }
    private static bool Finite(ScenePhysicsCommand c) =>
        float.IsFinite(c.X) && float.IsFinite(c.Y) && float.IsFinite(c.Z) && float.IsFinite(c.Dx) &&
        float.IsFinite(c.Dy) && float.IsFinite(c.Dz) && float.IsFinite(c.Reach) &&
        float.IsFinite(c.HitX) && float.IsFinite(c.HitY) && float.IsFinite(c.HitZ);
    private static bool NearPlayer(float x,float y,float z,PlayerState p,float distance)
    {
        var dx=x-p.X;var dy=y-(p.Y+0.85f);var dz=z-p.Z;
        return dx*dx+dy*dy+dz*dz<=distance*distance;
    }
    private void RefreshTarget(PlayerState player)
    {
        _target=null;
        if(_aim is not { } c || _aimAge>0.25 || !NearPlayer(c.X,c.Y,c.Z,player,2))return;
        var origin=stackalloc float[3]{c.X,c.Y,c.Z};var direction=stackalloc float[3]{c.Dx,c.Dy,c.Dz};
        if(NativeMapWorld.BodyRay(_world,origin,direction,c.Reach,out var hit))_target=hit.Managed();
    }
    private void Handle(ScenePhysicsCommand c,PlayerState player)
    {
        if(c.RequestId==0)return;
        if(_receipts.Any(v=>v.RequestId==c.RequestId))return;
        if(!Finite(c)){Receipt(c,false,"invalid_request");return;}
        if(c.Kind=="aim")
        {
            if(c.Reach is >0 and <=3 && NearPlayer(c.X,c.Y,c.Z,player,2)) {_aim=c;_aimAge=0;RefreshTarget(player);}
            return;
        }
        if(c.Kind=="release")
        {
            NativeMapWorld.ReleaseBodyGrab(_world);_grabbed=0;Receipt(c,true,"");return;
        }
        if(c.Kind=="move")
        {
            if(_grabbed==0 || !NearPlayer(c.X,c.Y,c.Z,player,3.5f))return;
            var target=stackalloc float[3]{c.X,c.Y,c.Z};
            if(NativeMapWorld.MoveBodyGrab(_world,target))_grabAge=0;
            return;
        }
        RefreshTarget(player);
        if(_target is not { } selected || selected.SourceId!=c.SourceId || !_bodies.TryGetValue(c.SourceId,out var body) || _removed.Contains(c.SourceId))
        {Receipt(c,false,"target_out_of_reach_or_occluded");return;}
        if(c.Kind=="pickup")
        {
            if(!body.Source.PickupAllowed){Receipt(c,false,"not_collectible");return;}
            var item=body.Source.ItemBase;
            _inventory.TryGetValue(item,out var held);
            if((ulong)held+body.Source.Count>1000000 || held==0 && _inventory.Count>=4096)
            {Receipt(c,false,"inventory_capacity");return;}
            if(NativeMapWorld.RemoveBody(_world,body.Handle)!=0){Receipt(c,false,"remove_failed");return;}
            _inventory[item]=held+body.Source.Count;
            _removed.Add(c.SourceId);if(_grabbed==c.SourceId)_grabbed=0;
            SaveState(new(c.RequestId,c.SourceId,true,"")); // Commit the recoverable success receipt with the ledger.
            _target=null;Receipt(c,true,"");return;
        }
        if(c.Kind=="grab")
        {
            if(!NearPlayer(c.X,c.Y,c.Z,player,3.5f)){Receipt(c,false,"grab_target_out_of_reach");return;}
            // Use the freshly raycast surface point; never trust a client anchor.
            var hit=stackalloc float[3]{selected.X,selected.Y,selected.Z};var target=stackalloc float[3]{c.X,c.Y,c.Z};
            var ok=NativeMapWorld.GrabBody(_world,body.Handle,hit,target,Math.Clamp(body.Source.Mass*100,25,500));
            if(ok){_grabbed=c.SourceId;_grabAge=0;}
            Receipt(c,ok,ok?"":"grab_failed");return;
        }
        Receipt(c,false,"unknown_action");
    }
    private void Receipt(ScenePhysicsCommand c,bool success,string reason)
    {
        if(_receipts.Count==256)_receipts.Dequeue();
        _receipts.Enqueue(new(c.RequestId,c.SourceId,success,reason));
        LiveDebugLog.Write($"server_scene_physics_action request={c.RequestId} source={c.SourceId} kind={c.Kind} success={(success?1:0)} reason={reason}");
    }
    private void SaveState(HostScenePhysicsReceipt? pending=null)
    {
        foreach(var pair in _bodies)
        {
            if(_removed.Contains(pair.Key)){_savedPoses.Remove(pair.Key);continue;}
            if(pair.Value.Handle!=0 && NativeMapWorld.BodyPose(_world,pair.Value.Handle,out var pose))
                _savedPoses[pair.Key]=pose.Managed() with { Flags=0 };
        }
        var receipts=_epoch==0?_savedReceipts:_receipts.ToArray();
        if(pending is { } committed)receipts=[..receipts.TakeLast(255),committed];
        ScenePhysicsPersistence.Save(_directory,new ScenePhysicsSavedState{Removed=_removed.ToArray(),
            Inventory=_inventory.Select(v=>new HostSceneInventoryCount(v.Key,v.Value)).ToArray(),Bodies=_savedPoses.Values.ToArray(),
            ReceiptEpoch=_epoch==0?_savedReceiptEpoch:_epoch,Receipts=receipts});
    }
    public void Dispose()
    {
        if(_grabbed!=0)NativeMapWorld.ReleaseBodyGrab(_world);
        SaveState();
    }
}
