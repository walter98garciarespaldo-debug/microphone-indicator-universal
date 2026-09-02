#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Constants
constexpr UINT TRAY_ICON_ID   = 1;
constexpr UINT_PTR TIMER_ID   = 101;
constexpr UINT_PTR TIMER_HUD_ID = 102;
constexpr int HOTKEY_ID       = 1;

// Custom window messages
constexpr UINT WM_USER_TRAY   = WM_USER + 1;
constexpr UINT WM_TRIGGER_HUD = WM_USER + 2;

// Tray menu command IDs
constexpr UINT_PTR MENU_TOGGLE    = 201;
constexpr UINT_PTR MENU_AUTOSTART = 203;
constexpr UINT_PTR MENU_EXIT      = 202;

// Global application state
static HICON g_hIconOnTray   = nullptr;
static HICON g_hIconMuteTray = nullptr;
static HICON g_hIconOnHud    = nullptr;
static HICON g_hIconMuteHud  = nullptr;

static bool g_currentMuteState = false;
static HWND g_hudHwnd          = nullptr;

// Obtain the default capture endpoint audio volume control interface
ComPtr<IAudioEndpointVolume> GetMicVolumeControl() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator)
    );
    if (FAILED(hr) || !enumerator) {
        return nullptr;
    }

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
    if (FAILED(hr) || !device) {
        return nullptr;
    }

    ComPtr<IAudioEndpointVolume> volume;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &volume);
    if (FAILED(hr)) {
        return nullptr;
    }

    return volume;
}

// Check current mute status of the default capture device
bool GetMicMute() {
    ComPtr<IAudioEndpointVolume> vol = GetMicVolumeControl();
    if (!vol) {
        return false;
    }

    BOOL isMuted = FALSE;
    if (SUCCEEDED(vol->GetMute(&isMuted))) {
        return isMuted != FALSE;
    }
    return false;
}

// Mute or unmute all capture endpoints
void SetMicMute(bool mute) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator)
    );
    if (FAILED(hr) || !enumerator) {
        return;
    }

    // 1. Mute/Unmute default console capture device
    ComPtr<IAudioEndpointVolume> defaultVol = GetMicVolumeControl();
    if (defaultVol) {
        defaultVol->SetMute(mute ? TRUE : FALSE, &GUID_NULL);
    }

    // 2. Mute/Unmute ALL active capture endpoints (microphones, headsets, virtual inputs)
    ComPtr<IMMDeviceCollection> collection;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection)) && collection) {
        UINT count = 0;
        if (SUCCEEDED(collection->GetCount(&count))) {
            for (UINT i = 0; i < count; ++i) {
                ComPtr<IMMDevice> device;
                if (SUCCEEDED(collection->Item(i, &device)) && device) {
                    ComPtr<IAudioEndpointVolume> volume;
                    if (SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &volume)) && volume) {
                        volume->SetMute(mute ? TRUE : FALSE, &GUID_NULL);
                    }
                }
            }
        }
    }
}

// Check if Autostart on Windows startup is enabled in HKCU registry
bool IsAutostartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD dataLen = 0;
        LONG res = RegQueryValueExW(
            hKey,
            L"MicrophoneIndicator",
            nullptr,
            &type,
            nullptr,
            &dataLen
        );
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

// Enable or disable autostart with Windows
void SetAutostart(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_WRITE,
        &hKey
    ) != ERROR_SUCCESS) {
        return;
    }

    if (enable) {
        wchar_t exePath[MAX_PATH] = { 0 };
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0) {
            DWORD sizeBytes = static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t));
            RegSetValueExW(
                hKey,
                L"MicrophoneIndicator",
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(exePath),
                sizeBytes
            );
        }
    } else {
        RegDeleteValueW(hKey, L"MicrophoneIndicator");
    }

    RegCloseKey(hKey);
}

// Update the system tray icon and tooltip, and optionally trigger visual HUD
void UpdateTrayIcon(HWND hwnd, bool triggerVisualHud) {
    bool isMuted = GetMicMute();
    g_currentMuteState = isMuted;

    HICON hIcon = isMuted ? g_hIconMuteTray : g_hIconOnTray;
    const wchar_t* tip = isMuted ? L"Microphone Muted" : L"Microphone On";

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_USER_TRAY;
    nid.hIcon = hIcon;
    wcsncpy_s(nid.szTip, tip, _TRUNCATE);

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (triggerVisualHud && g_hudHwnd && IsWindow(g_hudHwnd)) {
        SendMessageW(g_hudHwnd, WM_TRIGGER_HUD, 0, 0);
    }
}

