using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace VibrantVisualPatcherInstaller.Helpers
{
    internal static class DwmHelper
    {
        [DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int dwAttribute, ref int pvAttribute, int cbAttribute);

        internal static void ApplyWindowStyle(Window window, bool isDark)
        {
            var hwnd = new WindowInteropHelper(window).Handle;
            if (hwnd == IntPtr.Zero)
                return;

            int darkMode = isDark ? 1 : 0;
            DwmSetWindowAttribute(hwnd, 20, ref darkMode, sizeof(int));

            int cornerPref = 2;
            DwmSetWindowAttribute(hwnd, 33, ref cornerPref, sizeof(int));
        }
    }
}
