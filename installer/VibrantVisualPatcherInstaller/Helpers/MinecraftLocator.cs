using System;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace VibrantVisualPatcherInstaller.Helpers
{
    internal static class MinecraftLocator
    {
        internal static string Locate()
        {
            string path = FindFromRegistry();
            if (IsValidMinecraftDirectory(path))
                return path;

            path = FindFromPowerShell("Microsoft.MinecraftUWP");
            if (IsValidMinecraftDirectory(path))
                return path;

            path = FindFromPowerShell("Microsoft.MinecraftWindowsBeta");
            if (IsValidMinecraftDirectory(path))
                return path;

            path = FindFromDrives();
            if (IsValidMinecraftDirectory(path))
                return path;

            return null;
        }

        private static bool IsValidMinecraftDirectory(string dir)
        {
            if (string.IsNullOrWhiteSpace(dir))
                return false;

            try
            {
                if (!Directory.Exists(dir))
                    return false;

                return File.Exists(Path.Combine(dir, "Minecraft.Windows.exe"));
            }
            catch
            {
                return false;
            }
        }

        private static string FindFromRegistry()
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(@"Software\Classes\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppModel\Repository\Packages");
                if (key == null)
                    return null;

                string[] subkeyNames = key.GetSubKeyNames();

                foreach (string name in subkeyNames)
                {
                    if (name.StartsWith("Microsoft.MinecraftUWP", StringComparison.OrdinalIgnoreCase))
                    {
                        using var sub = key.OpenSubKey(name);
                        var folder = sub?.GetValue("PackageRootFolder") as string;
                        if (IsValidMinecraftDirectory(folder))
                            return folder;
                    }
                }

                foreach (string name in subkeyNames)
                {
                    if (name.StartsWith("Microsoft.MinecraftWindowsBeta", StringComparison.OrdinalIgnoreCase))
                    {
                        using var sub = key.OpenSubKey(name);
                        var folder = sub?.GetValue("PackageRootFolder") as string;
                        if (IsValidMinecraftDirectory(folder))
                            return folder;
                    }
                }
            }
            catch
            {
            }

            return null;
        }

        private static string FindFromPowerShell(string packageName)
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = "powershell.exe",
                    Arguments = $"-NoProfile -NonInteractive -Command \"(Get-AppxPackage -Name '{packageName}').InstallLocation\"",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    CreateNoWindow = true
                };

                using var proc = Process.Start(psi);
                if (proc != null)
                {
                    string output = proc.StandardOutput.ReadToEnd().Trim();
                    proc.WaitForExit(3000);
                    if (IsValidMinecraftDirectory(output))
                        return output;
                }
            }
            catch
            {
            }

            return null;
        }

        private static string FindFromDrives()
        {
            try
            {
                foreach (var drive in DriveInfo.GetDrives())
                {
                    if (!drive.IsReady)
                        continue;

                    string xboxPath = Path.Combine(drive.RootDirectory.FullName, @"XboxGames\Minecraft for Windows\Content");
                    if (IsValidMinecraftDirectory(xboxPath))
                        return xboxPath;

                    string previewPath = Path.Combine(drive.RootDirectory.FullName, @"XboxGames\Minecraft Preview for Windows\Content");
                    if (IsValidMinecraftDirectory(previewPath))
                        return previewPath;
                }
            }
            catch
            {
            }

            return null;
        }
    }
}
