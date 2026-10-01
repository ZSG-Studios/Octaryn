namespace Octaryn.Shared.Host;

// Generic host command envelope kind; games define command semantics over
// the opaque payload fields.
public enum HostCommandKind : uint
{
    None = 0
}
