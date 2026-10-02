namespace Octaryn.Shared.Host.Api;

// v2 optional capability: copies bounded signed interleaved PCM16, never opens files.
public interface IHostPcmAudioApi : IHostAudioApi
{
    bool RegisterPcm16(System.ReadOnlySpan<byte> samples,uint sampleRate,uint channels,out ulong clip);
    bool PlayClip(ulong clip,float volume,bool loop,bool nonspatial,float x,float y,float z,out ulong voice);
    bool StopVoice(ulong voice);
    bool ReleaseClip(ulong clip);
    bool TryGetVoicePlaying(ulong voice,out bool playing);
}
