namespace Octaryn.Shared.Host.Api;

// Sound ids accepted by IHostAudioApi.PlayActionSound. The client audio
// host admits these exact four built-in IDs. Other values are rejected,
// including hashes or form IDs of source-game sound assets.
public static class HostActionSoundIds
{
    public const ulong Place = 0;
    public const ulong Break = 1;
    public const ulong Select = 2;
    public const ulong Change = 3;
}
