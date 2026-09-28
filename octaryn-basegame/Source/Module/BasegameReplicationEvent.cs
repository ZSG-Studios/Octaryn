namespace Octaryn.Basegame;

// Well-known replication change kinds emitted by the basegame module.
public static class BasegameReplicationEvent
{
    public const ulong ModuleHello = 0x6F637461_72626D68; // "octar bmh"

    public const uint ItemGranted = 1u;

    public const uint ItemDropped = 2u;

    // Look-target highlight: payload1 is the item id (0 clears), payload2 the
    // stack count. Emitted on change only.
    public const uint ItemTargeted = 3u;
}
