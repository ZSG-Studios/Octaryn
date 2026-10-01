using System.Reflection;
using System.Text.Json;

internal static class CommandDependencyProbe
{
    internal static void Run(Assembly assembly, string evidence)
    {
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
        var type = assembly.GetType("Octaryn.Server.PlayerCommandQueue", true)!;
        double now = 0;
        var queue = Activator.CreateInstance(type, flags, null, [new Func<double>(() => now)], null)!;
        object? Call(string name, params object?[] args) => type.GetMethod(name, flags)!.Invoke(queue, args);
        ulong Property(string name) => (ulong)type.GetProperty(name, flags)!.GetValue(queue)!;
        var path = Path.Combine(evidence, "command-dependency.json");
        void Accept(ulong sequence) {
            File.WriteAllText(path, JsonSerializer.Serialize(new { version = 2, commands = new[] {
                new { frameIndex = sequence, flags = 0, controller = 1, moveX = 1, moveY = 0, moveZ = 0,
                    cameraPitch = 0, cameraYaw = 0, relativeMouse = 1 } } }));
            Call("Read", path);
        }
        void Check(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
        Accept(1);
        Call("SetDependencyBlocked", true);
        now = 10;
        Call("Accrue");
        Accept(2);
        object?[] selection = [1ul, null];
        Check(!(bool)Call("TrySelect", selection)! && Property("Acknowledged") == 0,
              "Collision wait selected or acknowledged a command");
        Check((bool)Call("TrySelectForReadiness", selection)! && Property("SelectedSequence") == 1 && Property("Acknowledged") == 0,
              "Collision readiness could not inspect the held command without acknowledging it");
        Call("SetDependencyBlocked", false);
        Check((bool)Call("TrySelect", selection)! && Property("SelectedSequence") == 1,
              "Collision wait expired the original command");
        Call("Commit", selection[1]);
        Check(Property("Acknowledged") == 1, "First resumed command ACK was not contiguous");
        selection = [2ul, null];
        Check((bool)Call("TrySelect", selection)! && Property("SelectedSequence") == 2,
              "Command arriving during collision wait was lost");
        Call("Commit", selection[1]);
        Check(Property("Acknowledged") == 2, "Second resumed command ACK was not contiguous");
        Accept(3);
        now += 1;
        Call("Accrue");
        selection = [3ul, null];
        Check((bool)Call("TrySelect", selection)! && Property("SelectedSequence") == 0,
              "Ordinary transport expiration changed");
        Call("Commit", selection[1]);
        Check(Property("Acknowledged") == 3, "Ordinary expired command did not retire");
        Console.WriteLine("command_dependency=passed hold_seconds=10 ordered_commands=2 ordinary_expiry=1");
    }
}
