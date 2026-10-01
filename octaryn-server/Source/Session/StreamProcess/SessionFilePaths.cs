namespace Octaryn.Server.Session;

// Session mailbox files the live dedicated-server loop reads and writes.
internal sealed record SessionFilePaths(
    string ChunkViewIntent,
    string? PlayerInputIntent,
    string? PlayerStateStream,
    string? WorldTimeIntent,
    string? UiActionIntent = null);
