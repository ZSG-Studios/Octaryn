namespace Octaryn.Shared.Host.Api;

public interface IHostAudioApi
{
    // Plays a module-declared action sound. False when the sound is unknown
    // or no audio device is available.
    bool PlayActionSound(ulong assetIdHash, float volume, float positionX, float positionY, float positionZ);
}
