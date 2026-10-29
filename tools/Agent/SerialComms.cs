using RJCP.IO.Ports;
using WmiLight;

namespace Agent
{
    public class DeviceNotFoundException : Exception
    {

    }

    internal class SerialComms
    {
        private readonly Executer executer = new();

        private string? GetMatchingSerialPort(string vidToMatch, string pidToMatch)
        {
            vidToMatch = vidToMatch.ToUpperInvariant();
            pidToMatch = pidToMatch.ToUpperInvariant();
            
            var matches = new List<string>();

            using (WmiConnection con = new WmiConnection())
            {
                foreach (WmiObject p in con.CreateQuery("SELECT * FROM WIN32_SerialPort"))
                {
                    var rawDeviceId = p["PNPDeviceID"];
                    if (rawDeviceId == null) continue;
                    var deviceId = rawDeviceId.ToString();
                    if (!string.IsNullOrWhiteSpace(deviceId) && deviceId.StartsWith("USB"))
                    {
                        string VID = string.Empty;
                        string PID = string.Empty;
                        foreach (var item in deviceId.Split(new string[] { "\\", "&" }, StringSplitOptions.RemoveEmptyEntries))
                        {
                            if (item.StartsWith("VID"))
                            {
                                VID = item.Split("_", StringSplitOptions.RemoveEmptyEntries)[1];
                            }
                            if (item.StartsWith("PID"))
                            {
                                PID = item.Split("_", StringSplitOptions.RemoveEmptyEntries)[1];
                            }
                        }

                        if (VID == vidToMatch && PID == pidToMatch)
                        {
                            var portName = p.GetPropertyValue("DeviceID")?.ToString();
                            if (!string.IsNullOrWhiteSpace(portName)) matches.Add(portName);
                        }
                    }
                }
            }

            if (matches.Any())
            {
                // The ESP32-S3 can expose multiple matching COM ports. TinyUSB
                // CDC is usually the highest-numbered port.
                return matches.OrderByDescending(x => int.Parse(x.Replace("COM", ""))).First();
            }

            throw new DeviceNotFoundException();
        }

        public void Start(string vid, string pid, CancellationToken cancellationToken, Action<string>? status = null, int baud = 115200)
        {
            var serialPort = GetMatchingSerialPort(vid, pid);

            if (string.IsNullOrWhiteSpace(serialPort))
            {
                throw new DeviceNotFoundException();
            }

            ReadCommands(baud, serialPort, cancellationToken, status);
        }

        private void ReadCommands(int baud, object serialPortName, CancellationToken cancellationToken, Action<string>? status)
        {
#if DEBUG
            Console.WriteLine($"Opening {serialPortName}");
#endif

            using (var cts = new CancellationTokenSource())
            using (var m_RxPort = new SerialPortStream(serialPortName.ToString())
            {
                BaudRate = baud,
                DataBits = 8,
                Parity = Parity.None,
                StopBits = StopBits.One,
                ReadTimeout = -1,
                WriteTimeout = -1
            })
            {
                m_RxPort.Open();
                status?.Invoke($"Board connected on {serialPortName}");
                using var registration = cancellationToken.Register(() =>
                {
                    try { m_RxPort.Close(); } catch { }
                });

                while (!cts.IsCancellationRequested && !cancellationToken.IsCancellationRequested)
                {
                    try
                    {
                        executer.ParseAndExecute(m_RxPort, cts);
                    }
                    catch (Exception)
                    {
                        cts.Cancel();
                        throw;
                    }
                }
            }

#if DEBUG
            Console.WriteLine("Serial port closed");
#endif
        }
    }
}
