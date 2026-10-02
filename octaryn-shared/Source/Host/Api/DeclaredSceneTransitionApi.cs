using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal interface ISceneTransitionBackend
{
    bool TryGetView(out HostTransitionView view);
    bool Begin(string assetId,string descriptor,in HostTransitionPose pose,out ulong revision,out string error);
    bool TryGetStatus(ulong revision,out HostTransitionState state,out string error);
    bool Cancel(ulong revision);
}

internal sealed class DeclaredSceneTransitionApi : IHostSceneTransitionApi,IDisposable
{
    private readonly ISceneTransitionBackend _backend;
    private readonly Dictionary<string,string> _assets=new(StringComparer.Ordinal);
    private readonly HashSet<ulong> _revisions=[];
    private readonly string _root;
    private readonly bool _read,_write;
    private bool _disposed;
    internal DeclaredSceneTransitionApi(ISceneTransitionBackend backend,GameModuleManifest manifest,string root)
    {
        _backend=backend;_root=Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
        var requested=(manifest.RequestedHostApis??[]).Contains(HostApiIds.Transition,StringComparer.Ordinal);
        _write=requested && (manifest.Schedule?.Systems??[]).Any(s=>(s.Writes??[]).Any(r=>
            r.ResourceId==HostApiIds.Transition && r.Mode==Host.ScheduledAccessMode.Write));
        _read=_write || requested && (manifest.Schedule?.Systems??[]).Any(s=>(s.Reads??[]).Any(r=>
            r.ResourceId==HostApiIds.Transition && r.Mode==Host.ScheduledAccessMode.Read));
        foreach(var asset in manifest.AssetDeclarations??[])
            if(asset.AssetKind=="model" && asset.AssetId.StartsWith(manifest.ModuleId+".",StringComparison.Ordinal) && Safe(asset.RelativePath))
                _assets.Add(asset.AssetId,asset.RelativePath);
    }
    public bool TryGetView(out HostTransitionView view)
    {view=default;return !_disposed && _read && _backend.TryGetView(out view) && view.Pose.IsValid &&
        (view.SceneAssetId=="host.menu" || _assets.ContainsKey(view.SceneAssetId));}
    public bool Begin(string sceneAssetId,in HostTransitionPose pose,out ulong revision,out string error)
    {
        revision=0;error="Transition requires a declared scene and scheduled write grant.";
        if(_disposed || !_write || !pose.IsValid || sceneAssetId is null || !_assets.TryGetValue(sceneAssetId,out var relative))return false;
        try
        {
            var path=Path.GetFullPath(Path.Combine(_root,relative));
            var comparison=OperatingSystem.IsWindows()?StringComparison.OrdinalIgnoreCase:StringComparison.Ordinal;
            if(!path.StartsWith(_root+Path.DirectorySeparatorChar,comparison))return false;
            var current=_root;RejectLink(current);
            foreach(var part in relative.Split('/')){current=Path.Combine(current,part);RejectLink(current);}
            var info=new FileInfo(path);
            if(info.Length is <=0 or >4*1024*1024){error="Scene descriptor exceeds metadata admission.";return false;}
            if(_revisions.Count>=64){error="Transition lifetime revision budget exceeded.";return false;}
            if(!_backend.Begin(sceneAssetId,path,in pose,out revision,out error))return false;
            if(revision==0){error="Transition backend returned an invalid revision.";return false;}
            _revisions.Add(revision);return true;
        }
        catch(Exception failure) when(failure is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException)
        {error="Declared scene descriptor unavailable: "+failure.Message;return false;}
    }
    public bool TryGetStatus(ulong revision,out HostTransitionState state,out string error)
    {
        state=default;error="Transition revision does not belong to this activation.";
        if(_disposed || !_read || !_revisions.Contains(revision))return false;
        var found=_backend.TryGetStatus(revision,out state,out error);
        if(found && state is HostTransitionState.Completed or HostTransitionState.Failed)_revisions.Remove(revision);
        return found;
    }
    public bool Cancel(ulong revision)=>!_disposed && _write && _revisions.Contains(revision) && _backend.Cancel(revision);
    public void Dispose(){if(_disposed)return;foreach(var revision in _revisions)_backend.Cancel(revision);_disposed=true;_revisions.Clear();}
    private static bool Safe(string path)=>!string.IsNullOrWhiteSpace(path) && path.StartsWith("Assets/",StringComparison.Ordinal) &&
        !path.Contains('\\') && !path.Contains(':') && !path.Contains('\0') && !Path.IsPathRooted(path) &&
        path.Split('/').All(p=>p.Length>0 && p is not "." and not ".." && !p.EndsWith('.') && !p.EndsWith(' '));
    private static void RejectLink(string path)
    {if((File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)throw new IOException("Scene paths cannot traverse symbolic links.");}
}
