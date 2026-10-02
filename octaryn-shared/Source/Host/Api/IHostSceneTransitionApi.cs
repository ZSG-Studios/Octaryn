namespace Octaryn.Shared.Host.Api;

public readonly record struct HostTransitionPose(double X,double Y,double Z,float Yaw,float Pitch)
{
    public bool IsValid => double.IsFinite(X) && double.IsFinite(Y) && double.IsFinite(Z) &&
        System.Math.Abs(X)<=1000000 && System.Math.Abs(Y)<=1000000 && System.Math.Abs(Z)<=1000000 &&
        float.IsFinite(Yaw) && float.IsFinite(Pitch) && System.Math.Abs(Yaw)<=1000000 && System.Math.Abs(Pitch)<=1.55f;
}
public readonly record struct HostTransitionView(string SceneAssetId,HostTransitionPose Pose,bool Enabled);
public enum HostTransitionState : uint { Queued,Loading,Completed,Failed }

// Local scene replacement is separate from CPU preparation. Completion means
// renderer/collision/session replacement succeeded; it is not gameplay parity.
public interface IHostSceneTransitionApi
{
    bool TryGetView(out HostTransitionView view);
    bool Begin(string sceneAssetId,in HostTransitionPose pose,out ulong revision,out string error);
    bool TryGetStatus(ulong revision,out HostTransitionState state,out string error);
    bool Cancel(ulong revision);
}
