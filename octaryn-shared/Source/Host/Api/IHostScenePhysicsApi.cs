namespace Octaryn.Shared.Host.Api;

// Commands are asynchronous authority requests; accepting a request does not
// mean it succeeded. Inventory changes must wait for a successful receipt.
public interface IHostScenePhysicsApi
{
    bool SetAim(float x, float y, float z, float dx, float dy, float dz, float reach);
    bool TryGetTarget(out HostSceneBodyHit hit);
    ulong Grab(ulong sourceId, float hitX, float hitY, float hitZ,
        float targetX, float targetY, float targetZ);
    bool MoveGrab(float x, float y, float z);
    ulong Release();
    ulong Pickup(ulong sourceId);
    bool TryGetReceipt(ulong requestId, out HostScenePhysicsReceipt receipt);
    bool TryGetPose(ulong sourceId, out HostSceneBodyPose pose);
    bool TryGetInventory(out IReadOnlyList<HostSceneInventoryCount> inventory)
    { inventory=[];return false; }
}

public readonly record struct HostSceneBodyHit(ulong SourceId,
    float X, float Y, float Z, float NormalX, float NormalY, float NormalZ, float Distance);
public readonly record struct HostSceneBodyPose(ulong SourceId,
    float X, float Y, float Z, float RotationX, float RotationY, float RotationZ, float RotationW,
    float VelocityX, float VelocityY, float VelocityZ, uint Flags);
public readonly record struct HostScenePhysicsReceipt(ulong RequestId, ulong SourceId,
    bool Succeeded, string Reason);
