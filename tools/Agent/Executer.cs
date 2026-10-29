using Agent.TLV;
using Agent.Remote;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text;

namespace Agent
{
    internal static class AgentInfo
    {
        public const string Version = "0.1.1";
        public const string ProtocolVersion = "2";
    }

    internal class Executer
    {
        private readonly SemaphoreSlim commandGate = new(1, 1);
        private readonly SemaphoreSlim outputGate = new(1, 1);
        private const int MaxRemoteQueueItems = 64;
        private const int MaxCommandOutputChars = 8192;
        private const long MaxMicRecordingBytes = 64L * 1024 * 1024;
        private static readonly TimeSpan CommandTimeout = TimeSpan.FromSeconds(30);

        private BlockingCollection<byte[]> remoteDataBlocks = new();
        private RemoteServer? remoteServer;
        private TransportStream? remoteStream = null;

        public enum Command
        {
            Execute = 1,
            DebugMsg = 2,
            WSCONNECT = 3,
            WSDATA = 4,
            WSDISCONNECT = 5,
            WSDATARECV = 6,
            RequestAgentStatus = 7,
            AgentStatus = 8,
            ExecuteResult = 9,
            MicPcmData = 10
        }

        public Executer()
        {
        }

        private void CloseRemoteTransport()
        {
            var stream = remoteStream;
            remoteStream = null;
            stream?.Dispose();
        }

        public void ParseAndExecute(Stream stream, CancellationTokenSource cts)
        {
            var commandByte = stream.ReadByte();
            if (commandByte < 0)
            {
                throw new EndOfStreamException("Serial connection closed while reading command.");
            }

            var cmd = (Command)commandByte;

            byte[] lengthBytes = new byte[4];
            ReadExactly(stream, lengthBytes);
            var length = BitConverter.ToUInt32(lengthBytes, 0);
            if (length > 1024 * 1024)
            {
                throw new InvalidDataException($"Serial frame too large: {length} bytes.");
            }

            var data = new byte[length];

            if (length != 0)
            {
                ReadExactly(stream, data);
            }

            HandleTLV(cmd, data, stream, cts);
        }

        private static void ReadExactly(Stream stream, byte[] buffer)
        {
            var offset = 0;
            while (offset < buffer.Length)
            {
                var read = stream.Read(buffer, offset, buffer.Length - offset);
                if (read == 0)
                {
                    throw new EndOfStreamException("Serial connection closed during frame.");
                }

                offset += read;
            }
        }

        private async Task WriteFrameAsync(Command command, byte[] data, int offset, int count, Stream stream, CancellationToken token)
        {
            await outputGate.WaitAsync(token);
            try
            {
                await TLVHandling.WriteTLVToStream((byte)command, data, offset, count, stream, token);
            }
            finally
            {
                outputGate.Release();
            }
        }

