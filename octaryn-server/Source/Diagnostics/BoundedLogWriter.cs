using System.Runtime.ExceptionServices;
using System.Threading.Channels;

namespace Octaryn.Server;

// Diagnostic lines may be dropped under pressure; authority receipts never use this queue.
internal sealed class BoundedLogWriter : IDisposable
{
    private readonly Channel<string> _queue;
    private readonly Thread _worker;
    private readonly Action<string>? _sink;
    private readonly string? _path;
    private readonly bool _console;
    private ExceptionDispatchInfo? _failure;
    private long _dropped;
    private int _disposed;

    public BoundedLogWriter(string? path, bool console = false, int capacity = 1024)
        : this(path, console, capacity, null) { }

    internal BoundedLogWriter(string? path, bool console, int capacity, Action<string>? sink)
    {
        _path = path;
        _console = console;
        _sink = sink;
        _queue = Channel.CreateBounded<string>(new BoundedChannelOptions(capacity)
        { SingleReader = true, FullMode = BoundedChannelFullMode.Wait });
        _worker = new Thread(Run) { IsBackground = true, Name = "octaryn-diagnostics" };
        _worker.Start();
    }

    public long Dropped => Interlocked.Read(ref _dropped);

    public bool TryWrite(string line)
    {
        Volatile.Read(ref _failure)?.Throw();
        if (line.Length <= 16384 && Volatile.Read(ref _disposed) == 0 && _queue.Writer.TryWrite(line)) return true;
        Interlocked.Increment(ref _dropped);
        return false;
    }

    private void Run()
    {
        try
        {
            if (!string.IsNullOrWhiteSpace(_path)) Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(_path))!);
            using var output = string.IsNullOrWhiteSpace(_path) ? null : new StreamWriter(_path, false);
            while (_queue.Reader.WaitToReadAsync().AsTask().GetAwaiter().GetResult())
            {
                while (_queue.Reader.TryRead(out var line))
                {
                    if (_console) Console.WriteLine(line);
                    output?.WriteLine(line);
                    _sink?.Invoke(line);
                }
                output?.Flush();
            }
            output?.Flush();
            if (Dropped != 0) Console.Error.WriteLine($"server_diagnostics_dropped lines={Dropped} path={_path}");
        }
        catch (Exception error) { Volatile.Write(ref _failure, ExceptionDispatchInfo.Capture(error)); }
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref _disposed, 1) != 0) return;
        _queue.Writer.TryComplete();
        _worker.Join();
        Volatile.Read(ref _failure)?.Throw();
    }
}
