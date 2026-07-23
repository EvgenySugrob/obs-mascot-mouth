# Building on Windows

These instructions target OBS Studio 32.1.2 on Windows x64.

## Requirements

- Visual Studio 2022 with Desktop development with C++;
- MSVC v143 x64/x86 build tools;
- Windows 10 or Windows 11 SDK;
- C++ ATL;
- Git for Windows;
- CMake 3.30.5.

Use the Visual Studio Installer to add missing Visual Studio components.

The Visual Studio Code extensions shown in the screenshot do not install the
MSVC compiler. Visual Studio or Visual Studio Build Tools must provide
`cl.exe`.

## Why CMake is required

CMake reads `CMakeLists.txt`, configures the OBS dependencies and generates the
Visual Studio build files. MSVC then compiles the source code into the plugin
DLL.

## Install Git and CMake

GitHub Desktop contains its own Git, but the `git` command may not be available
in PowerShell. Install Git for Windows if `git --version` fails:

```powershell
winget install --id Git.Git -e
```

Open the official CMake 3.30.5 release:

https://github.com/Kitware/CMake/releases/tag/v3.30.5

Download `cmake-3.30.5-windows-x86_64.msi`. During installation, enable the
option that adds CMake to `PATH`. Close and reopen the terminal afterward.

Verify the tools from Developer PowerShell for Visual Studio 2022:

```powershell
git --version
cmake --version
cl
```

## Get OBS Studio

```powershell
New-Item -ItemType Directory -Force C:\dev
Set-Location C:\dev

git clone --recursive --branch 32.1.2 --depth 1 `
  https://github.com/obsproject/obs-studio.git
```

## Add the plugin

Copy this repository to:

```text
C:\dev\obs-studio\plugins\obs-mascot-mouth
```

The resulting path must contain `CMakeLists.txt`, `src`, and `data` directly.

Open `C:\dev\obs-studio\plugins\CMakeLists.txt` and add:

```cmake
add_obs_plugin(obs-mascot-mouth PLATFORMS WINDOWS)
```

Place the line with the other `obs-*` plugins.

## Configure and build

Run from Developer PowerShell for Visual Studio 2022:

```powershell
Set-Location C:\dev\obs-studio

cmake --preset windows-x64

cmake --build --preset windows-x64 `
  --config Release `
  --target obs-mascot-mouth
```

Find the compiled module:

```powershell
$dll = Get-ChildItem C:\dev\obs-studio\build_x64 `
  -Recurse `
  -Filter obs-mascot-mouth.dll |
  Where-Object FullName -Match "Release" |
  Select-Object -First 1

$dll.FullName
```

## Install

Close OBS Studio before copying the files.

```powershell
$plugin = "$env:APPDATA\obs-studio\plugins\obs-mascot-mouth"

New-Item -ItemType Directory -Force "$plugin\bin\64bit"
New-Item -ItemType Directory -Force "$plugin\data\locale"

Copy-Item $dll.FullName "$plugin\bin\64bit\obs-mascot-mouth.dll"

Copy-Item `
  "C:\dev\obs-studio\plugins\obs-mascot-mouth\data\locale\*" `
  "$plugin\data\locale\"
```

Start OBS Studio and add a new `Mascot Mouth` source.

## Rebuild after code changes

```powershell
Set-Location C:\dev\obs-studio

cmake --build --preset windows-x64 `
  --config Release `
  --target obs-mascot-mouth
```

Close OBS Studio and repeat the two `Copy-Item` commands from the installation
section.

## Clean rebuild

Use this only when CMake configuration or dependencies become inconsistent:

```powershell
Set-Location C:\dev\obs-studio
Remove-Item -Recurse -Force build_x64
cmake --preset windows-x64
cmake --build --preset windows-x64 --config Release --target obs-mascot-mouth
```
