// Fixture-only schedule declarations. Production grant logic is linked directly.
namespace Octaryn.Shared.Host {public enum ScheduledAccessMode {Read,Write}}
namespace Octaryn.Shared.GameModules {
 public sealed class GameModuleManifest {public string[]? RequestedHostApis{get;init;} public Schedule? Schedule{get;init;}}
 public sealed class Schedule {public SystemDeclaration[]? Systems{get;init;}}
 public sealed class SystemDeclaration {public Resource[]? Reads{get;init;} public Resource[]? Writes{get;init;}}
 public sealed class Resource {public string ResourceId{get;init;}="";public Octaryn.Shared.Host.ScheduledAccessMode Mode{get;init;}}
}
