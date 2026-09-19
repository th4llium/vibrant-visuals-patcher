using Microsoft.Win32;

namespace VibrantVisualPatcherInstaller.Helpers
{
    internal static class ThemeHelper
    {
        internal static bool IsDarkMode()
        {
            using (var key = Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"))
            {
                var val = key?.GetValue("AppsUseLightTheme");
                return !(val is int i && i == 1);
            }
        }
    }
}
