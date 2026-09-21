using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Shapes;
using VibrantVisualPatcherInstaller.Helpers;
using IOPath = System.IO.Path;

namespace VibrantVisualPatcherInstaller
{
    public partial class MainWindow : Window
    {
        private readonly bool _isDark;
        private readonly CancellationTokenSource _cts = new CancellationTokenSource();
        private bool _isInstalled = false;
        private string _targetPath;

        public MainWindow()
        {
            _isDark = ThemeHelper.IsDarkMode();
            InitializeComponent();
        }

        private void Window_SourceInitialized(object sender, EventArgs e)
        {
            DwmHelper.ApplyWindowStyle(this, _isDark);
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            if (!_isDark)
                ApplyLightTheme();

            FadeIn();
            InitTargetPath();
        }

        private void InitTargetPath()
        {
            _targetPath = MinecraftLocator.Locate();
            if (!string.IsNullOrEmpty(_targetPath))
            {
                PathText.Text = _targetPath;
                PathText.ToolTip = _targetPath;
                StatusText.Text = "Ready to install.";
                InstallBtn.IsEnabled = true;
            }
            else
            {
                PathText.Text = "Minecraft Bedrock not found";
                PathText.ToolTip = "Click Browse to locate Minecraft folder";
                StatusText.Text = "Please locate your Minecraft folder.";
                InstallBtn.IsEnabled = false;
            }
        }

        private void BrowseBtn_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new Microsoft.Win32.OpenFileDialog
            {
                Title = "Select Minecraft.Windows.exe",
                Filter = "Minecraft Executable (Minecraft.Windows.exe)|Minecraft.Windows.exe|All Executables (*.exe)|*.exe|All files (*.*)|*.*",
                FileName = "Minecraft.Windows.exe"
            };

            if (dlg.ShowDialog() == true)
            {
                var selectedDir = IOPath.GetDirectoryName(dlg.FileName);
                if (!string.IsNullOrEmpty(selectedDir) && Directory.Exists(selectedDir))
                {
                    _targetPath = selectedDir;
                    PathText.Text = _targetPath;
                    PathText.ToolTip = _targetPath;
                    InstallBtn.IsEnabled = true;
                    StatusText.Text = "Ready to install.";
                }
            }
        }

        private async void InstallBtn_Click(object sender, RoutedEventArgs e)
        {
            if (_isInstalled)
            {
                _cts.Cancel();
                Close();
                return;
            }

            if (Process.GetProcessesByName("Minecraft.Windows").Length > 0)
            {
                StatusText.Text = "Please close Minecraft before installing.";
                return;
            }

            if (string.IsNullOrEmpty(_targetPath) || !Directory.Exists(_targetPath))
            {
                StatusText.Text = "Please select a valid Minecraft directory first.";
                return;
            }

            InstallBtn.IsEnabled = false;
            BrowseBtn.IsEnabled = false;
            InstallBtn.Content = "Installing...";
            await RunInstall();
        }

        private void ApplyLightTheme()
        {
            var r = Application.Current.Resources;
            r["OpaqueWindowBg"] = Brush("#FFF9F9F9");
            r["PanelBorder"]    = Brush("#FFE0E0E0");
            r["PrimaryText"]    = Brush("#FF141414");
            r["SecondaryText"]  = Brush("#FF6E6E6E");
            r["TrackBrush"]     = Brush("#FFE2E2E2");
            r["AccentBrush"]    = Brush("#FF141414");
            r["SeparatorBrush"] = Brush("#FFE8E8E8");
            r["HoverBrush"]     = Brush("#FFEFEFEF");
        }

