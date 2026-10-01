namespace Octaryn.Shared.Host.Api;

// Sound ids accepted by IHostAudioApi.PlayActionSound. The client audio
// host maps the id onto its action-sound table by id % 4, in table order
// place, break, select, change (see octaryn.basegame action-sounds.json).
public static class HostActionSoundIds
{
    public const ulong Place = 0;
    public const ulong Break = 1;
    public const ulong Select = 2;
    public const ulong Change = 3;
}
