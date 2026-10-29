using RemoteViewing.Vnc.Server;
using RemoteViewing.Vnc;
using Microsoft.Extensions.Logging;

namespace Agent.Remote
{
    internal class RemoteServer : IVncPasswordChallenge, IDisposable
    {
        private readonly Action OnError;
        private readonly VncServerSession session;
        private readonly RemoteFramebufferSource framebuffer;
        private readonly VncMouse remoteMouse = new();
        private readonly VncHostKeyboard remoteKeyboard = new();
        private bool disposedValue;
        private int errorSignalled;

        public RemoteServer(Action onerr)
        {
            this.OnError = onerr;

            // Keep the host framebuffer at native resolution for phone viewers.
            framebuffer = new RemoteFramebufferSource(OnFrameUpdate);

            var loggerFactory = new LoggerFactory();
            var log = loggerFactory.CreateLogger("remote");

            session = new VncServerSession(this, log);
            session.SetFramebufferSource(framebuffer);
            session.PointerChanged += remoteMouse.OnMouseUpdate;
            session.KeyChanged += remoteKeyboard.HandleKeyChanged;
#pragma warning disable CS8622 // Nullability of reference types in type of parameter doesn't match the target delegate (possibly because of nullability attributes).
            session.ConnectionFailed += HandleConnectionFailed;
            session.Closed += HandleClosed;
#pragma warning restore CS8622 // Nullability of reference types in type of parameter doesn't match the target delegate (possibly because of nullability attributes).

            session.Encoder = new TightEncoder(session)
            {
                Compression = TightCompression.Jpeg,
            };
        }

        private void OnFrameUpdate()
        {
            // Use the server's default pixel format for every update. The format
            // negotiated by the browser can otherwise produce black JPEG frames.
            session.ClientPixelFormat = new VncPixelFormat();
            session.MaxUpdateRate = 2;
        }

        public void Start(Stream stream)
        {
            if (disposedValue)
                throw new ObjectDisposedException(nameof(RemoteServer));
            if (stream is not Agent.IAuthenticatedTransport)
                throw new UnauthorizedAccessException("VNC requires the authenticated serial transport.");
            if (!framebuffer.IsAvailable)
            {
                SignalError();
                return;
            }
            Interlocked.Exchange(ref errorSignalled, 0);

            var options = new VncServerSessionOptions
            {
                AuthenticationMethod = AuthenticationMethod.None
            };

            framebuffer.Reset();

            // Start the VNC handshake over the authenticated serial transport.
            session.Connect(stream, options);
        }

        private void HandleConnectionFailed(object sender, EventArgs e)
        {
            SignalError();
        }

        private void HandleClosed(object sender, EventArgs e)
        {
            SignalError();
        }

        private void SignalError()
        {
            if (Interlocked.Exchange(ref errorSignalled, 1) == 0)
                OnError();
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    SignalError();
                    remoteKeyboard.Dispose();
                    framebuffer.Dispose();
                }
                disposedValue = true;
            }
        }

        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        public byte[] GenerateChallenge()
        {
            throw new NotImplementedException();
        }

        public void GetChallengeResponse(byte[] challenge, char[] password, byte[] response)
        {
            throw new NotImplementedException();
        }

        public void GetChallengeResponse(byte[] challenge, byte[] password, byte[] response)
        {
            throw new NotImplementedException();
        }
    }
}