        private static SolidColorBrush Brush(string hex)
        {
            return new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex));
        }

        private void FadeIn()
        {
            MainBorder.Opacity = 0;
            var anim = new DoubleAnimation(0, 1, new Duration(TimeSpan.FromMilliseconds(200)))
            {
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
            };
            MainBorder.BeginAnimation(UIElement.OpacityProperty, anim);
        }

        private async Task RunInstall()
        {
            try
            {
                string winhttpDest = IOPath.Combine(_targetPath, "WINHTTP.dll");
                string gameModsDir = IOPath.Combine(_targetPath, "mods");
                string mainPatcherDest = IOPath.Combine(gameModsDir, "vibrant-visuals-patcher.dll");

                await RunStep(
                    stepContainer: Step1Container,
                    ring:          Step1Ring,
                    icon:          Step1Icon,
                    progress:      Step1Progress,
                    statusText:    Step1StatusText,
                    fetchMsg:      "Connecting to GitHub (QYCottage/ModLoader)...",
                    downloadMsg:   "Downloading ModLoader from GitHub...",
                    owner:         "QYCottage",
                    repo:          "ModLoader",
                    assetName:     "WINHTTP.dll",
                    destPath:      winhttpDest);

                await RunStep(
                    stepContainer: Step2Container,
                    ring:          Step2Ring,
                    icon:          Step2Icon,
                    progress:      Step2Progress,
                    statusText:    Step2StatusText,
                    fetchMsg:      "Connecting to GitHub (th4llium/vibrant-visuals-patcher)...",
                    downloadMsg:   "Downloading Vibrant Visuals from GitHub...",
                    owner:         "th4llium",
                    repo:          "vibrant-visuals-patcher",
                    assetName:     "vibrant-visuals-patcher.dll",
                    destPath:      mainPatcherDest);

                SyncAdditionalModDirectories(mainPatcherDest);

                await OnAllDone();
            }
            catch (OperationCanceledException) { }
            catch (Exception ex)
            {
                BrowseBtn.IsEnabled = true;
                InstallBtn.IsEnabled = true;
                InstallBtn.Content = "Retry";
                StatusText.Text = "Installation failed: " + ex.Message;
            }
        }

        private static void SyncAdditionalModDirectories(string sourceDllPath)
        {
            if (!File.Exists(sourceDllPath))
                return;

            try
            {
                string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
                string appDataMods1 = IOPath.Combine(appData, "Minecraft Bedrock", "mods");
                Directory.CreateDirectory(appDataMods1);
                File.Copy(sourceDllPath, IOPath.Combine(appDataMods1, "vibrant-visuals-patcher.dll"), true);

                string appDataPreview = IOPath.Combine(appData, "Minecraft Bedrock Preview");
                if (Directory.Exists(appDataPreview))
                {
                    string appDataMods2 = IOPath.Combine(appDataPreview, "mods");
                    Directory.CreateDirectory(appDataMods2);
                    File.Copy(sourceDllPath, IOPath.Combine(appDataMods2, "vibrant-visuals-patcher.dll"), true);
                }
            }
            catch
            {
            }
        }

        private async Task RunStep(
            Grid stepContainer,
            Ellipse ring,
            TextBlock icon,
            ProgressBar progress,
            TextBlock statusText,
            string fetchMsg,
            string downloadMsg,
            string owner,
            string repo,
            string assetName,
            string destPath)
        {
            ActivateStep(stepContainer, ring, icon);
            StatusText.Text = fetchMsg;
            statusText.Text = "Connecting...";

            try
            {
                var url = await Installer.GetLatestAssetUrl(owner, repo, assetName);
                StatusText.Text = downloadMsg;

                long finalDownloaded = 0;
                var prog = new Progress<(double frac, long downloaded, long total)>(info =>
                {
                    progress.Value = info.frac;
                    if (info.downloaded > 0)
                        finalDownloaded = info.downloaded;

                    if (info.total > 0)
                    {
                        var kbDone = info.downloaded / 1024.0;
                        var kbTotal = info.total / 1024.0;
                        statusText.Text = $"Downloading from GitHub ({kbDone:F1} / {kbTotal:F1} KB)...";
                    }
                    else
                    {
                        statusText.Text = "Downloading from GitHub...";
                    }
                });

                await Installer.DownloadAndInstall(url, destPath, prog, _cts.Token);
                await Task.Delay(250, _cts.Token);

                double displayKb = finalDownloaded > 0 ? finalDownloaded / 1024.0 : (new FileInfo(destPath).Length / 1024.0);
                DoneStep(ring, icon, statusText, $"{displayKb:F1} KB downloaded \u00B7 Installed");
            }
            catch (OperationCanceledException)
            {
                throw;
            }
            catch (Exception ex)
            {
                ErrorStep(ring, icon, statusText, ex.Message);
                StatusText.Text = "Installation failed. Close and try again.";
                InstallBtn.IsEnabled = true;
                BrowseBtn.IsEnabled = true;
                InstallBtn.Content = "Retry";
                throw;
            }
        }

        private void ActivateStep(Grid container, Ellipse ring, TextBlock icon)
        {
            var fadeUp = new DoubleAnimation(0.45, 1.0, new Duration(TimeSpan.FromMilliseconds(200)))
            {
                EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseOut }
            };
            container.BeginAnimation(OpacityProperty, fadeUp);

            var ringPulse = new DoubleAnimation(1.0, 0.4, new Duration(TimeSpan.FromMilliseconds(900)))
            {
                AutoReverse = true,
                RepeatBehavior = RepeatBehavior.Forever,
                EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut }
            };
            ring.BeginAnimation(OpacityProperty, ringPulse);

            ring.Stroke = (Brush)Application.Current.Resources["PrimaryText"];
            icon.Foreground = (Brush)Application.Current.Resources["PrimaryText"];
        }

        private void DoneStep(Ellipse ring, TextBlock icon, TextBlock statusText, string summary)
        {
            ring.BeginAnimation(OpacityProperty, null);
            ring.Opacity = 1.0;

            var successBrush = (Brush)Application.Current.Resources["SuccessBrush"];
            ring.Fill       = successBrush;
            ring.Stroke     = successBrush;
            icon.Text       = "\u2713";
            icon.Foreground = new SolidColorBrush(Colors.White);
            statusText.Text = summary;
        }

        private void ErrorStep(Ellipse ring, TextBlock icon, TextBlock statusText, string message)
        {
            ring.BeginAnimation(OpacityProperty, null);
            ring.Opacity = 1.0;

            var errorBrush = (Brush)Application.Current.Resources["ErrorBrush"];
            ring.Fill       = errorBrush;
            ring.Stroke     = errorBrush;
            icon.Text       = "\u2715";
            icon.Foreground = new SolidColorBrush(Colors.White);
            statusText.Text = $"Failed: {message}";
        }

        private async Task OnAllDone()
        {
            _isInstalled = true;
            InstallBtn.IsEnabled = true;

            for (int i = 3; i >= 1; i--)
            {
                if (_cts.IsCancellationRequested)
                    return;

                StatusText.Text = $"Installation complete. Closing in {i}s...";
                InstallBtn.Content = $"Close ({i}s)";

                try
                {
                    await Task.Delay(1000, _cts.Token);
                }
                catch (OperationCanceledException)
                {
                    return;
                }
            }

            if (!_cts.IsCancellationRequested)
                Close();
        }

        private void Border_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.LeftButton == MouseButtonState.Pressed)
                DragMove();
        }

        private void CloseBtn_Click(object sender, RoutedEventArgs e)
        {
            _cts.Cancel();
            Close();
        }
    }
}
