using Agent;

var payload = new byte[512 * 1024];
Random.Shared.NextBytes(payload);
using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30));
using var received = new MemoryStream();
using var transport = new TransportStream(timeout.Token,
    (_, _) => Array.Empty<byte>(),
    async block =>
    {
        await Task.Delay(5, timeout.Token);
        lock (received) received.Write(block);
    });

await Task.Run(() => transport.Write(payload, 0, payload.Length), timeout.Token);
while (true)
{
    lock (received) if (received.Length >= payload.Length) break;
    timeout.Token.ThrowIfCancellationRequested();
    await Task.Delay(10, timeout.Token);
}

if (!payload.AsSpan().SequenceEqual(received.ToArray()))
    throw new Exception("Remote transport changed frame data.");

Console.WriteLine("AGENT_TRANSPORT: pass");