        private static async Task<string> Run(string command, string arguments, CancellationToken token = default)
        {
            Process? process = null;
            try
            {
                using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(token);
                timeoutCts.CancelAfter(CommandTimeout);
                var startInfo = new ProcessStartInfo()
                {
                    FileName = command,
                    Arguments = arguments,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                process = new Process() { StartInfo = startInfo };
                    // Capture output with a hard bound while the process is still running.
                    // Truncating only after exit allows a noisy command to exhaust Agent RAM.
                    StringBuilder output = new StringBuilder(MaxCommandOutputChars);
                    object outputLock = new();
                    void AppendOutput(string? line)
                    {
                        if (line == null) return;
                        lock (outputLock)
                        {
                            if (output.Length >= MaxCommandOutputChars) return;
                            var remaining = MaxCommandOutputChars - output.Length;
                            if (line.Length > remaining) line = line[..remaining];
                            output.Append(line);
                            if (output.Length < MaxCommandOutputChars) output.Append('\n');
                        }
                    }

                    process.OutputDataReceived += (_, e) => AppendOutput(e.Data);
                    process.ErrorDataReceived += (_, e) => AppendOutput(e.Data);

                    process.Start();
                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();

                    await process.WaitForExitAsync(timeoutCts.Token);
                    process.WaitForExit();
                    lock (outputLock) return output.ToString();
            }
            catch (OperationCanceledException)
            {
                if (process != null && !process.HasExited)
                {
                    try { process.Kill(entireProcessTree: true); } catch { }
                }
                return "Command timed out or was canceled.";
            }
            catch (Exception ex)
            {
                return ex.Message;
            }
            finally
            {
                process?.Dispose();
            }
        }

        private async Task RunCommand(string command, Stream stream, CancellationToken token = default)
        {
            if (!await commandGate.WaitAsync(0, token))
            {
                return;
            }

            try
            {
#pragma warning disable CS0168 // Variable is declared but never used
                var output = await Run("cmd.exe", "/c "+command, token);
                var buffer = Encoding.UTF8.GetBytes(output);

                const int MAX_BUFFER_LENGTH = 8192;

                    // Bound the response so large command output cannot exhaust device memory.
                buffer = buffer.Take(MAX_BUFFER_LENGTH).ToArray();

                int dataSent = 0;
                while (dataSent < buffer.Length)
                {
                    var amountOfDataToSend = buffer.Length - dataSent > 2048 ? 2048 : buffer.Length - dataSent;
                    await WriteFrameAsync(Command.ExecuteResult, buffer, dataSent, amountOfDataToSend, stream, token);
                    await Task.Delay(50, token); // Pace chunks on the serial transport.
                    dataSent += amountOfDataToSend;
#if DEBUG
                        Console.WriteLine("OUT ExecuteResult)" + amountOfDataToSend);
#endif
                }
#if DEBUG
                Console.WriteLine("OUT ExecuteResult) complete");
#endif
#pragma warning restore CS0168 // Variable is declared but never used
            }
            catch (Exception ex)
            {
#if DEBUG
                Console.WriteLine("OUT ExecuteResult) " + ex.Message);
#else
                _ = ex;
#endif
            }
            finally
            {
                commandGate.Release();
            }
        }

        private void HandleTLV(Command cmd, byte[] data, Stream stream, CancellationTokenSource cts)
        {
            switch (cmd)
            {
                case Command.Execute:
                    var cmdLine = Encoding.UTF8.GetString(data);
#if DEBUG
                    Console.WriteLine("IN Execute) "+cmdLine);
#endif
                    _ = RunCommand(cmdLine, stream, cts.Token);
                    break;
                case Command.WSCONNECT:
                    global::Program.Log("VNC viewer connected to board");
#if DEBUG
                    Console.WriteLine("IN WSCONNECT");
#endif

                    CloseRemoteTransport();
                    remoteServer?.Dispose();
                    remoteServer = null;

                    try { remoteDataBlocks.CompleteAdding(); } catch (InvalidOperationException) { }
                    remoteDataBlocks = new BlockingCollection<byte[]>(new ConcurrentQueue<byte[]>(), MaxRemoteQueueItems);
                    remoteStream = new TransportStream(cts.Token, (count, token) =>
                    {
                        try
                        {
                            using (var cts2 = CancellationTokenSource.CreateLinkedTokenSource(cts.Token, token))
                            {
                                while (!cts2.IsCancellationRequested)
                                {
                                    return remoteDataBlocks.Take(cts2.Token);
                                }
                            }

                            throw new Exception("Timed out");
                        }
                        catch (OperationCanceledException)
                        {
                            throw;
                        }
                        catch
                        {
                            cts.Cancel();
                            throw;
                        }
                    },
                    async (buffer) =>
                    {
#pragma warning disable CS0168 // Variable is declared but never used
                        try
                        {
                            int dataSent = 0;
                            long totalBytesSent = 0;
                            while (dataSent < buffer.Length)
                            {
                                var amountOfDataToSend = buffer.Length - dataSent > 1024 ? 1024 : buffer.Length - dataSent;
                                await WriteFrameAsync(Command.WSDATARECV, buffer, dataSent, amountOfDataToSend, stream, cts.Token);
                                totalBytesSent += amountOfDataToSend;
                                if (totalBytesSent == amountOfDataToSend || totalBytesSent / 65536 > (totalBytesSent - amountOfDataToSend) / 65536)
                                    global::Program.Log($"VNC desktop data sent to board: {totalBytesSent} bytes");
                                await Task.Delay(25, cts.Token); // Pace frames so the board keeps its CDC frame boundaries.
                                dataSent += amountOfDataToSend;
#if DEBUG
                                Console.WriteLine("OUT WSDATARECV)" + amountOfDataToSend);
#endif
                            }

#if DEBUG
                            Console.WriteLine("OUT WSDATARECV) all sent");
#endif
                        }
                        catch (OperationCanceledException)
                        {
                            throw;
                        }
                        catch (Exception ex)
                        {
#if DEBUG
                            Console.WriteLine("OUT WSDATARECV) " + ex.Message);
#endif
                            cts.Cancel();
                            throw;
                        }
#pragma warning restore CS0168 // Variable is declared but never used
                    });
                    var sessionStream = remoteStream!;
                    var sessionServer = new RemoteServer(sessionStream.Dispose);
                    remoteServer = sessionServer;
                    _ = Task.Run(() =>
                    {
                        try
                        {
                            sessionServer.Start(sessionStream);
                        }
                        catch (Exception ex)
                        {
                            global::Program.Log($"Remote session error: {ex.Message}");
                            sessionStream.Dispose();
                        }
                    }, cts.Token);
                    break;
                case Command.WSDISCONNECT:
                    global::Program.Log("VNC viewer disconnected from board");
                    CloseRemoteTransport();
                    remoteServer?.Dispose();
                    remoteServer = null;
#if DEBUG
                    Console.WriteLine("IN WSDISCONNECT)");
#endif
                    break;
                case Command.WSDATA:
#if DEBUG
                    Console.WriteLine("IN WSDATA) "+data.Length);
#endif
                    try
                    {
                            remoteDataBlocks.TryAdd(data);
                    }
                    catch (InvalidOperationException)
                    {
                        // Ignore a late packet from a remote session that already closed.
                    }
                    break;
                case Command.DebugMsg:
                    var boardMessage = Encoding.UTF8.GetString(data).Trim();
                    if (!string.IsNullOrEmpty(boardMessage))
                        global::Program.Log($"Board: {boardMessage}");
#if DEBUG
                    Console.WriteLine("IN DebugMsg) "+ boardMessage);
#endif
                    break;
                case Command.RequestAgentStatus:
                    using (var ms = new MemoryStream())
                    {
                        var status = Encoding.UTF8.GetBytes($"{Environment.MachineName}\n{AgentInfo.Version}\n{AgentInfo.ProtocolVersion}");
                        WriteFrameAsync(Command.AgentStatus, status, 0, status.Length, stream, cts.Token).Wait();
                    }
                    break;
                case Command.MicPcmData:
                    const string filename = "mic.pcm";

                    if (data.Length != 0)
                    {
                        try
                        {
                            var existingLength = File.Exists(filename) ? new FileInfo(filename).Length : 0;
                            if (existingLength > MaxMicRecordingBytes || data.Length > MaxMicRecordingBytes - existingLength)
                            {
                                Console.WriteLine("Microphone recording quota reached.");
                                break;
                            }
                            if (new DriveInfo(Path.GetPathRoot(Path.GetFullPath(filename))!).AvailableFreeSpace < data.Length)
                            {
                                Console.WriteLine("Insufficient disk space for microphone data.");
                                break;
                            }
                            using var file = File.Open(filename, FileMode.Append, FileAccess.Write, FileShare.Read);
                            file.Write(data, 0, data.Length);
                        }
                        catch (IOException ex)
                        {
                            Console.WriteLine($"Could not write microphone data: {ex.Message}");
                        }
                    }
                    break;
                default:
                    throw new InvalidDataException("unknown command");
            }
        }

    }
}
