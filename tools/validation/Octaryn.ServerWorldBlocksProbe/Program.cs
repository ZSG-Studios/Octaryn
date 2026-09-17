if (args.Contains("--block-receipts")) return ServerWorldBlocksProbe.RunBlockReceiptQualification();
if (args.Contains("--chunk-publication")) return ServerWorldBlocksProbe.RunChunkPublicationQualification();
return ServerWorldBlocksProbe.Run();

internal static partial class ServerWorldBlocksProbe
{
    public static int RunChunkPublicationQualification() { ValidateProcessPublication(); return 0; }
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
