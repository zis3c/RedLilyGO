using HPPH;
using RemoteViewing.Vnc;
using ScreenCapture.NET;

namespace Agent.Remote
{
    internal class RemoteFramebufferSource : IVncFramebufferSource, IDisposable
    {
        public bool SupportsResizing => false;

        public int Width { get; set; } = 200;

        public int Height { get; set; } = 400;

        private readonly IScreenCaptureService? screenCaptureService;
        private readonly IEnumerable<GraphicsCard>? graphicsCards;
        private readonly IEnumerable<Display>? displays;
        private readonly IScreenCapture? screenCapture;
        private readonly ICaptureZone? fullscreen;
        private VncFramebuffer? framebuffer = null;
        private readonly object captureLifecycleLock = new();
        private bool disposedValue;
        private DateTime lastUpdateTime = DateTime.MinValue;
        private Action onNewFrame;
        private int firstFrameLogged;
        public bool IsAvailable { get; }

        public RemoteFramebufferSource(Action onNewFrame)
        {
            this.onNewFrame = onNewFrame;
            try
            {
                screenCaptureService = new DX11ScreenCaptureService();
                graphicsCards = screenCaptureService.GetGraphicsCards();
                if (!graphicsCards.Any()) return;
                var graphicsCard = graphicsCards.First();

                displays = screenCaptureService.GetDisplays(graphicsCard);
                if (!displays.Any()) return;
                var display = displays.First();

                screenCapture = screenCaptureService.GetScreenCapture(display);
                fullscreen = screenCapture.RegisterCaptureZone(0, 0, screenCapture.Display.Width,
                    screenCapture.Display.Height, downscaleLevel: 0);
                // Keep the capture zone updated without a separate polling task.
                fullscreen.AutoUpdate = true;
                Width = fullscreen.Width;
                Height = fullscreen.Height;
                IsAvailable = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Screen capture unavailable: {ex.Message}");
                IsAvailable = false;
            }
        }

        public VncFramebuffer Capture()
        {
            lock (captureLifecycleLock)
            {
                if (disposedValue)
                    throw new ObjectDisposedException(nameof(RemoteFramebufferSource));
                if (!IsAvailable || screenCapture == null || fullscreen == null)
                    throw new InvalidOperationException("No capturable display is available.");

                onNewFrame();

            // Refresh the capture zone before copying its latest image. When the
            // desktop is unchanged, the zone retains its previous complete frame.
            screenCapture.CaptureScreen();

            using (fullscreen.Lock())
            {
                IImage image = fullscreen.Image;

                if (Interlocked.Exchange(ref firstFrameLogged, 1) == 0)
                    global::Program.Log($"Captured host desktop frame: {image.Width}x{image.Height}");

                if (this.framebuffer == null || this.framebuffer.Width != image.Width || this.framebuffer.Height != image.Height)
                {
                    this.framebuffer = new VncFramebuffer("RedLilyGO Remote", image.Width, image.Height, new VncPixelFormat());
                }

                RefImage<ColorBGRA> refImage = image.AsRefImage<ColorBGRA>();
                byte[] pixel = new byte[4];

                lock (this.framebuffer.SyncRoot)
                {
                    for (int row = 0; row < image.Height; row++)
                    {
                        for (int col = 0; col < image.Width; col++)
                        { 
                            pixel[0] = refImage[col, row].B;
                            pixel[1] = refImage[col, row].G;
                            pixel[2] = refImage[col, row].R;
                            pixel[3] = refImage[col, row].A;
                            this.framebuffer.SetPixel(col, row, pixel);
                        }
                    }
                }

                lastUpdateTime = DateTime.UtcNow;

                return this.framebuffer;
            }
            }
        }

        public ExtendedDesktopSizeStatus SetDesktopSize(int width, int height)
        {
            return ExtendedDesktopSizeStatus.Prohibited;
        }

        protected virtual void Dispose(bool disposing)
        {
            lock (captureLifecycleLock)
            {
                if (disposedValue) return;
                if (disposing)
                {
                    screenCapture?.Dispose();
                }

                disposedValue = true;
            }
        }

        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        internal void Reset()
        {
            lastUpdateTime = DateTime.UtcNow;
        }
    }
}
