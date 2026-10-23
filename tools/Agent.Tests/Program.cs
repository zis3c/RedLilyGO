using System.Text;
using Agent;

static class Program
{
    private static int Main()
    {
        var executer = new Executer();
        Run("chunked status frame", () => ChunkedStatusFrame(executer));
        Run("truncated frame", () => TruncatedFrame(executer));
        Run("oversized frame", () => OversizedFrame(executer));
        Run("transport fragmented reads", TransportFragmentedReads);
        Run("transport empty and invalid I/O", TransportEmptyAndInvalidIo);
        Console.WriteLine("AGENT_PROTOCOL: pass");
        return 0;
    }

    private static void ChunkedStatusFrame(Executer executer)
    {
        var request = Frame((byte)Executer.Command.RequestAgentStatus, Array.Empty<byte>());
        using var input = new ChunkedStream(request, 1);
        using var output = new MemoryStream();
        using var cts = new CancellationTokenSource();
        executer.ParseAndExecute(new DuplexStream(input, output), cts);

        var response = output.ToArray();
        Assert(response.Length > 5, "status response missing");
        Assert(response[0] == (byte)Executer.Command.AgentStatus, "wrong status tag");
        var length = BitConverter.ToInt32(response, 1);
        var status = Encoding.UTF8.GetString(response, 5, length).Split('\n');
        Assert(status.Length == 3 && status[1] == "0.1.0" && status[2] == "2", "status payload mismatch");
    }

    private static void TruncatedFrame(Executer executer)
    {
        using var input = new ChunkedStream(new byte[] { (byte)Executer.Command.DebugMsg, 2, 0 }, 1);
        using var cts = new CancellationTokenSource();
        Expect<EndOfStreamException>(() => executer.ParseAndExecute(input, cts));
    }

    private static void OversizedFrame(Executer executer)
    {
        var frame = new byte[] { (byte)Executer.Command.DebugMsg }
            .Concat(BitConverter.GetBytes(1024 * 1024 + 1))
            .ToArray();
        using var cts = new CancellationTokenSource();
        Expect<InvalidDataException>(() => executer.ParseAndExecute(new MemoryStream(frame), cts));
    }

    private static void TransportFragmentedReads()
    {
        var expected = Enumerable.Range(0, 70_000).Select(i => (byte)i).ToArray();
        var blocks = new Queue<byte[]>(new[] { expected[..3], expected[3..4], expected[4..] });
        using var stream = new TransportStream(CancellationToken.None,
            (_, _) => blocks.Count > 0 ? blocks.Dequeue() : Array.Empty<byte>(),
            _ => Task.CompletedTask);
        var actual = new byte[expected.Length];
        Assert(stream.Read(actual, 0, 2) == 2, "first read failed");
        Assert(stream.Read(actual, 2, 4) == 4, "split read failed");
        Assert(stream.Read(actual, 6, 65_536) == 65_536, "compaction read failed");
        Assert(stream.Read(actual, 65_542, actual.Length - 65_542) == actual.Length - 65_542, "tail read failed");
        Assert(actual.SequenceEqual(expected), "fragmented transport data was overwritten");
    }

    private static void TransportEmptyAndInvalidIo()
    {
        using var stream = new TransportStream(CancellationToken.None,
            (_, _) => throw new InvalidOperationException("Empty/invalid reads must not consume transport data"),
            _ => throw new InvalidOperationException("Empty writes must not reach transport"));
        Assert(!stream.CanSeek, "transport must report non-seekable");
        Assert(stream.Read(Array.Empty<byte>(), 0, 0) == 0, "empty read failed");
        stream.Write(Array.Empty<byte>(), 0, 0);
        stream.WriteAsync(Array.Empty<byte>(), 0, 0, CancellationToken.None).GetAwaiter().GetResult();
        Expect<ArgumentException>(() => stream.Read(new byte[1], 0, 2));
        Expect<ArgumentException>(() => stream.Write(new byte[1], 0, 2));
    }

    private static byte[] Frame(byte tag, byte[] data)
    {
        using var stream = new MemoryStream();
        stream.WriteByte(tag);
        stream.Write(BitConverter.GetBytes(data.Length));
        stream.Write(data);
        return stream.ToArray();
    }

    private static void Run(string name, Action test)
    {
        test();
        Console.WriteLine($"PASS {name}");
    }

    private static void Assert(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private static void Expect<T>(Action action) where T : Exception
    {
        try { action(); }
        catch (T) { return; }
        throw new InvalidOperationException($"Expected {typeof(T).Name}");
    }
}

sealed class ChunkedStream : MemoryStream
{
    private readonly int chunkSize;
    public ChunkedStream(byte[] buffer, int chunkSize) : base(buffer) => this.chunkSize = chunkSize;
    public override int Read(byte[] buffer, int offset, int count) => base.Read(buffer, offset, Math.Min(count, chunkSize));
}

sealed class DuplexStream : Stream
{
    private readonly Stream input;
    private readonly Stream output;
    public DuplexStream(Stream input, Stream output) { this.input = input; this.output = output; }
    public override bool CanRead => true;
    public override bool CanSeek => false;
    public override bool CanWrite => true;
    public override long Length => input.Length;
    public override long Position { get => input.Position; set => input.Position = value; }
    public override int Read(byte[] buffer, int offset, int count) => input.Read(buffer, offset, count);
    public override void Write(byte[] buffer, int offset, int count) => output.Write(buffer, offset, count);
    public override void Flush() => output.Flush();
    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
    public override void SetLength(long value) => throw new NotSupportedException();
}
