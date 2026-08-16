# Microphone Indicator for Windows

[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](https://unlicense.org/)
[![Platform](https://img.shields.io/badge/platform-Windows-0078d7.svg)](#)
[![Latest Release](https://img.shields.io/badge/release-v1.1.0-blue.svg)](https://github.com/walter98garciarespaldo-debug/microphone-indicator-windows/releases)
[![Language: C++](https://img.shields.io/badge/language-C%2B%2B17-00599c.svg)](https://isocpp.org/)

A lightweight, low-level system utility written in native C++ (Win32 & WRL COM) to globally toggle active capture endpoints (microphones) using a keyboard shortcut.

It registers a global hotkey (`Ctrl + Alt + Space`) to mute or unmute all active recording devices. To provide instant visual confirmation without the latency or display queues of standard Windows notifications, it renders a custom, screen-centered hardware-accelerated HUD overlay.

## License & Freedom

This software is released into the public domain under the **Unlicense**. 

You are free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means. 

No attribution is required, no licensing restrictions apply, and no warranties are given. Shape it, fork it, strip it, or monetize it. It is entirely yours.

## Features

- **Global Hotkey Binding**: Binds to `Ctrl + Alt + Space` using the Windows `RegisterHotKey` API.
- **Multi-Endpoint Control**: Enumerates all active audio capture devices via COM/WASAPI (`IMMDeviceEnumerator` and `IAudioEndpointVolume`) using `Microsoft::WRL::ComPtr` (RAII) and toggles their mute state simultaneously.
- **High-Performance HUD**: Renders a custom layered window (`WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE`) at the center of the primary display. Utilizes double-buffered GDI drawing for flicker-free rendering.
- **Smooth Animation**: Driven by a high-resolution multimedia timer to animate alpha transparency (fade-in, hold, fade-out) over 300 milliseconds. Resets instantly if triggered repeatedly.
- **System Tray Integration**: Displays a clean status icon in the system tray with a context menu to toggle mute status, configure autostart, or exit.
- **Windows Autostart Integration**: Option to automatically start the utility on user login, writing directly to the `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` registry hive (no elevation/UAC prompt required during runtime).
- **Elevated Installer**: An NSIS-based script packages the application into an installer that targets `%PROGRAMFILES64%\MicrophoneIndicator`, registers shortcuts, and handles clean uninstallation.

## Directory Structure

```text
microphone-indicator-windows/
├── installer/
│   └── installer.nsi          # NSIS setup script and uninstaller configuration
├── resources/
│   ├── icon.ico               # Main application icon
│   ├── mute.ico               # Muted microphone icon
│   ├── on.ico                 # Active microphone icon
│   └── resources.rc           # Windows Resource script for icons & version metadata
├── scripts/
│   └── publish_release.py     # GitHub release creation and asset uploader
├── src/
│   └── main.cpp               # Core application logic in C++17 (Win32, COM WRL, GDI HUD)
├── vendored/
│   └── CMakeLists.txt         # Optional multi-compiler CMake configuration
├── .env.example               # Example environment configuration for release automation
├── .gitattributes             # GitHub Linguist language statistics overrides
├── .gitignore                 # Git ignore rules for MSVC, CMake and release binaries
├── build.ps1                  # 1-click build script (MSVC release build + NSIS installer)
└── README.md                  # Project documentation
```

## Build Requirements

1. **C++ Toolchain**: Microsoft Visual Studio / Build Tools (`cl.exe`, `rc.exe`) or MinGW GCC (`g++`, `windres`) supporting C++17.
2. **NSIS Compiler** (Optional): Required to package the installer. The build script expects `makensis.exe` in the `.nsis/nsis-3.10/` folder.

## Build Instructions

Compile the release binary and build the installer by running the build script:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

The output files will be placed in the `releases/` directory:
- `microphone-indicator-windows.exe` (Standalone portable executable, ~175 KB)
- `microphone-indicator-setup.exe` (Elevated installer, ~171 KB)
