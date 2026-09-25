namespace Octaryn.Shared.Host;

public enum HostWorkPhase
{
    Input = 1,
    Gameplay = 2,
    Simulation = 3,
    Replication = 4,
    PresentationPrepare = 5,
    AssetProcessing = 6,
    PersistencePrepare = 7,
    Validation = 8
}
