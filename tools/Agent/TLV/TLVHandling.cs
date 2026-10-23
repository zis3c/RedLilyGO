namespace Agent.TLV
{
    internal class TLVHandling
    {
        private const int MaxTlvPayloadBytes = 1024 * 1024;

        public static async Task<int> WriteTLVToStream(byte tag, byte[] data, int offset, int count, Stream output, CancellationToken token = default)
        {
            if (data == null) throw new ArgumentNullException(nameof(data));
            if (output == null) throw new ArgumentNullException(nameof(output));
            if (offset < 0 || count < 0 || count > MaxTlvPayloadBytes || offset > data.Length - count)
                throw new ArgumentOutOfRangeException(nameof(count), "Invalid TLV payload range.");

            var bufferToSend = new byte[count + 4 + 1];
            bufferToSend[0] = tag;
            var lenBytes = BitConverter.GetBytes(count);
            Buffer.BlockCopy(lenBytes, 0, bufferToSend, 1, lenBytes.Length);
            Buffer.BlockCopy(data, offset, bufferToSend, 1 + lenBytes.Length, count);

            await output.WriteAsync(bufferToSend, 0, bufferToSend.Length, token);
            return bufferToSend.Length;
        }
    }
}
