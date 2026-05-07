return ServerWorldBlocksProbe.Run();

internal static partial class ServerWorldBlocksProbe
{
    public static int Run()
    {
        ValidateWorldConstants();
        ValidateEditAndQuery();
        ValidateSupportRules();
        ValidatePlayerSpawnAndWalkCollision();
        ValidateSnapshotOrder();
        ValidatePersistenceRoundTrip();
        ValidateCommandSink();
        ValidateClientCommandQueue();
        ValidateModuleCommandPath();
        ValidateSubmittedClientCommands();
        ValidateSnapshotDrain();
        ValidateActivatorPersistenceLifecycle();
        return 0;
    }
}