// HUD window procedure (macOS-style transparent floating overlay)
LRESULT CALLBACK WndProcHud(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int animFrame = 0;

    switch (msg) {
        case WM_TRIGGER_HUD: {
            animFrame = 0;
            // Force window topmost
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            // Set initial alpha to 0 using COLORKEY (Magenta 0xFF00FF) and ALPHA
            SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY | LWA_ALPHA);
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            InvalidateRect(hwnd, nullptr, TRUE);
            SetTimer(hwnd, TIMER_HUD_ID, 15, nullptr);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == TIMER_HUD_ID) {
                animFrame++;
                int alpha = 0;
                if (animFrame <= 5) {
                    alpha = (animFrame * 200) / 5;
                } else if (animFrame <= 15) {
                    alpha = 200;
                } else if (animFrame <= 20) {
                    alpha = ((20 - animFrame) * 200) / 5;
                } else {
                    KillTimer(hwnd, TIMER_HUD_ID);
                    ShowWindow(hwnd, SW_HIDE);
                }
                SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), static_cast<BYTE>(alpha), LWA_COLORKEY | LWA_ALPHA);
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, 160, 160);
            HGDIOBJ oldBitmap = SelectObject(hdcMem, hBitmap);

            // 1. Clear background with Magenta (RGB 255, 0, 255) as colorkey
            HBRUSH keyBrush = CreateSolidBrush(RGB(255, 0, 255));
            RECT rect = { 0, 0, 160, 160 };
            FillRect(hdcMem, &rect, keyBrush);
            DeleteObject(keyBrush);

            // 2. Draw rounded dark gray background (macOS-like HUD styling: RGB 0x1F, 0x1F, 0x1F)
            HBRUSH bgBrush = CreateSolidBrush(RGB(0x1F, 0x1F, 0x1F));
            HGDIOBJ oldBrush = SelectObject(hdcMem, bgBrush);

            HPEN hPen = CreatePen(PS_NULL, 0, 0);
            HGDIOBJ oldPen = SelectObject(hdcMem, hPen);

            RoundRect(hdcMem, 0, 0, 160, 160, 24, 24);

            SelectObject(hdcMem, oldBrush);
            DeleteObject(bgBrush);
            SelectObject(hdcMem, oldPen);
            DeleteObject(hPen);

            // 3. Draw current microphone icon centered (96x96 at offset 32,32)
            bool isMuted = GetMicMute();
            HICON hIcon = isMuted ? g_hIconMuteHud : g_hIconOnHud;
            DrawIconEx(
                hdcMem,
                32,
                32,
                hIcon,
                96,
                96,
                0,
                nullptr,
                DI_NORMAL
            );

            BitBlt(hdc, 0, 0, 160, 160, hdcMem, 0, 0, SRCCOPY);

            SelectObject(hdcMem, oldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);

            EndPaint(hwnd, &ps);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Main message window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Register global hotkey: Ctrl + Alt + Space
            RegisterHotKey(hwnd, HOTKEY_ID, MOD_CONTROL | MOD_ALT, VK_SPACE);

            // Set 1-second polling timer
            SetTimer(hwnd, TIMER_ID, 1000, nullptr);

            // Add initial system tray icon
            bool isMuted = GetMicMute();
            g_currentMuteState = isMuted;
            HICON hIcon = isMuted ? g_hIconMuteTray : g_hIconOnTray;
            const wchar_t* tip = isMuted ? L"Microphone Muted" : L"Microphone On";

            NOTIFYICONDATAW nid = {};
            nid.cbSize = sizeof(NOTIFYICONDATAW);
            nid.hWnd = hwnd;
            nid.uID = TRAY_ICON_ID;
            nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
            nid.uCallbackMessage = WM_USER_TRAY;
            nid.hIcon = hIcon;
            wcsncpy_s(nid.szTip, tip, _TRUNCATE);

            Shell_NotifyIconW(NIM_ADD, &nid);
            return 0;
        }

        case WM_HOTKEY: {
            if (static_cast<int>(wParam) == HOTKEY_ID) {
                bool currentMute = GetMicMute();
                SetMicMute(!currentMute);
                UpdateTrayIcon(hwnd, true);
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == TIMER_ID) {
                bool actualMute = GetMicMute();
                if (actualMute != g_currentMuteState) {
                    UpdateTrayIcon(hwnd, true);
                }
            }
            return 0;
        }

        case WM_USER_TRAY: {
            UINT event = static_cast<UINT>(lParam);
            if (event == WM_RBUTTONUP || event == WM_LBUTTONDBLCLK) {
                HMENU hMenu = CreatePopupMenu();
                if (!hMenu) return 0;

                const wchar_t* toggleLabel = GetMicMute() ? L"Turn Microphone On" : L"Turn Microphone Off";
                AppendMenuW(hMenu, MF_STRING, MENU_TOGGLE, toggleLabel);

                UINT autostartFlag = IsAutostartEnabled() ? MF_CHECKED : MF_UNCHECKED;
                AppendMenuW(hMenu, MF_STRING | autostartFlag, MENU_AUTOSTART, L"Start with Windows");

                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, MENU_EXIT, L"Quit");

                POINT pt = {};
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);

                TrackPopupMenu(
                    hMenu,
                    TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                    pt.x,
                    pt.y,
                    0,
                    hwnd,
                    nullptr
                );

                DestroyMenu(hMenu);
            }
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case MENU_TOGGLE: {
                    bool currentMute = GetMicMute();
                    SetMicMute(!currentMute);
                    UpdateTrayIcon(hwnd, true);
                    break;
                }
                case MENU_AUTOSTART: {
                    bool current = IsAutostartEnabled();
                    SetAutostart(!current);
                    break;
                }
                case MENU_EXIT: {
                    DestroyWindow(hwnd);
                    break;
                }
                default:
                    break;
            }
            return 0;
        }

        case WM_DESTROY: {
            NOTIFYICONDATAW nid = {};
            nid.cbSize = sizeof(NOTIFYICONDATAW);
            nid.hWnd = hwnd;
            nid.uID = TRAY_ICON_ID;
            Shell_NotifyIconW(NIM_DELETE, &nid);

            UnregisterHotKey(hwnd, HOTKEY_ID);
            KillTimer(hwnd, TIMER_ID);

            if (g_hudHwnd && IsWindow(g_hudHwnd)) {
                DestroyWindow(g_hudHwnd);
            }

            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Application Entry Point
int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    // Initialize COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    // Load System Tray Icons (crisp 16x16)
    g_hIconOnTray   = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(2), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    g_hIconMuteTray = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(3), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    // Load HUD Icons (crisp 96x96)
    g_hIconOnHud    = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(2), IMAGE_ICON, 96, 96, LR_DEFAULTCOLOR));
    g_hIconMuteHud  = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(3), IMAGE_ICON, 96, 96, LR_DEFAULTCOLOR));

    // Register HUD Window Class
    const wchar_t* classNameHud = L"MicIndicatorHUDClass";
    WNDCLASSEXW wndClassHud = {};
    wndClassHud.cbSize        = sizeof(WNDCLASSEXW);
    wndClassHud.lpfnWndProc   = WndProcHud;
    wndClassHud.hInstance     = hInst;
    wndClassHud.lpszClassName = classNameHud;
    RegisterClassExW(&wndClassHud);

    // Center HUD on primary screen
    int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int hudWidth  = 160;
    int hudHeight = 160;
    int x = (screenWidth - hudWidth) / 2;
    int y = (screenHeight - hudHeight) / 2;

    // Create HUD Window (Layered, Topmost, Transparent to clicks, No Activate)
    g_hudHwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        classNameHud,
        L"MicIndicatorHUD",
        WS_POPUP,
        x, y, hudWidth, hudHeight,
        nullptr,
        nullptr,
        hInst,
        nullptr
    );

    // Register Main Tray Window Class
    const wchar_t* className = L"MicIndicatorClass";
    WNDCLASSEXW wndClass = {};
    wndClass.cbSize        = sizeof(WNDCLASSEXW);
    wndClass.lpfnWndProc   = WndProc;
    wndClass.hInstance     = hInst;
    wndClass.lpszClassName = className;
    RegisterClassExW(&wndClass);

    // Create Message-Only Window
    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"MicIndicator",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        hInst,
        nullptr
    );

    if (!hwnd) {
        CoUninitialize();
        return 1;
    }

    // Main Message Loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
