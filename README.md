# vibrant-visuals-patcher

`vibrant-visuals-patcher` is a native Minecraft Bedrock for Windows mod that enables Vibrant Visuals on DX12-capable devices that Minecraft blocks through its renderer, feature-level, vendor, device, or driver gates.

The main goal is support for **DirectX 12 Feature Level 11.0+** devices. Minecraft's normal Vibrant Visuals path can ask for **D3D12 FL12.1+**, which blocks older GPUs and iGPUs even when they can still run the deferred renderer.

## Features

- Enables the D3D12 renderer path early during process startup
- Forces the Vibrant Visuals capability gate
- Supports D3D12 FL11.0+ through downlevel device retrying
- Spoofs D3D12 feature-level queries for downlevel-created devices
- Keeps success output short, but prints detailed adapter diagnostics when the D3D12 check fails

## Installation

1. Install [QYCottage/ModLoader](https://github.com/QYCottage/ModLoader).
2. Download `vibrant-visuals-patcher.dll`.
3. Put the DLL in your Minecraft mods folder.

For the Xbox/GDK install, the folder is usually:

```text
C:\XboxGames\Minecraft for Windows\Content\mods
```

Start Minecraft normally after copying the DLL.

## Expected Output

Successful startup should look similar to:

```text
[INFO] vibrant-visuals-patcher v1.0.0
[INFO] Author: th4llium
[INFO] D3D12 vendor gate patched early.
[INFO] D3D12 sampler-flags patched early.
[INFO] D3D12 downlevel retry hook installed.
[INFO] Vibrant Visuals capability hook installed.
[INFO] Startup complete.
```

If FL12.x device creation fails and the FL11 path is used, this warning is expected:

```text
[WARN] D3D12 FL12.x device creation failed; retrying FL11.1/FL11.0.
```

If the feature-level spoof path is queried, this warning is expected:

```text
[WARN] D3D12 feature-level query spoofed to FL12.0.
```

Logs are also written to:

```text
%TEMP%\vibrant-visuals-patcher.log
```

## Options

Disable FL11 fallback:

```powershell
$env:VVP_ALLOW_D3D12_FL11 = "0"
```

Disable D3D12 downlevel retrying:

```powershell
$env:VVP_DISABLE_D3D12_DOWNLEVEL_RETRY = "1"
```

Enable slow compatibility scans for moved Minecraft builds:

```powershell
$env:VVP_SLOW_COMPAT_SCAN = "1"
```

## Building

Requirements:

- Windows
- Visual Studio 2022 Build Tools with MSVC
- CMake 3.24 or newer

Build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Output:

```text
build\Release\vibrant-visuals-patcher.dll
```

## Notes

This mod cannot add hardware features that a GPU does not physically support. The FL11.0+ path works by bypassing Minecraft's allowlist and reported-feature-level gates so compatible downlevel DX12 devices can try the renderer.

## License

MIT License. See [LICENSE](LICENSE).
