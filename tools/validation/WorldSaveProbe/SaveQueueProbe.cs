using Octaryn.Server.Persistence.World;

internal static class SaveQueueProbe
{
    internal static void Run()
    {
        using var gate = new ManualResetEventSlim();
        var writes = new List<int>();
        using (var queue = new OrderedSaveQueue<int>(value => { gate.Wait(); writes.Add(value); }, "save-probe"))
        {
            if (!queue.TryEnqueue(1) || !queue.TryEnqueue(2) || queue.TryEnqueue(3))
                throw new InvalidOperationException("Save queue bound failed.");
            gate.Set(); queue.Flush();
            if (queue.Completed?.Sequence != 2 || !writes.SequenceEqual(new[] { 1, 2 }))
                throw new InvalidOperationException("Save acknowledgements/order failed.");
        }
        var failing = new OrderedSaveQueue<int>(_ => throw new IOException("fixture disk failure"), "save-failure-probe");
        failing.TryEnqueue(1);
        try { failing.Flush(); throw new InvalidOperationException("Disk failure was swallowed."); }
        catch (IOException) { }
        try { failing.Dispose(); throw new InvalidOperationException("Disk failure was swallowed during shutdown."); }
        catch (IOException) { }
        Console.WriteLine("world_save_queue status=passed bounded=2 ordered=1 acknowledged=1 failures_surface=1");
    }
}
