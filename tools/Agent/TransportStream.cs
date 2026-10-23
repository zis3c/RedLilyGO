using System.Collections.Concurrent;

namespace Agent
{
    internal interface IAuthenticatedTransport
    {
    }

    internal class TransportStream : Stream, IDisposable, IAuthenticatedTransport
    {
        private readonly CancellationTokenSource cts;
        private readonly MemoryStream DataToStreamOwner = new();
        private readonly object readLock = new();
        private const int MaxPendingWriteBytes = 256 * 1024;
        private const int MaxWriteBatchBytes = 32 * 1024;
        private readonly BlockingCollection<byte[]> blocksToWrite =
            new(new ConcurrentQueue<byte[]>(), MaxPendingWriteBytes / MaxWriteBatchBytes);

        private readonly Func<int, CancellationToken, byte[]> ReadBlock;
        private readonly Func<byte[], Task> WriteBlock;

        private readonly Task Writer;
        private int _Disposed;

        public TransportStream(CancellationToken token, Func<int, CancellationToken, byte[]> readBlock, Func<byte[], Task> writeBlock)
        {
            ReadBlock = readBlock;
            WriteBlock = writeBlock;

            cts = CancellationTokenSource.CreateLinkedTokenSource(token);

            Writer = Task.Run(async () =>
            {
                while (!cts.IsCancellationRequested)
                {
                    try
                    {
                        var buffer = blocksToWrite.Take(cts.Token);

                        using var batch = new MemoryStream(Math.Min(MaxWriteBatchBytes, buffer.Length));
                        batch.Write(buffer, 0, buffer.Length);
                        while (batch.Length < MaxWriteBatchBytes && blocksToWrite.TryTake(out buffer))
                        {
                            batch.Write(buffer, 0, buffer.Length);
                        }
                        await WriteBlock(batch.ToArray());
                    }
                    catch (OperationCanceledException) when (cts.IsCancellationRequested)
                    {
                        break;
                    }
                    catch (Exception ex)
                    {
#if DEBUG
                        Console.WriteLine("Error in transport stream: "+ex.Message);
#endif
                        Console.WriteLine("Error in transport stream: "+ex.Message);
                        cts.Cancel();
                        throw;
                    }
                }
            }, cts.Token);
        }

        public override bool CanRead => true;

        public override bool CanSeek => false;

        public override bool CanWrite => true;

        public override long Length => DataToStreamOwner.Length;

        public override long Position { get => DataToStreamOwner.Position; set => throw new NotImplementedException(); }

        public override void Flush()
        {
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            ValidateBufferArguments(buffer, offset, count);
            lock (readLock)
            {
                ThrowIfDisposed();
                var maxRead = DataToStreamOwner.Length - DataToStreamOwner.Position;
                while (count > maxRead)
                {
                    var block = ReadBlock((int)(count - maxRead), cts.Token);
                    if (block == null || block.Length == 0)
                    {
                        throw new EndOfStreamException("Remote transport closed while reading.");
                    }
                    var readPosition = DataToStreamOwner.Position;
                    DataToStreamOwner.Position = DataToStreamOwner.Length;
                    DataToStreamOwner.Write(block);
                    DataToStreamOwner.Position = readPosition;
                    maxRead = DataToStreamOwner.Length - DataToStreamOwner.Position;
                }

                var read = DataToStreamOwner.Read(buffer, offset, count);
                if (DataToStreamOwner.Position >= 64 * 1024)
                {
                    var remaining = DataToStreamOwner.Length - DataToStreamOwner.Position;
                    var tail = new byte[remaining];
                    if (remaining > 0) DataToStreamOwner.Read(tail, 0, (int)remaining);
                    DataToStreamOwner.SetLength(0);
                    DataToStreamOwner.Position = 0;
                    if (tail.Length > 0) DataToStreamOwner.Write(tail, 0, tail.Length);
                    DataToStreamOwner.Position = 0;
                }
                return read;
            }
        }

        public override long Seek(long offset, SeekOrigin origin)
        {
            throw new NotImplementedException();
        }

        public override void SetLength(long value)
        {
            throw new NotImplementedException();
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            ValidateBufferArguments(buffer, offset, count);
            ThrowIfDisposed();
            if (count == 0) return;
            while (count > 0)
            {
                var length = Math.Min(count, MaxWriteBatchBytes);
                byte[] chunk = new byte[length];
                Buffer.BlockCopy(buffer, offset, chunk, 0, length);
                blocksToWrite.Add(chunk, cts.Token);
                offset += length;
                count -= length;
            }
        }

        public override Task WriteAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken)
        {
            return Task.Run(() => Write(buffer, offset, count), cancellationToken);
        }

        ~TransportStream()
        {
            Dispose(false);
        }

        protected override void Dispose(bool disposing)
        {
            base.Dispose(disposing);
            if (Interlocked.Exchange(ref _Disposed, 1) == 0)
            {
                cts.Cancel();
                if (disposing)
                {
                    try { Writer.Wait(TimeSpan.FromSeconds(2)); }
                    catch (AggregateException) { }
                    catch (ObjectDisposedException) { }
                }
                lock (readLock)
                {
                    DataToStreamOwner.Dispose();
                }
                cts.Dispose();
            }
        }

        private void ThrowIfDisposed()
        {
            if (Volatile.Read(ref _Disposed) != 0)
                throw new ObjectDisposedException(nameof(TransportStream));
        }
    }
}
