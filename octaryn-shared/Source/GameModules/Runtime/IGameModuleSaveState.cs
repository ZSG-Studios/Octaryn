namespace Octaryn.Shared.GameModules;

// The host owns files and save generations. Modules only encode their state.
public interface IGameModuleSaveState
{
    const int MaximumBytes = 1024 * 1024;

    int CaptureSaveState(Span<byte> destination);

    // Validate the complete snapshot before changing live state.
    void RestoreSaveState(ReadOnlySpan<byte> source);

    // Persisted by the host before the session can issue receipt identities.
    void BeginSaveSession(uint sessionId);
}
