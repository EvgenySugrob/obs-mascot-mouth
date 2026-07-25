# Updating and Packaging on Windows

These instructions assume that OBS Studio 32.1.2 and the plugin build tree are
already configured.

Set the local paths before running the commands:

```powershell
$obsSource = "F:\dev\obs-studio"
$releaseDirectory = "F:\dev\releases"
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
$obsInstall = "D:\Programs\obs-studio"

New-Item -ItemType Directory -Force "$obsInstall\obs-plugins\64bit"
New-Item -ItemType Directory -Force "$obsInstall\data\obs-plugins\obs-mascot-mouth\locale"

Copy-Item `
  "$obsSource\build_x64\plugins\obs-mascot-mouth\Release\obs-mascot-mouth.dll" `
  "$obsInstall\obs-plugins\64bit\obs-mascot-mouth.dll" `
  -Force

Copy-Item `
  "$obsSource\plugins\obs-mascot-mouth\data\locale\*" `
  "$obsInstall\data\obs-plugins\obs-mascot-mouth\locale\" `
  -Force
```

For a custom or portable OBS installation, copy the DLL and locale files to:

```text
<OBS installation>\obs-plugins\64bit\obs-mascot-mouth.dll
<OBS installation>\data\obs-plugins\obs-mascot-mouth\locale
```

## Package

```powershell
$stage = "$releaseDirectory\obs-mascot-mouth-0.4.0"

New-Item -ItemType Directory -Force "$stage\obs-plugins\64bit"
New-Item -ItemType Directory -Force "$stage\data\obs-plugins\obs-mascot-mouth\locale"

Copy-Item `
  "$obsSource\build_x64\plugins\obs-mascot-mouth\Release\obs-mascot-mouth.dll" `
  "$stage\obs-plugins\64bit\obs-mascot-mouth.dll" `
  -Force

Copy-Item `
  "$obsSource\plugins\obs-mascot-mouth\data\locale\*" `
  "$stage\data\obs-plugins\obs-mascot-mouth\locale\" `
  -Force

Compress-Archive `
  -Path "$stage\*" `
  -DestinationPath "$releaseDirectory\obs-mascot-mouth-0.4.0-windows-x64.zip" `
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
Maximum speaking bounce: 4 px
Maximum speaking stretch: 1.5%
Maximum speaking tilt: 1.5°
Idle breathing amount: 0.6%
Idle breathing speed: 0.25
```
