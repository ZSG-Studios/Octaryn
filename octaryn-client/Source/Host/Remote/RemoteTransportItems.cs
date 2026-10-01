using LiteNetLib;

namespace Octaryn.Client.Host.Remote;

internal sealed partial class RemoteTransportClient
{
    private readonly WorldItemSnapshots _worldItems = new();
    public WorldItemSnapshot WorldItems => _worldItems.Published;

    private void OnWorldItems(LiteNetPeer peer, ReadOnlySpan<byte> bytes)
    {
        if (!_worldItems.Accept(bytes, out var acknowledgement))
        {
            Fail("error: invalid authoritative world-item stream");
            return;
        }
        if (acknowledgement is not null) peer.Send(acknowledgement, DeliveryMethod.ReliableOrdered);
    }
}
