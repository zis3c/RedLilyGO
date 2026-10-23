
using System.Reflection;
using Task = System.Threading.Tasks.Task;
using Agent;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Diagnostics;
using static Agent.Executer;
using System;
#if DEBUG
using Agent.LocalTesting;
#endif

class Program
{
    private static readonly string LogDirectory = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "RedLilyGO");
    private static readonly string LogPath = Path.Combine(LogDirectory, "agent.log");

    internal static void Log(string message)
    {
        var line = $"[{DateTimeOffset.Now:O}] {message}";
        Console.WriteLine(line);
        try
        {
            Directory.CreateDirectory(LogDirectory);
            File.AppendAllText(LogPath, line + Environment.NewLine);
        }
        catch
        {
            // Continue running even if the log directory or file is unavailable.
        }
    }

    // Export an ANSI command-line entry point for rundll32 callers.
    // rundll32 win-x64\PortableApp.dll Open32 vid=cafe pid=1001 cwd=%APPDATA%
    [UnmanagedCallersOnly(EntryPoint = "Open32", CallConvs = new Type[] { typeof(CallConvStdcall) })]
    public static void Open32(IntPtr hwnd, IntPtr hinst, IntPtr lpszCmdLine, int nCmdShow)
    {
#pragma warning disable CS0168 // Variable is declared but never used
        try
        {
            var cmdLine = Marshal.PtrToStringAnsi(lpszCmdLine);
            if (!string.IsNullOrWhiteSpace(cmdLine))
            {
                Main(cmdLine.Split(" ")).Wait();
            }
        }
        catch (Exception ex)
        {
#if DEBUG
            File.AppendAllText("C:\\temp\\log.txt", ex.Message);
#endif
        }
#pragma warning restore CS0168 // Variable is declared but never used
    }

    // Export a Unicode command-line entry point for native DLL callers.
    [UnmanagedCallersOnly(EntryPoint = "Open")]
    public static void Open(IntPtr cmdLinePtr)
    {
#pragma warning disable CS0168 // Variable is declared but never used
        try
        {
            var cmdLine = Marshal.PtrToStringUni(cmdLinePtr);
            if (!string.IsNullOrWhiteSpace(cmdLine))
            {
                Main(cmdLine.Split(" ")).Wait();
            }
        }
        catch (Exception ex)
        {
#if DEBUG
            File.AppendAllText("C:\\temp\\log.txt", ex.Message);
#endif
        }
#pragma warning restore CS0168 // Variable is declared but never used
    }

#if DEBUG
    public
#else
    private
#endif
    static async Task Main(string[] args)
    {
        if (args.Any(x => x.Equals("--help", StringComparison.OrdinalIgnoreCase) || x.Equals("-h", StringComparison.OrdinalIgnoreCase)))
        {
            Console.WriteLine("RedLilyGO Agent");
            Console.WriteLine("Usage: RedLilyGO.Agent.exe [vid=CAFE] [pid=403F]");
            Console.WriteLine("Options: --help, --version");
            return;
        }

        if (args.Any(x => x.Equals("--version", StringComparison.OrdinalIgnoreCase) || x.Equals("-v", StringComparison.OrdinalIgnoreCase)))
        {
            Console.WriteLine($"RedLilyGO Agent {AgentInfo.Version} (protocol {AgentInfo.ProtocolVersion})");
            return;
        }

        // Honor an optional working directory supplied by the caller.
        args.Where(x => x.StartsWith("cwd=")).Select(x => x.Remove(0, 4)).ToList().ForEach(x => Environment.CurrentDirectory = Environment.ExpandEnvironmentVariables(x));

        using (Mutex mutex = new Mutex(true, Assembly.GetExecutingAssembly().FullName, out bool createdNew))
        {
            if (!createdNew)
            {
#if DEBUG
                Console.WriteLine("Already running, quitting");
#endif
                return;
            }

#if DEBUG
            if (args.Any(x => x == "tcp"))
            {
                await TcpServer.Start(7000);
            }
            else if (args.Any(x => x == "web"))
            {
                WebServer.Start(7000, Environment.CurrentDirectory);
            }
            else
            {
#endif
                // Use the project USB identity by default, while allowing custom
                // firmware VID and PID values for development boards.
                var vid = GetArgument(args, "vid", "CAFE");
                var pid = GetArgument(args, "pid", "403F");

                using var lifetime = new CancellationTokenSource();
                Console.CancelKeyPress += (_, eventArgs) =>
                {
                    eventArgs.Cancel = true;
                    lifetime.Cancel();
                    Log("Shutdown requested");
                };

                Log($"RedLilyGO Agent starting. USB VID={vid}, PID={pid}");
                SerialComms serial = new();
                while (!lifetime.IsCancellationRequested)
                {
                    try
                    {
                        Log("Waiting for RedLilyGO board...");
                        serial.Start(vid, pid, lifetime.Token, Log);
                        Log("Board connection ended; retrying");
                    }
                    catch (DeviceNotFoundException)
                    {
                        Log("Board offline; retrying in 1 second");
                        await Task.Delay(TimeSpan.FromSeconds(1), lifetime.Token);
                    }
                    catch (OperationCanceledException) when (lifetime.IsCancellationRequested)
                    {
                        break;
                    }
                    catch (Exception ex)
                    {
                        Log($"Connection error: {ex.Message}");
                        await Task.Delay(TimeSpan.FromSeconds(1), lifetime.Token);
                    }
                }
                Log("RedLilyGO Agent stopped");
#if DEBUG
            }
#endif
        }
    }

    private static string GetArgument(string[] args, string name, string defaultValue)
    {
        var prefix = name + "=";
        var value = args.FirstOrDefault(x => x.StartsWith(prefix, StringComparison.OrdinalIgnoreCase));
        return value == null ? defaultValue : value[prefix.Length..].ToUpperInvariant();
    }
}
