namespace Octaryn.Server;

// Explicit session file locations for one authoritative stream. The local
// client session resolves these from process environment variables; a remote
// session supplies its own per-connection paths so the same publication,
// tick and acknowledgement logic runs without a shared filesystem.
internal sealed record SessionFilePaths(
    string ChunkViewIntent,
    string ChunkStream,
    string? PlayerInputIntent,
    string? PlayerStateStream,
    string? BlockInteractionIntent,
    string? WorldTimeIntent,
    bool MetadataOnly);
