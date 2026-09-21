using System;
using System.IO;
using System.Net.Http;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace VibrantVisualPatcherInstaller.Helpers
{
    internal static class Installer
    {
        private static readonly HttpClient Http = new HttpClient();

        static Installer()
        {
            Http.Timeout = TimeSpan.FromSeconds(60);
            Http.DefaultRequestHeaders.Add("User-Agent", "VibrantVisualPatcherInstaller/1.1.0");
        }

        internal static async Task<string> GetLatestAssetUrl(string owner, string repo, string assetName)
        {
            var directUrl = $"https://github.com/{owner}/{repo}/releases/latest/download/{assetName}";
            try
            {
                using (var req = new HttpRequestMessage(HttpMethod.Head, directUrl))
                {
                    using (var res = await Http.SendAsync(req, HttpCompletionOption.ResponseHeadersRead))
                    {
                        if (res.IsSuccessStatusCode)
                            return directUrl;
                    }
                }
            }
            catch { }

            try
            {
                var apiUrl = $"https://api.github.com/repos/{owner}/{repo}/releases";
                var json = await Http.GetStringAsync(apiUrl);
                var pattern = "\"browser_download_url\"\\s*:\\s*\"([^\"]+" + Regex.Escape(assetName) + ")\"";
                var match = Regex.Match(json, pattern, RegexOptions.IgnoreCase);
                if (match.Success)
                    return match.Groups[1].Value;
            }
            catch { }

            if (repo == "vibrant-visuals-patcher" && assetName == "vibrant-visuals-patcher.dll")
                return $"https://github.com/{owner}/{repo}/releases/download/v1.1.0/{assetName}";

            return directUrl;
        }

        internal static async Task DownloadAndInstall(
            string url,
            string destPath,
            IProgress<(double frac, long downloaded, long total)> progress,
            CancellationToken ct)
        {
            var dir = Path.GetDirectoryName(destPath);
            if (!string.IsNullOrEmpty(dir))
                Directory.CreateDirectory(dir);

            using (var response = await Http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, ct))
            {
                response.EnsureSuccessStatusCode();
                var total = response.Content.Headers.ContentLength ?? -1L;

                using (var stream = await response.Content.ReadAsStreamAsync())
                using (var file = new FileStream(destPath, FileMode.Create, FileAccess.Write, FileShare.None, 8192, true))
                {
                    var buffer = new byte[8192];
                    long downloaded = 0;
                    int read;

                    while ((read = await stream.ReadAsync(buffer, 0, buffer.Length, ct)) > 0)
                    {
                        await file.WriteAsync(buffer, 0, read, ct);
                        downloaded += read;
                        var frac = total > 0 ? (double)downloaded / total : 0.5;
                        progress?.Report((frac, downloaded, total));
                    }
                }
            }

            progress?.Report((1.0, 0, 0));
        }
    }
}
