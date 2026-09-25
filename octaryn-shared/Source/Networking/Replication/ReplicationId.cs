namespace Octaryn.Shared.Networking;

public readonly record struct ReplicationId(ulong Value)
{
    public static ReplicationId None => new(0);
}
