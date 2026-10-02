namespace Octaryn.Shared.Host.Api;

public interface IHostAudioApi
{
    // Plays an exact HostActionSoundIds built-in, with finite volume in [0,1].
    // v1 action tones are nonspatial; finite position is retained for ABI
    // compatibility. Arbitrary clips and unavailable audio return false.
    bool PlayActionSound(ulong assetIdHash, float volume, float positionX, float positionY, float positionZ);
}
