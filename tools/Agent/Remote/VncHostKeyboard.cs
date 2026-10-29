using System.Runtime.InteropServices;
using RemoteViewing.Vnc;

namespace Agent.Remote;

internal sealed class VncHostKeyboard : IDisposable
{
    private const uint KeyUp = 0x0002;
    private const uint ExtendedKey = 0x0001;
    private const byte Shift = 1;
    private const byte Control = 2;
    private const byte Alt = 4;
    private readonly object sync = new();
    private readonly Dictionary<uint, HeldKey> pressed = new();

    public void HandleKeyChanged(object? sender, KeyChangedEventArgs e)
    {
        lock (sync)
        {
            var symbol = (uint)e.Keysym;
            if (e.Pressed)
            {
                if (!TryMap(symbol, out var key, out var extended, out var modifiers)) return;
                if (pressed.ContainsKey(symbol))
                {
                    SendKey(key, extended, true);
                    return;
                }

                byte synthesized = 0;
                foreach (var modifier in new[] { Shift, Control, Alt })
                {
                    if ((modifiers & modifier) == 0 || IsModifierDown(modifier)) continue;
                    SendKey(ModifierKey(modifier), false, true);
                    synthesized |= modifier;
                }
                SendKey(key, extended, true);
                pressed[symbol] = new HeldKey(key, extended, synthesized);
            }
            else if (pressed.Remove(symbol, out var held))
            {
                SendKey(held.Key, held.Extended, false);
                foreach (var modifier in new[] { Alt, Control, Shift })
                {
                    if ((held.SynthesizedModifiers & modifier) != 0)
                        SendKey(ModifierKey(modifier), false, false);
                }
            }
        }
    }

    public void Dispose()
    {
        lock (sync)
        {
            foreach (var held in pressed.Values)
            {
                SendKey(held.Key, held.Extended, false);
                foreach (var modifier in new[] { Alt, Control, Shift })
                {
                    if ((held.SynthesizedModifiers & modifier) != 0)
                        SendKey(ModifierKey(modifier), false, false);
                }
            }
            pressed.Clear();
        }
    }

    private bool IsModifierDown(byte modifier)
    {
        var left = ModifierKey(modifier);
        var right = modifier switch { Shift => 0xA1, Control => 0xA3, _ => 0xA5 };
        return pressed.Values.Any(key => key.Key == left || key.Key == right);
    }

    private static byte ModifierKey(byte modifier) => modifier switch
    {
        Shift => 0xA0,
        Control => 0xA2,
        _ => 0xA4
    };

    private static bool TryMap(uint symbol, out byte key, out bool extended, out byte modifiers)
    {
        extended = false;
        modifiers = 0;
        switch (symbol)
        {
            case 0xFF08: key = 0x08; return true;
            case 0xFF09: key = 0x09; return true;
            case 0xFF0D: key = 0x0D; return true;
            case 0xFF13: key = 0x13; return true;
            case 0xFF14: key = 0x91; return true;
            case 0xFF1B: key = 0x1B; return true;
            case 0xFF50: key = 0x24; extended = true; return true;
            case 0xFF51: key = 0x25; extended = true; return true;
            case 0xFF52: key = 0x26; extended = true; return true;
            case 0xFF53: key = 0x27; extended = true; return true;
            case 0xFF54: key = 0x28; extended = true; return true;
            case 0xFF55: key = 0x21; extended = true; return true;
            case 0xFF56: key = 0x22; extended = true; return true;
            case 0xFF57: key = 0x23; extended = true; return true;
            case 0xFF61: key = 0x2C; extended = true; return true;
            case 0xFF63: key = 0x2D; extended = true; return true;
            case 0xFF7F: key = 0x90; extended = true; return true;
            case 0xFFE1: key = 0xA0; return true;
            case 0xFFE2: key = 0xA1; return true;
            case 0xFFE3: key = 0xA2; return true;
            case 0xFFE4: key = 0xA3; return true;
            case 0xFFE5: key = 0x14; return true;
            case 0xFFE7 or 0xFFEB: key = 0x5B; extended = true; return true;
            case 0xFFE8 or 0xFFEC: key = 0x5C; extended = true; return true;
            case 0xFFE9: key = 0xA4; extended = true; return true;
            case 0xFFEA: key = 0xA5; extended = true; return true;
            case 0xFFFF: key = 0x2E; extended = true; return true;
            default:
                if (symbol is >= 0xFFBE and <= 0xFFD5)
                {
                    key = (byte)(0x70 + symbol - 0xFFBE);
                    return true;
                }
                if (symbol is < 0x20 or > 0x7E)
                {
                    key = 0;
                    return false;
                }
                var mapped = VkKeyScanW((char)symbol);
                if (mapped == -1)
                {
                    key = 0;
                    return false;
                }
                var bits = (ushort)mapped;
                key = (byte)(bits & 0xFF);
                modifiers = (byte)((bits >> 8) & 7);
                extended = key is 0x21 or 0x22 or 0x23 or 0x24 or 0x25 or 0x26 or 0x27 or 0x28 or 0x2D or 0x2E;
                return true;
        }
    }

    private static void SendKey(byte key, bool extended, bool down) =>
        keybd_event(key, 0, (down ? 0u : KeyUp) | (extended ? ExtendedKey : 0u), UIntPtr.Zero);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "VkKeyScanW")]
    private static extern short VkKeyScanW(char character);

    [DllImport("user32.dll", EntryPoint = "keybd_event")]
    private static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);

    private readonly record struct HeldKey(byte Key, bool Extended, byte SynthesizedModifiers);
}
