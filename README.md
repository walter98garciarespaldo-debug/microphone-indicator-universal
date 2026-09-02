# Microphone Indicator (Universal)

[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](https://unlicense.org/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-0078d7.svg)](#)
[![Latest Release](https://img.shields.io/badge/release-v1.2.0-blue.svg)](https://github.com/walter98garciarespaldo-debug/microphone-indicator-universal/releases)
[![Language: C++](https://img.shields.io/badge/language-C%2B%2B17-00599c.svg)](https://isocpp.org/)

A lightweight, low-level system utility written in native C++ to globally toggle active capture endpoints (microphones) using a keyboard shortcut with instant visual HUD feedback across **Windows** and **Linux**.

It registers a global hotkey (`Ctrl + Alt + Space`) to mute or unmute active recording devices. To provide instant visual confirmation without the latency of standard OS notifications, it renders a custom hardware-accelerated translucent HUD overlay.

---

## License & Freedom

This software is released into the public domain under the **Unlicense**. 

You are free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means. 

No attribution is required, no licensing restrictions apply, and no warranties are given. Shape it, fork it, strip it, or monetize it. It is entirely yours.

---

## Features

- **Cross-Platform Native C++ Core**:
  - **Windows**: Pure Win32 API, COM/WASAPI endpoint enumeration, double-buffered GDI HUD.
  - **Linux**: Qt6 + PulseAudio/PipeWire event subscription + direct Linux Kernel `evdev` hotkey listener.
- **Universal Global Hotkey Binding**: Binds to `Ctrl + Alt + Space`. On Linux, it operates at the kernel event layer, seamlessly supporting both **Wayland** (KDE, GNOME) and **X11**.
- **Zero-Polling Audio Monitoring**: Subscribes directly to audio subsystem events (`WASAPI` / `PulseAudio`) to reflect real-time mute changes from external apps or hardware buttons instantly.
- **High-Performance Floating HUD**: Translucent, rounded on-screen display with smooth presentation for instant feedback.
- **System Tray Integration**: Crisp status icon in the system tray / status notifier area with context menu controls.
- **System Autostart**: Easy one-click autostart configuration for both Windows (Registry) and Linux (`~/.config/autostart`).

---

## Directory Structure

```text
microphone-indicator/
├── installer/
│   ├── installer.nsi          # NSIS Windows setup script and uninstaller configuration
│   └── install-linux.sh       # Linux system installer (/opt + .desktop + symlinks)
├── resources/
│   ├── icon.ico / on.ico / mute.ico    # Windows resource icons
│   ├── mic-on.png / mic-mute.png       # Linux high-res PNG icons
│   └── resources.rc                    # Windows resource compiler definition
├── scripts/
│   └── publish_release.py     # Universal GitHub release & asset uploader
├── src/
│   ├── windows/
│   │   └── main.cpp           # Windows implementation (Win32, COM WASAPI, GDI HUD)
│   └── linux/
│       └── main.cpp           # Linux implementation (Qt6, PulseAudio, Kernel evdev)
├── .env.example               # Release automation template
├── CMakeLists.txt             # Cross-platform CMake configuration
├── build.ps1                  # Windows build script (MSVC release build + NSIS)
├── build.sh                   # Linux build script
└── README.md                  # Project documentation
```

---

## Building & Installation

### Linux
Prerequisites: `g++` (C++17), `qt6-base`, `libpulse`.

1. **Compile**:
   ```bash
   bash build.sh
   ```
2. **Install to system**:
   ```bash
   sudo bash installer/install-linux.sh
   ```

### Windows
Prerequisites: Visual Studio C++ Build Tools (`cl.exe`, `rc.exe`) or MinGW, and NSIS (optional for installer packaging).

1. **Compile**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File build.ps1
   ```

---

## Release Artifacts

The compiled releases are located in the `releases/` directory:
- **Windows Standalone**: `microphone-indicator-windows.exe` (~175 KB)
- **Windows Installer**: `microphone-indicator-setup.exe` (~171 KB)
- **Linux Standalone Binary**: `microphone-indicator-linux` (~70 KB)
- **Linux Tarball**: `microphone-indicator-linux-x86_64.tar.gz`
