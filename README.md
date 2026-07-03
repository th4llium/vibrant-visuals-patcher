# vibrant-visuals-patcher

`vibrant-visuals-patcher` is a native Minecraft Bedrock for Windows mod that enables Vibrant Visuals on DX12-capable devices that Minecraft blocks through renderer, feature-level, vendor, device, or driver checks.

Version 1.0.2 targets **Minecraft for Windows Client 26.31**.

The main goal is support for **DirectX 12 Feature Level 11.0+** devices. Minecraft's normal Vibrant Visuals path can request **D3D12 FL12.1+**, which blocks older GPUs and iGPUs even when the deferred renderer can still run.

## Features

- Enables the D3D12 renderer path early during process startup
- Forces the Vibrant Visuals capability gate
- Supports D3D12 FL11.0+ through downlevel device retrying
- Spoofs D3D12 feature-level queries for downlevel-created devices
- Supports loading alongside BetterRenderDragon

## Installation

1. Install [QYCottage/ModLoader](https://github.com/QYCottage/ModLoader).
2. Download `vibrant-visuals-patcher.dll`.
3. Put the DLL in your Minecraft mods folder.

For the Xbox/GDK install, the folder is usually:

```text
C:\XboxGames\Minecraft for Windows\Content\mods
```

Start Minecraft normally after copying the DLL.

## BetterRenderDragon

BetterRenderDragon is not required, but this mod is designed to work with it.

When BetterRenderDragon is loaded, `vibrant-visuals-patcher` avoids taking over the shared Vibrant Visuals predicate before BRD scans for it. BRD installs its hook first, then this mod attaches to that hook path so Vibrant Visuals can still be forced without disabling BRD's shader reload, material redirection, or frame hooks.

## Expected Output

Successful startup should look similar to:

```text
[INFO] vibrant-visuals-patcher v1.0.2
[INFO] Author: th4llium
[INFO] D3D12 vendor gate patched early.
[INFO] D3D12 sampler-flags patched early.
[INFO] D3D12 downlevel retry hook installed.
[INFO] Vibrant Visuals capability hook installed.
[INFO] Startup complete.
```

With BetterRenderDragon loaded, successful startup should also include:

```text
[INFO] BetterRenderDragon detected; using BRD-compatible early renderer patch.
Hooked HOOK1
[INFO] BetterRenderDragon Vibrant Visuals hook relay redirected.
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
