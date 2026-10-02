namespace Octaryn.Shared.Host.Api;

public enum HostPresentMode : uint { Immediate, VSync, TripleBuffered }
public enum HostUpscalerMode : uint { Off, Native, Quality, Balanced, Performance, UltraPerformance, Custom }
public enum HostGraphicsQuality : uint { Low, Medium, High, Ultra }
public enum HostGraphicsApplyResult : int { Applied, AppliedNotPersisted, Rejected, Unavailable }

// Requested preferences and separately observed dimensions/capabilities.
public readonly record struct HostGraphicsSettings
{
    public bool Fullscreen { get; init; }
    public uint WindowWidth { get; init; }
    public uint WindowHeight { get; init; }
    public HostPresentMode PresentMode { get; init; }
    // 0 uncapped, 1 display refresh, otherwise 30..240.
    public uint FrameCapFps { get; init; }
    public HostUpscalerMode UpscalerMode { get; init; }
    public bool FsrSharpening { get; init; }
    public float FsrSharpness { get; init; }
    public float FsrRenderScale { get; init; }
    public bool FsrDynamicResolution { get; init; }
    public float FsrMinScale { get; init; }
    public float FsrMaxScale { get; init; }
    public uint FsrTargetFps { get; init; }
    public bool RayTracing { get; init; }
    public bool Pbr { get; init; }
    public bool Pom { get; init; }
    public bool Fog { get; init; }
    public bool Clouds { get; init; }
    public bool SkyGradient { get; init; }
    public bool Stars { get; init; }
    public bool Sun { get; init; }
    public bool Moon { get; init; }
    public HostGraphicsQuality ShadowQuality { get; init; }
    public HostGraphicsQuality ReflectionQuality { get; init; }
    public uint ShadowDistance { get; init; }
    public uint ReflectionDistance { get; init; }
    // Readback only. Applying does not override these fields or promise GPU readiness.
    public uint RenderWidth { get; init; }
    public uint RenderHeight { get; init; }
    public uint DisplayWidth { get; init; }
    public uint DisplayHeight { get; init; }
    public bool RayTracingAvailable { get; init; }
}

public interface IHostGraphicsApi
{
    bool TryGet(out HostGraphicsSettings settings);
    // Applied means preferences accepted; the renderer consumes them on its next frame.
    HostGraphicsApplyResult Apply(in HostGraphicsSettings settings, bool persist);
}
