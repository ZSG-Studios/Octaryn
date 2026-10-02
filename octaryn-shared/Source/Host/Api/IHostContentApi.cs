namespace Octaryn.Shared.Host.Api;

public interface IHostContentApi
{
    const int MaximumReadBytes = 4 * 1024 * 1024;

    // IDs resolve only against this module's validated Data/ declarations.
    bool TryRead(string contentId, out ReadOnlyMemory<byte> data, out string error);
}
