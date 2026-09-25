namespace Octaryn.Shared.World;

public sealed record BlockReceiptCell(int X, int Y, int Z, ushort Block);
public sealed record BlockReceipt(ulong Sequence, ulong CommandID, bool Accepted,
    ulong Revision, IReadOnlyList<BlockReceiptCell> Blocks);
public sealed record BlockReceiptBatch(int Version, string Session, IReadOnlyList<BlockReceipt> Receipts);
public sealed record BlockReceiptAck(int Version, string Session, ulong Sequence);
