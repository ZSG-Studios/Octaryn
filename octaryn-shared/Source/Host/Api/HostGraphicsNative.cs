using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host.Api;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
internal unsafe struct HostGraphicsApiTable
{
    public uint Version, Size;
    public delegate* unmanaged[Cdecl]<HostGraphicsNative*, int> Get;
    public delegate* unmanaged[Cdecl]<HostGraphicsNative*, uint, int> Apply;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 88)]
internal struct HostGraphicsNative
{
    public uint Version, Size, Flags, Capabilities;
    public uint WindowWidth, WindowHeight, PresentMode, FrameCapFps, UpscalerMode;
    public uint ReflectionQuality, ShadowQuality, ShadowDistance, ReflectionDistance, FsrTargetFps;
    public float FsrSharpness, FsrRenderScale, FsrMinScale, FsrMaxScale;
    public uint RenderWidth, RenderHeight, DisplayWidth, DisplayHeight;

    internal readonly HostGraphicsSettings Managed() => new()
    {
        Fullscreen = (Flags & 1) != 0, FsrSharpening = (Flags & 2) != 0,
        FsrDynamicResolution = (Flags & 4) != 0, RayTracing = (Flags & 8) != 0,
        Pbr = (Flags & 16) != 0, Pom = (Flags & 32) != 0, Fog = (Flags & 64) != 0,
        Clouds = (Flags & 128) != 0, SkyGradient = (Flags & 256) != 0,
        Stars = (Flags & 512) != 0, Sun = (Flags & 1024) != 0, Moon = (Flags & 2048) != 0,
        WindowWidth = WindowWidth, WindowHeight = WindowHeight, PresentMode = (HostPresentMode)PresentMode,
        FrameCapFps = FrameCapFps, UpscalerMode = (HostUpscalerMode)UpscalerMode,
        ReflectionQuality = (HostGraphicsQuality)ReflectionQuality, ShadowQuality = (HostGraphicsQuality)ShadowQuality,
        ShadowDistance = ShadowDistance, ReflectionDistance = ReflectionDistance, FsrTargetFps = FsrTargetFps,
        FsrSharpness = FsrSharpness, FsrRenderScale = FsrRenderScale, FsrMinScale = FsrMinScale, FsrMaxScale = FsrMaxScale,
        RenderWidth = RenderWidth, RenderHeight = RenderHeight, DisplayWidth = DisplayWidth, DisplayHeight = DisplayHeight,
        RayTracingAvailable = (Capabilities & 1) != 0
    };
    internal static HostGraphicsNative From(in HostGraphicsSettings settings) => new()
    {
        Version = 1, Size = 88,
        Flags = (settings.Fullscreen ? 1u : 0) | (settings.FsrSharpening ? 2u : 0) |
            (settings.FsrDynamicResolution ? 4u : 0) | (settings.RayTracing ? 8u : 0) |
            (settings.Pbr ? 16u : 0) | (settings.Pom ? 32u : 0) | (settings.Fog ? 64u : 0) |
            (settings.Clouds ? 128u : 0) | (settings.SkyGradient ? 256u : 0) |
            (settings.Stars ? 512u : 0) | (settings.Sun ? 1024u : 0) | (settings.Moon ? 2048u : 0),
        WindowWidth = settings.WindowWidth, WindowHeight = settings.WindowHeight, PresentMode = (uint)settings.PresentMode,
        FrameCapFps = settings.FrameCapFps, UpscalerMode = (uint)settings.UpscalerMode,
        ReflectionQuality = (uint)settings.ReflectionQuality, ShadowQuality = (uint)settings.ShadowQuality,
        ShadowDistance = settings.ShadowDistance, ReflectionDistance = settings.ReflectionDistance,
        FsrTargetFps = settings.FsrTargetFps, FsrSharpness = settings.FsrSharpness,
        FsrRenderScale = settings.FsrRenderScale, FsrMinScale = settings.FsrMinScale, FsrMaxScale = settings.FsrMaxScale
    };
}
