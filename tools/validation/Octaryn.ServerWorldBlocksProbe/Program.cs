return ServerWorldBlocksProbe.Run();

internal static partial class ServerWorldBlocksProbe
{
    public static int Run()
    {
        ValidateWorldConstants();
        ValidateEditAndQuery();
        ValidateSupportRules();
        ValidateFluidRules();
        ValidatePlayerSpawnAndWalkCollision();
        ValidateSnapshotOrder();
        ValidatePersistenceRoundTrip();
        ValidateCommandSink();
        ValidateClientCommandQueue();
        ValidateModuleCommandPath();
        ValidateSubmittedClientCommands();
        ValidateSnapshotDrain();
        ValidateActivatorPersistenceLifecycle();
        ValidateChunkPublication();
        ValidatePlayerStatePublicationClock();
        ValidateProcessSnapshotMode();
        ValidateReplicationBackpressure();
        ValidateFluidSimulation();
        ValidateWorldItems();
        return 0;
    }
}
