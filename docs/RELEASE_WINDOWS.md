# Updating and Packaging on Windows

These instructions assume that OBS Studio 32.1.2 and the plugin build tree are
already configured.

Set the local paths before running the commands:

```powershell
$obsSource = "C:\dev\obs-studio"
$releaseDirectory = "C:\dev\releases"
```

## Update the repository

Copy the new source files into the standalone repository, preserving its
`.git` directory. Copy the same source files into:

```text
<OBS source>\plugins\obs-mascot-mouth
```

## Rebuild

Run from Developer PowerShell for Visual Studio 2022:

```powershell
Set-Location $obsSource
cmake --preset windows-x64
cmake --build --preset windows-x64 --config Release --target obs-mascot-mouth
```

## Install

Close OBS Studio. For the recommended per-machine layout:

```powershell
$plugin = "$env:ProgramData\obs-studio\plugins\obs-mascot-mouth"

New-Item -ItemType Directory -Force "$plugin\bin\64bit"
New-Item -ItemType Directory -Force "$plugin\data\locale"

Copy-Item `
  "$obsSource\build_x64\plugins\obs-mascot-mouth\Release\obs-mascot-mouth.dll" `
  "$plugin\bin\64bit\obs-mascot-mouth.dll" `
  -Force

Copy-Item `
  "$obsSource\plugins\obs-mascot-mouth\data\locale\*" `
  "$plugin\data\locale\" `
  -Force
```

For a custom or portable OBS installation, copy the DLL and locale files to:

```text
<OBS installation>\obs-plugins\64bit\obs-mascot-mouth.dll
<OBS installation>\data\obs-plugins\obs-mascot-mouth\locale
```

## Package

```powershell
$stage = "$releaseDirectory\obs-mascot-mouth-0.3.0"
$package = "$stage\obs-mascot-mouth"

New-Item -ItemType Directory -Force "$package\bin\64bit"
New-Item -ItemType Directory -Force "$package\data\locale"

Copy-Item `
  "$obsSource\build_x64\plugins\obs-mascot-mouth\Release\obs-mascot-mouth.dll" `
  "$package\bin\64bit\obs-mascot-mouth.dll" `
  -Force

Copy-Item `
  "$obsSource\plugins\obs-mascot-mouth\data\locale\*" `
  "$package\data\locale\" `
  -Force

Compress-Archive `
  -Path "$package" `
  -DestinationPath "$releaseDirectory\obs-mascot-mouth-0.3.0-windows-x64.zip" `
  -Force
```

## Recommended settings

```text
Close threshold: -42 dB
Open threshold: -35 dB
Wide-open threshold: -22 dB
Close delay: 120 ms
Mouth smoothing: 80 ms
Minimum blink interval: 3500 ms
Maximum blink interval: 6500 ms
Blink duration: 120 ms
Maximum vertical movement: 4 px
Maximum scale increase: 1.5%
```
