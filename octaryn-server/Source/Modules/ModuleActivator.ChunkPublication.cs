namespace Octaryn.Server.Modules;

internal sealed partial class ModuleActivator
{
    // Publication and authority ticks run serially on the server session owner.
    // Pair the revision captured by the publication tracker with this exact store.
    internal void CaptureChunkPublicationRevision(ulong revision)
    {
        if (revision != BlockRevision) throw new InvalidOperationException("Chunk publication revision changed before capture.");
        _chunkColumns.AuthoritativeBlockRevision = revision;
    }
}
