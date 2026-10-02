namespace Octaryn.Shared.Host.Api;

internal static class HostGraphicsValidation
{
    internal static bool Valid(in HostGraphicsSettings settings) =>
        settings.WindowWidth is >= 64 and <= 16384 && settings.WindowHeight is >= 64 and <= 16384 &&
        (uint)settings.PresentMode <= 2 && (uint)settings.UpscalerMode <= 6 &&
        (settings.FrameCapFps <= 1 || settings.FrameCapFps is >= 30 and <= 240) &&
        settings.FsrTargetFps is >= 30 and <= 240 && (uint)settings.ShadowQuality <= 3 &&
        (uint)settings.ReflectionQuality <= 3 && settings.ShadowDistance <= 1024 && settings.ReflectionDistance <= 1024 &&
        Range(settings.FsrSharpness, 0, 1) && Range(settings.FsrRenderScale, 1f/3f, 1) &&
        Range(settings.FsrMinScale, 1f/3f, 1) && Range(settings.FsrMaxScale, settings.FsrMinScale, 1);

    private static bool Range(float value, float minimum, float maximum) =>
        float.IsFinite(value) && value >= minimum && value <= maximum;
}
