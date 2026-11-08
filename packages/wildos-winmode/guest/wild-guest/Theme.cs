using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using Microsoft.Win32;

namespace WildOS.Guest
{
    // Light or dark and the accent colour, from Wild OS (brief 5.4 step 8).
    // Windows draws the title bars of RemoteApp windows itself, so these
    // decide whether they match the rest of the desktop. The values are the
    // ones Settings, Personalisation, Colours writes.
    static class Theme
    {
        public const string StateFile = @"C:\ProgramData\WildOS\theme.json";

        public static void Save(Dictionary<string, object> theme)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(StateFile));
            File.WriteAllText(StateFile, Json.Write(theme));
        }

        public static Dictionary<string, object> Load()
        {
            try { return File.Exists(StateFile) ? Json.ParseObject(File.ReadAllText(StateFile)) : null; }
            catch (Exception) { return null; }
        }

        // Runs in the person's session: HKCU is theirs, and the broadcast
        // reaches their windows.
        public static void Apply(Dictionary<string, object> theme)
        {
            if (theme == null) return;
            bool dark = Json.Bool(theme, "dark", false);
            using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"))
            {
                k.SetValue("AppsUseLightTheme", dark ? 0 : 1, RegistryValueKind.DWord);
                k.SetValue("SystemUsesLightTheme", dark ? 0 : 1, RegistryValueKind.DWord);
            }
            int r, g, b;
            if (ParseColour(Json.Str(theme, "accent"), out r, out g, out b))
            {
                // Explorer and DWM keep the colour as 0xAABBGGRR, the
                // colorization value as 0xAARRGGBB.
                int abgr = unchecked((int)(0xFF000000u | ((uint)b << 16) | ((uint)g << 8) | (uint)r));
                int argb = unchecked((int)(0xC4000000u | ((uint)r << 16) | ((uint)g << 8) | (uint)b));
                using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Explorer\Accent"))
                {
                    k.SetValue("AccentColorMenu", abgr, RegistryValueKind.DWord);
                    k.SetValue("StartColorMenu", abgr, RegistryValueKind.DWord);
                }
                using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\DWM"))
                {
                    k.SetValue("AccentColor", abgr, RegistryValueKind.DWord);
                    k.SetValue("ColorizationColor", argb, RegistryValueKind.DWord);
                    k.SetValue("ColorizationAfterglow", argb, RegistryValueKind.DWord);
                }
            }
            // Office keeps a theme of its own; 6 is "Use system setting".
            using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Office\16.0\Common"))
            {
                k.SetValue("UI Theme", 6, RegistryValueKind.DWord);
            }
            UIntPtr result;
            Native.SendMessageTimeout(Native.HWND_BROADCAST, Native.WM_SETTINGCHANGE, UIntPtr.Zero, "ImmersiveColorSet",
                Native.SMTO_ABORTIFHUNG, 1000, out result);
        }

        public static bool ParseColour(string text, out int r, out int g, out int b)
        {
            r = g = b = 0;
            if (string.IsNullOrEmpty(text)) return false;
            string hex = text.TrimStart(new[] { '#' });
            int value;
            if (hex.Length != 6 || !int.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value)) return false;
            r = (value >> 16) & 0xFF;
            g = (value >> 8) & 0xFF;
            b = value & 0xFF;
            return true;
        }
    }
}
