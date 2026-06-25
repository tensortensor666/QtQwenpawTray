# QwenPawTray

QwenPawTray is a Windows system tray controller for `qwenpaw`, implemented with Qt/C++.

## Features

- Keep a tray icon running in the Windows notification area
- Left click the tray icon to open `http://127.0.0.1:8088/chat`
- Start, stop, and restart `qwenpaw app` from the tray menu
- Wait for service readiness and show tray notifications
- Configure HTTP/HTTPS proxy environment variables for the service and updater
- Optional startup registration in `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- Write tray and service logs under `%LOCALAPPDATA%\QwenPawTray\logs`
- Remove logs older than 7 days
- Run one tray instance per Windows session
- Update QwenPaw through `python -m pip install --upgrade qwenpaw`

## Requirements

- Windows x64
- Qt 5 or Qt 6 with `Widgets` and `Network`
- CMake 3.16+
- MSVC/Visual Studio Build Tools
- Python and `qwenpaw` available from `PATH`
- Inno Setup 6, only when building the installer

Verify the runtime dependency first:

```powershell
qwenpaw --version
qwenpaw app
```

## Build

Configure and build with CMake:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

The executable is generated at:

```text
build\Release\QwenPawTray.exe
```

You can also use the helper script:

```powershell
$env:QT_DIR = "C:\Qt\6.8.3\msvc2022_64"
.\scripts\build_release.bat
```

Optional environment variables:

- `QT_DIR`: Qt MSVC kit directory
- `CMAKE_EXE`: CMake executable, defaults to `cmake`
- `VSDEVCMD`: path to `VsDevCmd.bat`

## Installer

Build the installer with:

```powershell
$env:QT_DIR = "C:\Qt\6.8.3\msvc2022_64"
.\scripts\build_installer.bat
```

The installer is generated at:

```text
dist\QwenPawTray-Setup.exe
```

If Inno Setup is not installed in the default location, set:

```powershell
$env:ISCC_EXE = "C:\Path\To\ISCC.exe"
```

## Runtime Files

QwenPawTray creates these files on first run:

```text
%LOCALAPPDATA%\QwenPawTray\tray_config.json
%LOCALAPPDATA%\QwenPawTray\logs\
```

Proxy changes are saved immediately. If the service is already running, restart it from the tray menu to apply the new environment variables.

## GitHub

Generated binaries and deployment folders are ignored by `.gitignore`.

For a new repository:

```powershell
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/<user>/<repo>.git
git push -u origin main
```
